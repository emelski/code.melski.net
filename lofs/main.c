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

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/parser.h>
#include <linux/slab.h>
#if defined(HAVE_MOUNT_IN_FS_TYPE)
#include <linux/backing-dev.h>
#endif
#include "lofs_kernel.h"

/**
 * Module parameter that defines the lofs_verbosity level.
 */
int lofs_verbosity = 0;

module_param(lofs_verbosity, int, 0);
MODULE_PARM_DESC(lofs_verbosity,
                 "Initial verbosity level (0 or 1; defaults to "
                 "0, which is Quiet)");

void __lofs_printk(const char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        if (fmt[1] == '7') { /* KERN_DEBUG */
                if (lofs_verbosity >= 1)
                        vprintk(fmt, args);
        } else
                vprintk(fmt, args);
        va_end(args);
}

#ifdef HAVE_CRED_IN_DENTRY_OPEN
#include <linux/security.h>
#define DENTRY_OPEN(d, m, f)    dentry_open((d), (m), (f), current_cred())
#else
#define DENTRY_OPEN(d, m, f)    dentry_open((d), (m), (f))
#endif

/**
 * lofs_privileged_open
 * @lower_file: Result of dentry_open by root on lower dentry
 * @lower_dentry: Lower dentry for file to open
 * @lower_mnt: Lower vfsmount for file to open
 *
 * This function gets a file opened againt the lower dentry.
 *
 * Returns zero on success; non-zero otherwise
 */

static int lofs_privileged_open(
    struct file **lower_file,
    struct dentry *lower_dentry,
    struct vfsmount *lower_mnt,
    mode_t mode)
{
    int flags = O_LARGEFILE | O_RDONLY;
    int rc = 0;

    /* Corresponding dput() and mntput() are done when the persistent file is
     * fput() when the lofs inode is destroyed.
     */

    dget(lower_dentry);
    mntget(lower_mnt);

    if (mode & FMODE_WRITE) {
        flags = O_LARGEFILE | O_RDWR;
    }

    (*lower_file) = DENTRY_OPEN(lower_dentry, lower_mnt, flags);
    if (IS_ERR(*lower_file)) {
        /* We should NOT mntput(lower_mnt) or dput(lower_dentry) in this
         * error case because dentry_open() already did that (EC-9392).
         */
        rc = PTR_ERR(*lower_file);
    }
    return rc;
}

/**
 * lofs_init_persistent_file
 * @lofs_dentry: Fully initialized lofs dentry object, with
 *                   the lower dentry and the lower mount set
 *
 * lofs only ever keeps a single open file for every lower
 * inode. All I/O operations to the lower inode occur through that
 * file.  When the first lofs file is *opened*, this function creates the
 * persistent file struct and associates it with the lofs inode.  When the
 * lofs inode is destroyed, the lower file is closed.
 *
 * The persistent file is opened with as little access privilege as
 * possible -- if the caller only needs read access, that's all you get.
 * If a later user needs write access, the read handle is closed and
 * replaced with a read/write handle.
 *
 * NOTE:  The read/write handle will never be demoted to a read-only handle!
 * This could conceivably cause a problem if somebody does something like,
 * for example, rewriting an executable file via lofs, then trying to run
 * that program (since we will have an open writable handle to it).  To
 * fix that we would need to have some more sophisticated reference counting
 * on the handle, and we would have to watch out for situations where the
 * lofs file is closed, but the lower file is still needed (eg, if somebody
 * opens a file, mmaps it, closes the file, then writes to the mapping).
 *
 * This function does nothing if a lower persistent file is already
 * associated with the lofs inode and has sufficient access permission.
 *
 * Returns zero on success; non-zero otherwise
 */
int lofs_init_persistent_file(struct dentry *lofs_dentry, mode_t mode)
{
    struct lofs_inode_info *inode_info =
            lofs_inode_to_private(lofs_dentry->d_inode);
    int rc = 0;

    mutex_lock(&inode_info->lower_file_mutex);
    if (!inode_info->lower_file 
            || (inode_info->lower_file->f_mode & mode) != mode) {
        struct file *tmp = 0;
        struct dentry *lower_dentry;
        struct vfsmount *lower_mnt = lofs_dentry_to_lower_mnt(lofs_dentry);

        lower_dentry = lofs_dentry_to_lower(lofs_dentry);
        rc = lofs_privileged_open(&tmp, lower_dentry, lower_mnt, mode);
        if (rc) {
            printk(KERN_ERR "Error opening lower persistent file "
                    "for lower_dentry [0x%p] and lower_mnt [0x%p]; "
                    "rc = [%d]\n", lower_dentry, lower_mnt, rc);
        } else {
            if (inode_info->lower_file) {
                // Had a persistent file but in the wrong mode, so close it.
                // Do this _after_ opening the file with the correct mode, and
                // only if that open works!  That way we don't pull the rug out
                // from underneath existing users of the file that don't need
                // the new permissions.

                fput(inode_info->lower_file);
                inode_info->lower_file = 0;
            }
            inode_info->lower_file = tmp;
        }
    }
    mutex_unlock(&inode_info->lower_file_mutex);
    return rc;
}

