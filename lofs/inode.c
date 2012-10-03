/**
 * lofs: Linux filesystem loopback layer
 *
 * Copyright (C) 1997-2004 Erez Zadok
 * Copyright (C) 2001-2004 Stony Brook University
 * Copyright (C) 2004-2007 International Business Machines Corp.
 * Copyright (C) 2011-2012 Electric Cloud, Inc.
 *   Author(s): Eric Melski <ericm@electric-cloud.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include "lofs_kernel.h"

static struct dentry *lock_parent(struct dentry *dentry)
{
    struct dentry *dir;
    dir = dget_parent(dentry);
    LOCK_INODE(dir->d_inode);
    return dir;
}

static void unlock_dir(struct dentry *dir)
{
    UNLOCK_INODE(dir->d_inode);
    dput(dir);
}

/*----------------------------------------------------------------------------
 * lofs_inode_test
 *
 *      Determine whether or not a given lofs inode refers to the specified
 *      lower inode.  Returns 1 if so, 0 otherwise.
 *----------------------------------------------------------------------------
 */

int lofs_inode_test(struct inode *inode, void *candidate_lower_inode)
{
    if ((lofs_inode_to_lower(inode) == (struct inode *)candidate_lower_inode))
        return 1;
    return 0;
}

/*----------------------------------------------------------------------------
 * lofs_inode_set
 *
 *      Initialize a lofs inode and link it to a given lower inode.
 *----------------------------------------------------------------------------
 */