KMEM_CACHE_T *lofs_sb_info_cache;

#if defined(HAVE_MOUNT_IN_FS_TYPE)
static struct dentry *lofs_mount(
    struct file_system_type *fs_type,
    int flags,
    const char *dev_name,
    void *raw_data)
{
    static const struct qstr slash = { .name = "/", .len = 1 };
    struct super_block *s;
    struct lofs_sb_info *sbi;
    struct lofs_dentry_info *root_info;
    struct inode *inode;
    const char *err = "Getting sb failed";
    struct path path;
    int rc;

    sbi = kmem_cache_zalloc(lofs_sb_info_cache, GFP_KERNEL);
    if (!sbi) {
        rc = -ENOMEM;
        goto out;
    }

    s = sget(fs_type, NULL, set_anon_super, NULL);
    if (IS_ERR(s)) {
        rc = PTR_ERR(s);
        goto out;
    }

    s->s_flags = flags;
#if defined(HAVE_BACKING_DEV)
    rc = bdi_setup_and_register(&sbi->bdi, "lofs", BDI_CAP_MAP_COPY);
    if (rc)
        goto out1;

    s->s_bdi = &sbi->bdi;
#endif
    lofs_set_superblock_private(s, sbi);

    /* ->kill_sb() will take care of sbi after that point */
    sbi = NULL;
    s->s_op   = &lofs_sops;
    s->s_d_op = &lofs_dops;

    err = "Reading sb failed";
    rc = kern_path(slash.name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &path);
    if (rc) {
        lofs_printk(KERN_WARNING, "kern_path() failed\n");
        goto out1;
    }

    lofs_set_superblock_lower(s, path.dentry->d_sb);
    s->s_maxbytes = path.dentry->d_sb->s_maxbytes;
    s->s_blocksize = path.dentry->d_sb->s_blocksize;
    s->s_magic = 0x10f5;

    inode = lofs_get_inode(path.dentry->d_inode, s);
    rc = PTR_ERR(inode);
    if (IS_ERR(inode)) {
        goto out_free;
    }

#ifdef HAVE_D_MAKE_ROOT
    s->s_root = d_make_root(inode);
#else
    s->s_root = d_alloc_root(inode);
    if (!s->s_root) {
        iput(inode);
    }
#endif
    if (!s->s_root) {
        rc = -ENOMEM;
        goto out_free;
    }

    rc = -ENOMEM;
    root_info = kmem_cache_zalloc(lofs_dentry_info_cache, GFP_KERNEL);
    if (!root_info)
        goto out_free;

    /* ->kill_sb() will take care of root_info */
    lofs_set_dentry_private(s->s_root, root_info);
    lofs_set_dentry_lower(s->s_root, path.dentry);
    lofs_set_dentry_lower_mnt(s->s_root, path.mnt);

    s->s_flags |= MS_ACTIVE;
    return dget(s->s_root);

out_free:
    path_put(&path);
out1:
    deactivate_locked_super(s);
out:
    if (sbi) {
        kmem_cache_free(lofs_sb_info_cache, sbi);
    }
    printk(KERN_ERR "%s; rc = [%d]\n", err, rc);
    return ERR_PTR(rc);
}

/**
 * lofs_kill_block_super
 * @sb: The lofs super block
 *
 * Used to bring the superblock down and free the private data.
 */
static void lofs_kill_block_super(struct super_block *sb)
{
    struct lofs_sb_info *sb_info = lofs_superblock_to_private(sb);
    kill_anon_super(sb);
#if defined(HAVE_BACKING_DEV)
    if (sb_info) {
        bdi_destroy(&sb_info->bdi);
    }
#endif
    kmem_cache_free(lofs_sb_info_cache, sb_info);
}

#else /* !HAVE_MOUNT_IN_FS_TYPE */

/**
 * lofs_fill_super
 * @sb: The lofs super block
 * @raw_data: The options passed to mount
 * @silent: Not used but required by function prototype
 *
 * Sets up what we can of the sb, rest is done in lofs_read_super
 *
 * Returns zero on success; non-zero otherwise
 */
static int
lofs_fill_super(struct super_block *sb, void *raw_data, int silent)
{
    int rc = 0;
    struct inode *inode;
    struct nameidata nd;

    /* Released in lofs_put_super() */
    struct lofs_sb_info *sbi;
    sbi = kmem_cache_zalloc(lofs_sb_info_cache, GFP_KERNEL);
    if (!sbi) {
        lofs_printk(KERN_WARNING, "Out of memory\n");
        return -ENOMEM;
    }
    lofs_set_superblock_private(sb, sbi);
    sb->s_op = (struct super_operations *) &lofs_sops;

    /* Released through deactivate_super(sb) from get_sb_nodev */
#if defined(HAVE_S_D_OP)
    sb->s_d_op = &lofs_dops;
#endif

    rc = path_lookup("/", LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &nd);
    if (rc) {
        lofs_printk(KERN_WARNING, "path_lookup() failed\n");
        return rc;
    }
    lofs_set_superblock_lower(sb, NAMEIDATA_TO_DENTRY(&nd)->d_sb);
    sb->s_maxbytes =  NAMEIDATA_TO_DENTRY(&nd)->d_sb->s_maxbytes;
    sb->s_blocksize = NAMEIDATA_TO_DENTRY(&nd)->d_sb->s_blocksize;
#ifdef HAVE_ADDRESS_SPACE_OPS_EXT
    sb->s_flags |= MS_HAS_NEW_AOPS;
#endif

    /* Get the root inode and dentry.  We have to bootstrap this one,
     * since it doesn't get created via the regular lookup mechanism.
     */

    inode = lofs_get_inode(NAMEIDATA_TO_DENTRY(&nd)->d_inode, sb);
    if (IS_ERR(inode)) {
        dput(NAMEIDATA_TO_DENTRY(&nd));
        mntput(NAMEIDATA_TO_VFSMNT(&nd));
        return PTR_ERR(inode);
    }

    sb->s_root = d_alloc_root(inode);
    if (!sb->s_root) {
        iput(inode);
        dput(NAMEIDATA_TO_DENTRY(&nd));
        mntput(NAMEIDATA_TO_VFSMNT(&nd));
        return -ENOMEM;
    }
    lofs_set_dentry_private(sb->s_root,
            kmem_cache_zalloc(lofs_dentry_info_cache, GFP_KERNEL));
    lofs_set_dentry_lower(sb->s_root, NAMEIDATA_TO_DENTRY(&nd));
    lofs_set_dentry_lower_mnt(sb->s_root, NAMEIDATA_TO_VFSMNT(&nd));

#if !defined(HAVE_S_D_OP)
    sb->s_root->d_op = (struct dentry_operations *) &lofs_dops;
#endif

    return 0;
}

/**
 * lofs_get_sb
 * @fs_type
 * @flags
 * @dev_name: The path to mount over
 * @raw_data: The options passed into the kernel
 *
 * The whole lofs_get_sb process is broken into 3 functions:
 * lofs_fill_super(): used by get_sb_nodev, fills out the super_block
 *                        with as much information as it can before needing
 *                        the lower filesystem.
 * lofs_read_super(): this accesses the lower filesystem and uses
 *                        lofs_interpolate to perform most of the linking
 */
static int lofs_get_sb(struct file_system_type *fs_type, int flags,
                        const char *dev_name, void *raw_data,
                        struct vfsmount *mnt)
{
    int rc;
    rc = get_sb_nodev(fs_type, flags, raw_data, lofs_fill_super, mnt);
    if (rc < 0) {
        printk(KERN_ERR "Getting sb failed; rc = [%d]\n", rc);
    }
    return rc;
}

/**
 * lofs_kill_block_super
 * @sb: The lofs super block
 *
 * Used to bring the superblock down and free the private data.
 * Private data is free'd in lofs_put_super()
 */
static void lofs_kill_block_super(struct super_block *sb)
{
        generic_shutdown_super(sb);
}

#endif

static struct file_system_type lofs_fs_type = {
        .owner = THIS_MODULE,
        .name = "lofs",
#if defined(HAVE_MOUNT_IN_FS_TYPE)
        .mount = lofs_mount,
#else
        .get_sb = lofs_get_sb,
#endif
        .kill_sb = lofs_kill_block_super,
        .fs_flags = 0
};