int lofs_inode_set(
    struct inode *inode,                /* The lofs inode. */
    void *opaque)                       /* Lower inode, must cast to use. */
{
    struct inode *lower_inode = (struct inode *) opaque;
    lofs_set_inode_lower(inode, lower_inode);
    FSSTACK_COPY_ATTR_ALL(inode, lower_inode);
    fsstack_copy_inode_size(inode, lower_inode);

    inode->i_ino = lower_inode->i_ino;
    inode->i_version++;
    inode->i_mapping->a_ops = (struct address_space_operations *) &lofs_aops;
#if defined(HAVE_BACKING_DEV)
    inode->i_mapping->backing_dev_info = inode->i_sb->s_bdi;
#endif

    if (S_ISLNK(lower_inode->i_mode)) {
        inode->i_op = (struct inode_operations *) &lofs_symlink_iops;
    } else if (S_ISDIR(lower_inode->i_mode)) {
        inode->i_op = (struct inode_operations *) &lofs_dir_iops;
    } else {
        inode->i_op = (struct inode_operations *) &lofs_main_iops;
    }

    if (S_ISDIR(lower_inode->i_mode)) {
        inode->i_fop = (struct file_operations *) &lofs_dir_fops;
    } else if (special_file(lower_inode->i_mode)) {
        init_special_inode(inode, lower_inode->i_mode, lower_inode->i_rdev);
    } else {
        inode->i_fop = &lofs_main_fops;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * __lofs_get_inode
 *
 *      Helper for lofs_get_inode, handles the common error checking cases.
 *----------------------------------------------------------------------------
 */

static struct inode *__lofs_get_inode(
    struct inode *lower_inode,          /* Lower inode to associate with. */
    struct super_block *sb)             /* lofs super block. */
{
    struct inode *inode;
    if (!igrab(lower_inode)) {
        /* The lower inode is about to be freed, so bail out. */

        return ERR_PTR(-ESTALE);
    }

    /* Allocate a new inode for lofs.  It will be initialized in
     * lofs_inode_set.  Also, if there is already an inode for this lofs and
     * lower inode pair, lofs_inode_test will tell us so and we will reuse
     * it.
     */

    inode = iget5_locked(sb, (unsigned long) lower_inode,
            lofs_inode_test,
            lofs_inode_set,
            lower_inode);
    if (!inode) {
        iput(lower_inode);
        return ERR_PTR(-EACCES);
    }
    if (!(inode->i_state & I_NEW)) {
        iput(lower_inode);
    }
    return inode;
}

/*----------------------------------------------------------------------------
 * lofs_get_inode
 *
 *      Get a lofs inode for the given lower inode and lofs super_block pair.
 *----------------------------------------------------------------------------
 */

struct inode *lofs_get_inode(
    struct inode *lower_inode,          /* The lower inode. */
    struct super_block *sb)             /* lofs super_block. */
{
    struct inode *inode = __lofs_get_inode(lower_inode, sb);
    if (!IS_ERR(inode) && (inode->i_state & I_NEW)) {
        unlock_new_inode(inode);
    }
    return inode;
}

/*----------------------------------------------------------------------------
 * lofs_interpose
 *
 *      Simple wrapper to handle error checking and the d_instantiate call
 *      common to interposition in many cases.
 *----------------------------------------------------------------------------
 */

static int lofs_interpose(
    struct dentry *lower_dentry,        /* dentry from the lower filesystem. */
    struct dentry *dentry,              /* lofs dentry. */
    struct super_block *sb)             /* lofs super_block. */
{
    struct inode *inode = lofs_get_inode(lower_dentry->d_inode, sb);
    if (IS_ERR(inode)) {
        return PTR_ERR(inode);
    }
    d_instantiate(dentry, inode);
    return 0;
}

/**
 * lofs_create
 * @dir:        The inode of the directory in which to create the file.
 * @dentry:     The lofs dentry
 * @mode:       The mode of the new file.
 * @nd:         nameidata
 *
 * Creates a new file.
 *
 * Returns zero on success; non-zero on error condition
 */
static int
lofs_create(struct inode *directory_inode, struct dentry *dentry,
                CREATE_MODE_TYPE mode, struct nameidata *nd)
{
    int rc;
    struct dentry *lower_dir_dentry;
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    struct vfsmount *lower_mnt  = lofs_dentry_to_lower_mnt(dentry);
    LOFS_ND_DECLARATIONS;

    lower_dir_dentry = lock_parent(lower_dentry);
    if (IS_ERR(lower_dir_dentry)) {
        lofs_printk(KERN_ERR, "Error locking directory of dentry\n");
        rc = PTR_ERR(lower_dir_dentry);
        goto out;
    }

    LOFS_ND_SAVE_ARGS(nd, lower_dentry, lower_mnt);
    rc = vfs_create(lower_dir_dentry->d_inode, lower_dentry, mode, nd);
    LOFS_ND_RESTORE_ARGS(nd);
    if (rc) {
        goto out_lock;
    }

    rc = lofs_interpose(lower_dentry, dentry, directory_inode->i_sb);
    if (rc) {
        goto out_lock;
    }

    fsstack_copy_attr_times(directory_inode, lower_dir_dentry->d_inode);
    fsstack_copy_inode_size(directory_inode, lower_dir_dentry->d_inode);
out_lock:
    unlock_dir(lower_dir_dentry);
out:
    return rc;
}

/*----------------------------------------------------------------------------
 * lofs_follow_down
 *
 *      Given a vfsmount/dentry pair, check if a filesystem is mounted and
 *      traverse into that filesystem if so.  Handles the nuances around
 *      automounted filesystems on new versions of Linux that have "managed"
 *      dentries.
 *
 *      After this function, the provided vfsmount/dentry pointers will be
 *      updated with the values for the mounted filesystem, if traversal
 *      occurred.
 *----------------------------------------------------------------------------
 */

static int lofs_follow_down(
    struct vfsmount **lower_mnt,        /* Lower filesystem.  Will be replaced
                                         * by the mounted filesystem, if the
                                         * dentry is covered. */
    struct dentry **lower_dentry)       /* Dentry in the lower filesytem. Will
                                         * be replaced by the root of the
                                         * mounted filesystem, if the dentry
                                         * is covered. */
{
    int rc = 0;
    struct path path;
    path.mnt    = *lower_mnt;
    path.dentry = *lower_dentry;
    while (d_mountpoint(path.dentry)) {
        if (FOLLOW_DOWN(&path) != 1) {
            break;
        }
    }

#ifdef HAVE_MANAGED_DENTRIES
    if (LOFS_MANAGED_DENTRY(path.dentry)) {
        /* Lower dentry is managed -- probably means it's automounted.  We can
         * provoke the automount by doing a full lookup of the path -- it's not
         * enough to just do a lookup of the single dentry, because we need to
         * hit the code paths in the kernel that handle automounting.
         * Specifically, if follow_managed() were exported, we could use that
         * instead of follow_down() above, but it's not, so we have to do it
         * this way.
         */

        struct path old = path;
        char *buffer = kmalloc(2048, GFP_KERNEL);
        char *full = D_PATH(&path, buffer, 2048);
        int len = strlen(full);
        if (strcmp(full + len - 10, " (deleted)") == 0) {
            full[len - 10] = 0;
        }
        rc = lofs_lookup_managed(full, &path);
        kfree(buffer);

        if (rc == 0) {
            /* Make sure to release the original dentry / vfsmount, if the
             * lookup succeeded.  They will have been replaced, possibly, by
             * the activity in lofs_lookup_managed.
             */

            dput(old.dentry);
            mntput(old.mnt);

            /* Now we can redo the follow_down() and reasonably expect it to
             * find the automounted filesystem, if any.
             */

            FOLLOW_DOWN(&path);
        }
    }
#endif

    *lower_dentry = path.dentry;
    *lower_mnt    = path.mnt;
    return rc;
}

/**
 * lofs_lookup_and_interpose_lower - Perform a lookup
 */
static int lofs_lookup_and_interpose_lower(struct dentry *lofs_dentry,
                                        struct dentry *lower_dentry,
                                        struct inode *lofs_dir_inode)
{
    struct dentry *lower_dir_dentry;
    struct vfsmount *lower_mnt;
    struct inode *lower_inode, *inode;
    int rc = 0;

    lower_mnt = mntget(lofs_dentry_to_lower_mnt(lofs_dentry->d_parent));

    /* If the lower_dentry has a filesystem mounted over it, follow the
     * mountpoint.  This is really the entire secret sauce of lofs here.
     */

    rc = lofs_follow_down(&lower_mnt, &lower_dentry);
    if (rc != 0) {
        goto out_dput;
    }

    if (lower_dentry->d_op == &lofs_dops) {
        /* If we wind up with a lower_dentry that is itself in lofs, bail
         * out, to avoid sending recursive tree walks into infinite loops.
         */

        rc = -EINVAL;
        goto out_dput;
    }

    lower_dir_dentry = lower_dentry->d_parent;
    lower_inode = lower_dentry->d_inode;
    fsstack_copy_attr_atime(lofs_dir_inode, lower_dir_dentry->d_inode);
    lofs_set_dentry_private(lofs_dentry,
            kmem_cache_alloc(lofs_dentry_info_cache, GFP_KERNEL));
    if (!lofs_dentry_to_private(lofs_dentry)) {
        rc = -ENOMEM;
        printk(KERN_ERR "%s: Out of memory whilst attempting "
                "to allocate lofs_dentry_info struct\n",
                __func__);
        rc = -ENOMEM;
        goto out_dput;
    }
    lofs_set_dentry_lower(lofs_dentry, lower_dentry);
    lofs_set_dentry_lower_mnt(lofs_dentry, lower_mnt);
    if (!lower_dentry->d_inode) {
        /* We want to add because we couldn't find in lower */

        d_add(lofs_dentry, NULL);
        return 0;
    }
    inode = __lofs_get_inode(lower_inode, lofs_dir_inode->i_sb);
    if (IS_ERR(inode)) {
        return PTR_ERR(inode);
    }
    if (inode->i_state & I_NEW) {
        unlock_new_inode(inode);
    }
    d_add(lofs_dentry, inode);
    return rc;
        
out_dput:
    dput(lower_dentry);
    mntput(lower_mnt);
    d_drop(lofs_dentry);
    return rc;
}

/**
 * lofs_lookup
 * @lofs_dir_inode:     The lofs directory inode
 * @lofs_dentry:        The lofs dentry that we are looking up
 * @lofs_nd:            nameidata; may be NULL
 *
 * Find a file on disk. If the file does not exist, then we'll add it to the
 * dentry cache and continue on to read it from the disk.
 */
static struct dentry *lofs_lookup(struct inode *lofs_dir_inode,
                                      struct dentry *lofs_dentry,
                                      struct nameidata *lofs_nd)
{
        struct dentry *lower_dir_dentry, *lower_dentry;
        struct vfsmount *lower_mnt;
        int lower_valid, rc = 0;

#ifndef HAVE_S_D_OP
        lofs_dentry->d_op = (struct dentry_operations *) &lofs_dops;
#endif
        if ((lofs_dentry->d_name.len == 1
             && !strcmp(lofs_dentry->d_name.name, "."))
            || (lofs_dentry->d_name.len == 2
                && !strcmp(lofs_dentry->d_name.name, ".."))) {
                goto out_d_drop;
        }

        lower_dir_dentry = lofs_dentry_to_lower(lofs_dentry->d_parent);

        LOCK_INODE(lower_dir_dentry->d_inode);

        lower_dentry = d_lookup(lower_dir_dentry, &lofs_dentry->d_name);
        if (lower_dentry) {

            UNLOCK_INODE(lower_dir_dentry->d_inode);

            /* lower_dentry is in lofs_dentry_to_lower(lofs_dentry->d_parent),
             * so we must use lofs_dentry_to_lower_mnt(lofs_dentry->d_parent)
             * when revalidating it.  Only later do we follow mounts.
             */

            lower_mnt = lofs_dentry_to_lower_mnt(lofs_dentry->d_parent);
            lower_valid = lofs_revalidate_lower(
                lower_dentry, lower_mnt, lofs_nd);

            if (unlikely(lower_valid <= 0)) {
                if (unlikely(lower_valid < 0)) {
                    /* Lower filesystem reported an error. */

                dput(lower_dentry);
                    rc = lower_valid;
                    goto out_d_drop;
                }

                /* Lower filesystem says the cached dentry is invalid.  Try
                 * to invalidate it, but if that fails, then imitate the kernel
                 * behavior (at least for RHEL 4 and 5) of using the dentry
                 * that we failed to invalidate as if it were valid.  At least
                 * for those versions, going on to lookup_one_len would end up
                 * fetching that same dentry, just at a higher CPU cost.  The
                 * remaining option that we might consider is to error out.
                 */
                if (! d_invalidate(lower_dentry)) {
                    dput(lower_dentry);
                lower_dentry = 0;

                LOCK_INODE(lower_dir_dentry->d_inode);
            }
        }
        }

        /* At this point we hold the directory inode lock
         * if and only if lower_dentry is NULL. */

        if (!lower_dentry) {
            lower_dentry = lookup_one_len(lofs_dentry->d_name.name,
                    lower_dir_dentry,
                    lofs_dentry->d_name.len);

            UNLOCK_INODE(lower_dir_dentry->d_inode);

            if (IS_ERR(lower_dentry)) {
                rc = PTR_ERR(lower_dentry);
                goto out_d_drop;
            }
        }

        rc = lofs_lookup_and_interpose_lower(lofs_dentry, lower_dentry,
                lofs_dir_inode);
        goto out;
out_d_drop:
        d_drop(lofs_dentry);
out:
        return ERR_PTR(rc);
}

static int lofs_link(struct dentry *old_dentry, struct inode *dir,
                         struct dentry *new_dentry)
{
        struct dentry *lower_old_dentry;
        struct dentry *lower_new_dentry;
        struct dentry *lower_dir_dentry;
        u64 file_size_save;
        int rc;

        file_size_save = i_size_read(old_dentry->d_inode);
        lower_old_dentry = lofs_dentry_to_lower(old_dentry);
        lower_new_dentry = lofs_dentry_to_lower(new_dentry);
        dget(lower_old_dentry);
        dget(lower_new_dentry);
        lower_dir_dentry = lock_parent(lower_new_dentry);
        rc = VFS_LINK(lower_old_dentry, lower_dir_dentry->d_inode,
                      lower_new_dentry);
        if (rc || !lower_new_dentry->d_inode)
                goto out_lock;
        rc = lofs_interpose(lower_new_dentry, new_dentry, dir->i_sb);
        if (rc)
                goto out_lock;
        fsstack_copy_attr_times(dir, lower_new_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_new_dentry->d_inode);
        SET_NLINK(old_dentry->d_inode,
                lofs_inode_to_lower(old_dentry->d_inode)->i_nlink);
        i_size_write(new_dentry->d_inode, file_size_save);
out_lock:
        unlock_dir(lower_dir_dentry);
        dput(lower_new_dentry);
        dput(lower_old_dentry);
        return rc;
}

static int lofs_unlink(struct inode *dir, struct dentry *dentry)
{
        int rc = 0;
        struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
        struct inode *lower_dir_inode = lofs_inode_to_lower(dir);
        struct dentry *lower_dir_dentry;

        dget(lower_dentry);
        lower_dir_dentry = lock_parent(lower_dentry);
        rc = VFS_UNLINK(lower_dir_inode, lower_dentry);
        if (rc) {
            goto out_unlock;
        }
        fsstack_copy_attr_times(dir, lower_dir_inode);
        SET_NLINK(dentry->d_inode, 
                lofs_inode_to_lower(dentry->d_inode)->i_nlink);
        dentry->d_inode->i_ctime = dir->i_ctime;
out_unlock:
        unlock_dir(lower_dir_dentry);
        dput(lower_dentry);
        return rc;
}

static int lofs_symlink(struct inode *dir, struct dentry *dentry,
                            const char *symname)
{
        int rc;
        struct dentry *lower_dentry;
        struct dentry *lower_dir_dentry;

        lower_dentry = lofs_dentry_to_lower(dentry);
        dget(lower_dentry);
        lower_dir_dentry = lock_parent(lower_dentry);
        rc = VFS_SYMLINK(lower_dir_dentry->d_inode, lower_dentry, symname, S_IALLUGO);
        if (rc || !lower_dentry->d_inode)
                goto out_lock;
        rc = lofs_interpose(lower_dentry, dentry, dir->i_sb);
        if (rc)
                goto out_lock;
        fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
out_lock:
        unlock_dir(lower_dir_dentry);
        dput(lower_dentry);
        if (!dentry->d_inode)
                d_drop(dentry);
        return rc;
}

static int lofs_mkdir(
    struct inode *dir, 
    struct dentry *dentry, 
    CREATE_MODE_TYPE mode)
{
        int rc;
        struct dentry *lower_dentry;
        struct dentry *lower_dir_dentry;

        lower_dentry = lofs_dentry_to_lower(dentry);
        lower_dir_dentry = lock_parent(lower_dentry);
        rc = VFS_MKDIR(lower_dir_dentry->d_inode, lower_dentry, mode);
        if (rc || !lower_dentry->d_inode)
                goto out;
        rc = lofs_interpose(lower_dentry, dentry, dir->i_sb);
        if (rc)
                goto out;
        fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
        SET_NLINK(dir, lower_dir_dentry->d_inode->i_nlink);
out:
        unlock_dir(lower_dir_dentry);
        if (!dentry->d_inode)
                d_drop(dentry);
        return rc;
}

static int lofs_rmdir(struct inode *dir, struct dentry *dentry)
{
        struct dentry *lower_dentry;
        struct dentry *lower_dir_dentry;
        int rc;

        lower_dentry = lofs_dentry_to_lower(dentry);
        dget(dentry);
        lower_dir_dentry = lock_parent(lower_dentry);
        dget(lower_dentry);
        rc = VFS_RMDIR(lower_dir_dentry->d_inode, lower_dentry);
        dput(lower_dentry);
        fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
        SET_NLINK(dir, lower_dir_dentry->d_inode->i_nlink);
        unlock_dir(lower_dir_dentry);
        dput(dentry);
        return rc;
}

static int
lofs_mknod(
    struct inode *dir, 
    struct dentry *dentry, 
    CREATE_MODE_TYPE mode, 
    dev_t dev)
{
        int rc;
        struct dentry *lower_dentry;
        struct dentry *lower_dir_dentry;

        lower_dentry = lofs_dentry_to_lower(dentry);
        lower_dir_dentry = lock_parent(lower_dentry);
        rc = VFS_MKNOD(lower_dir_dentry->d_inode, lower_dentry, mode, dev);
        if (rc || !lower_dentry->d_inode)
                goto out;
        rc = lofs_interpose(lower_dentry, dentry, dir->i_sb);
        if (rc)
                goto out;
        fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
out:
        unlock_dir(lower_dir_dentry);
        if (!dentry->d_inode)
                d_drop(dentry);
        return rc;
}

static int
lofs_rename(struct inode *old_dir, struct dentry *old_dentry,
                struct inode *new_dir, struct dentry *new_dentry)
{
        int rc;
        struct dentry *lower_old_dentry;
        struct dentry *lower_new_dentry;
        struct dentry *lower_old_dir_dentry;
        struct dentry *lower_new_dir_dentry;
        struct vfsmount *lower_old_mnt = lofs_dentry_to_lower_mnt(old_dentry);
        struct vfsmount *lower_new_mnt = lofs_dentry_to_lower_mnt(new_dentry);

        if (lower_old_mnt != lower_new_mnt) {
            /* Cannot rename across devices. */

            return -EXDEV;
        }

        lower_old_dentry = lofs_dentry_to_lower(old_dentry);
        lower_new_dentry = lofs_dentry_to_lower(new_dentry);
        dget(lower_old_dentry);
        dget(lower_new_dentry);
        lower_old_dir_dentry = dget_parent(lower_old_dentry);
        lower_new_dir_dentry = dget_parent(lower_new_dentry);
        lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
        rc = VFS_RENAME(lower_old_dir_dentry->d_inode, lower_old_dentry,
                        lower_new_dir_dentry->d_inode, lower_new_dentry);
        if (rc) {
            goto out_lock;
        }
        FSSTACK_COPY_ATTR_ALL(new_dir, lower_new_dir_dentry->d_inode);
        if (new_dir != old_dir) {
            FSSTACK_COPY_ATTR_ALL(old_dir, lower_old_dir_dentry->d_inode);
        }
out_lock:
        unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
        dput(lower_new_dentry->d_parent);
        dput(lower_old_dentry->d_parent);
        dput(lower_new_dentry);
        dput(lower_old_dentry);
        return rc;
}

static int
lofs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
        struct dentry *lower_dentry;
        int rc;

        lower_dentry = lofs_dentry_to_lower(dentry);
        if (!lower_dentry->d_inode->i_op->readlink) {
                rc = -EINVAL;
                goto out;
        }
        rc = lower_dentry->d_inode->i_op->readlink(lower_dentry, buf, bufsiz);
        if (rc >= 0) {
            fsstack_copy_attr_atime(dentry->d_inode, lower_dentry->d_inode);
        }
out:
        return rc;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#define FOLLOW_LINK_RETURN_TYPE         int
#define FOLLOW_LINK_RETURN_VALUE(x)     (x)
#define PUT_LINK_ARGS(x, y, z)          x, y
#else
#define FOLLOW_LINK_RETURN_TYPE         void *
#define FOLLOW_LINK_RETURN_VALUE(x)     ERR_PTR(x)
#define PUT_LINK_ARGS(x, y, z)          x, y, z
#endif

static FOLLOW_LINK_RETURN_TYPE lofs_follow_link(
    struct dentry *dentry,
    struct nameidata *nd)
{
        char *buf;
        int len = PAGE_SIZE, rc;
        mm_segment_t old_fs;

        /* Released in lofs_put_link(); only release here on error */
        buf = kmalloc(len, GFP_KERNEL);
        if (!buf) {
                rc = -ENOMEM;
                goto out;
        }
        old_fs = get_fs();
        set_fs(get_ds());
        rc = dentry->d_inode->i_op->readlink(dentry, (char __user *)buf, len);
        set_fs(old_fs);
        if (rc < 0)
                goto out_free;
        else
                buf[rc] = '\0';
        rc = 0;
        nd_set_link(nd, buf);
        goto out;
out_free:
        kfree(buf);
out:
        return FOLLOW_LINK_RETURN_VALUE(rc);
}

static void
lofs_put_link(PUT_LINK_ARGS(struct dentry *d, struct nameidata *nd, void *p))
{
        /* Free the char* */
        kfree(nd_get_link(nd));
}

#ifdef HAVE_NAMEIDATA_IN_PERMISSION
static int lofs_permission(struct inode *inode, int mask, struct nameidata *nd)
{
    int err;
    if (nd) {
        struct dentry *lower_dentry;
        struct vfsmount *lower_mount;
        LOFS_ND_DECLARATIONS;
        lower_dentry = lofs_dentry_to_lower(NAMEIDATA_TO_DENTRY(nd));
        lower_mount  = lofs_dentry_to_lower_mnt(NAMEIDATA_TO_DENTRY(nd));
        LOFS_ND_SAVE_ARGS(nd, lower_dentry, lower_mount);
        err = permission(lofs_inode_to_lower(inode), mask, nd);
        LOFS_ND_RESTORE_ARGS(nd);
    } else {
        err = permission(lofs_inode_to_lower(inode), mask, nd);
    }        
    return err;
}
#else
#ifdef HAVE_FLAGS_IN_PERMISSION
static int lofs_permission(struct inode *inode, int mask, unsigned int flags)
#else
static int lofs_permission(struct inode *inode, int mask)
#endif
{
    return inode_permission(lofs_inode_to_lower(inode), mask);
}
#endif

/**
 * lofs_setattr
 * @dentry: dentry handle to the inode to modify
 * @ia: Structure with flags of what to change and values
 *
 * Updates the metadata of an inode by passing through to the lower
 * filesystem.  We just update our inode to look like the lower, except
 * for size changes, for which we need to make sure the page cache is updated
 * to reflect the new size.
 */
static int lofs_setattr(struct dentry *dentry, struct iattr *ia)
{
        int rc = 0;
        struct dentry *lower_dentry;
        struct inode *inode;
        struct inode *lower_inode;
        struct file *lofs_file = ia->ia_file;

        inode = dentry->d_inode;
        lower_inode = lofs_inode_to_lower(inode);
        lower_dentry = lofs_dentry_to_lower(dentry);

        /* If ia_valid contains ATTR_KILL_SUID or ATTR_KILL_SGID then the
         * mode change is for clearing setuid/setgid bits. Allow lower fs
         * to interpret this in its own way.
         */

        if (ia->ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID)) {
            ia->ia_valid &= ~ATTR_MODE;
        }

        /* If ATTR_FILE is set, make sure the file pointer points to the lower
         * file, not the lofs file.
         */

        if (ia->ia_valid & ATTR_FILE) {
            ia->ia_file = lofs_file_to_lower(lofs_file);
        }

        LOCK_INODE(lower_dentry->d_inode);
        rc = NOTIFY_CHANGE(lower_dentry, ia, lofs_dentry_to_lower_mnt(dentry));
        UNLOCK_INODE(lower_dentry->d_inode);

        if (rc == 0 
                && (ia->ia_valid & ATTR_SIZE)
                && (ia->ia_size != i_size_read(inode))) {
            /* Truncate the pages associated with the lofs inode, if the
             * lower file was successfully truncated.
             */

            rc = vmtruncate(inode, ia->ia_size);
        }

        /* Reset the file pointer. */

        ia->ia_file = lofs_file;

        /* Lower inode has updated attributes, copy them to the lofs inode. */

        FSSTACK_COPY_ATTR_ALL(inode, lower_inode);
        fsstack_copy_inode_size(inode, lower_inode);

        return rc;
}

static int
lofs_getattr(
    struct vfsmount *mnt, 
    struct dentry *lofs_dentry, 
    struct kstat *stat)
{
    generic_fillattr(lofs_dentry_to_lower(lofs_dentry)->d_inode, stat);
    stat->dev = lofs_dentry_to_lower(lofs_dentry)->d_sb->s_dev;
    return 0;
}

static int
lofs_setxattr(struct dentry *dentry, const char *name, const void *value,
                  size_t size, int flags)
{
    int rc = -ENOSYS;
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    if (lower_dentry->d_inode->i_op->setxattr) {
        LOCK_INODE(lower_dentry->d_inode);
        rc = lower_dentry->d_inode->i_op->setxattr(lower_dentry, name, 
                                                   value, size, flags);
        UNLOCK_INODE(lower_dentry->d_inode);
    }
    return rc;
}

static ssize_t
lofs_getxattr(struct dentry *dentry, const char *name, void *val, size_t size)
{
    int rc = -ENOSYS;
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    if (lower_dentry->d_inode->i_op->getxattr) {
        LOCK_INODE(lower_dentry->d_inode);
        rc = lower_dentry->d_inode->i_op->getxattr(lower_dentry,name,val,size);
        UNLOCK_INODE(lower_dentry->d_inode);
    }
    return rc;
}

static ssize_t
lofs_listxattr(struct dentry *dentry, char *list, size_t size)
{
    int rc = -ENOSYS;
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    if (lower_dentry->d_inode->i_op->listxattr) {
        LOCK_INODE(lower_dentry->d_inode);
        rc = lower_dentry->d_inode->i_op->listxattr(lower_dentry, list, size);
        UNLOCK_INODE(lower_dentry->d_inode);
    }
    return rc;
}

static int lofs_removexattr(struct dentry *dentry, const char *name)
{
    int rc = -ENOSYS;
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    if (lower_dentry->d_inode->i_op->removexattr) {
        LOCK_INODE(lower_dentry->d_inode);
        rc = lower_dentry->d_inode->i_op->removexattr(lower_dentry, name);
        UNLOCK_INODE(lower_dentry->d_inode);
    }
    return rc;
}

const struct inode_operations lofs_symlink_iops = {
        .readlink = lofs_readlink,
        .follow_link = lofs_follow_link,
        .put_link = lofs_put_link,
        .permission = lofs_permission,
        .setattr = lofs_setattr,
        .getattr = lofs_getattr,
        .setxattr = lofs_setxattr,
        .getxattr = lofs_getxattr,
        .listxattr = lofs_listxattr,
        .removexattr = lofs_removexattr
};

const struct inode_operations lofs_dir_iops = {
        .create = lofs_create,
        .lookup = lofs_lookup,
        .link = lofs_link,
        .unlink = lofs_unlink,
        .symlink = lofs_symlink,
        .mkdir = lofs_mkdir,
        .rmdir = lofs_rmdir,
        .mknod = lofs_mknod,
        .rename = lofs_rename,
        .permission = lofs_permission,
        .setattr = lofs_setattr,
        .getattr = lofs_getattr,
        .setxattr = lofs_setxattr,
        .getxattr = lofs_getxattr,
        .listxattr = lofs_listxattr,
        .removexattr = lofs_removexattr
};

const struct inode_operations lofs_main_iops = {
        .permission = lofs_permission,
        .setattr = lofs_setattr,
        .getattr = lofs_getattr,
        .setxattr = lofs_setxattr,
        .getxattr = lofs_getxattr,
        .listxattr = lofs_listxattr,
        .removexattr = lofs_removexattr
};