/**
 * inode_info_init_once
 *
 * Initializes the lofs_inode_info_cache when it is created
 */

#if defined(HAVE_CLEANUP_IN_KMEM_CACHE_CREATE)          /* 3-arg init_once. */
#define INIT_ONCE_ARGS  void *vptr, KMEM_CACHE_T *cachep, unsigned long flags
#elif defined(HAVE_KMEM_CACHE_IN_KMEM_CACHE_CREATE_INIT)/* 2-arg init_once. */
#define INIT_ONCE_ARGS  KMEM_CACHE_T *cachep, void *vptr
#else                                                   /* 1-arg init-once. */
#define INIT_ONCE_ARGS  void *vptr
#endif

static void
inode_info_init_once(INIT_ONCE_ARGS)
{
        struct lofs_inode_info *ei = (struct lofs_inode_info *)vptr;
        inode_init_once(&ei->vfs_inode);
}

static struct lofs_cache_info {
        KMEM_CACHE_T **cache;
        const char *name;
        size_t size;
        void (*ctor)(INIT_ONCE_ARGS);
} lofs_cache_infos[] = {
        {
            .cache = &lofs_file_info_cache,
            .name = "lofs_file_cache",
            .size = sizeof(struct lofs_file_info),
        },
        {
            .cache = &lofs_dentry_info_cache,
            .name = "lofs_dentry_info_cache",
            .size = sizeof(struct lofs_dentry_info),
        },
        {
            .cache = &lofs_inode_info_cache,
            .name = "lofs_inode_cache",
            .size = sizeof(struct lofs_inode_info),
            .ctor = inode_info_init_once,
        },
        {
            .cache = &lofs_sb_info_cache,
            .name = "lofs_sb_cache",
            .size = sizeof(struct lofs_sb_info),
        },
        {
            .cache = &lofs_lookup_req_cache,
            .name = "lofs_lookup_req_cache",
            .size = sizeof(struct lofs_lookup_req),
        },
};

static void lofs_free_kmem_caches(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(lofs_cache_infos); i++) {
        struct lofs_cache_info *info;

        info = &lofs_cache_infos[i];
        if (*(info->cache)) {
            kmem_cache_destroy(*(info->cache));
        }
    }
}

/**
 * lofs_init_kmem_caches
 *
 * Returns zero on success; non-zero otherwise
 */
static int lofs_init_kmem_caches(void)
{
        int i;

        for (i = 0; i < ARRAY_SIZE(lofs_cache_infos); i++) {
                struct lofs_cache_info *info;

                info = &lofs_cache_infos[i];
                *(info->cache) = kmem_cache_create(info->name, info->size,
                        0, SLAB_HWCACHE_ALIGN, info->ctor
#ifdef HAVE_CLEANUP_IN_KMEM_CACHE_CREATE
                        , NULL
#endif
                    );
                if (!*(info->cache)) {
                        lofs_free_kmem_caches();
                        lofs_printk(KERN_WARNING, "%s: "
                                        "kmem_cache_create failed\n",
                                        info->name);
                        return -ENOMEM;
                }
        }
        return 0;
}

static int __init lofs_init(void)
{
    int rc = 0;

    rc = lofs_init_kmem_caches();
    if (rc) {
        printk(KERN_ERR "Failed to allocate one or more kmem_cache objects\n");
        goto out;
    }

    rc = register_filesystem(&lofs_fs_type);
    if (rc) {
        printk(KERN_ERR "Failed to register filesystem\n");
        goto out_free_kmem_caches;
    }

    rc = lofs_init_kthread();
    if (rc) {
        printk(KERN_ERR "kthread initialization failed; rc = [%d]\n", rc);
        goto out_unregister_filesystem;
    }
    goto out;

out_unregister_filesystem:
    unregister_filesystem(&lofs_fs_type);
out_free_kmem_caches:
    lofs_free_kmem_caches();
out:
    return rc;
}

static void __exit lofs_exit(void)
{
    lofs_destroy_kthread();
    unregister_filesystem(&lofs_fs_type);
    lofs_free_kmem_caches();
}

MODULE_AUTHOR("Eric Melski <ericm@electric-cloud.com>");
MODULE_DESCRIPTION("lofs");

MODULE_LICENSE("GPL");

module_init(lofs_init)
module_exit(lofs_exit)
