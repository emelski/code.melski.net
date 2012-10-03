/**
 * lofs: Linux loopback filesystem layer
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

#include "kernel_config.h"
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/security.h>
#include <linux/compat.h>
#if defined(HAVE_BIG_KERNEL_LOCK)
#include <linux/smp_lock.h>
#endif
#include "lofs_kernel.h"

/**
 * lofs_readdir
 * @file:       The lofs directory file.
 * @dirent:     Buffer to fill with directory entries.
 * @filldir:    The filldir callback function
 */
static int lofs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
        int rc;
        struct file *lower_file;
        struct inode *inode;

        lower_file = lofs_file_to_lower(file);
        if (lower_file->f_pos != file->f_pos) {
            vfs_llseek(lower_file, file->f_pos, 0 /* SEEK_SET */);
        }
        inode = FILE_TO_DENTRY(file)->d_inode;
        rc = vfs_readdir(lower_file, filldir, dirent);
        file->f_pos = lower_file->f_pos;
        if (rc >= 0) {
            fsstack_copy_attr_atime(inode,FILE_TO_DENTRY(lower_file)->d_inode);
        }
        return rc;
}

KMEM_CACHE_T *lofs_file_info_cache;

/**
 * lofs_open
 * @inode: inode speciying file to open
 * @file: Structure to return filled in
 *
 * Opens the file specified by inode.
 *
 * Returns zero on success; non-zero otherwise
 */
static int lofs_open(struct inode *inode, struct file *file)
{
    int rc;
    struct dentry *lofs_dentry = FILE_TO_DENTRY(file);
    struct dentry *lower_dentry = lofs_dentry_to_lower(lofs_dentry);
    struct lofs_file_info *file_info;

    /* Released in lofs_release or here if failure */
    file_info = kmem_cache_alloc(lofs_file_info_cache, GFP_KERNEL);
    lofs_set_file_private(file, file_info);
    if (!file_info) {
        lofs_printk(KERN_ERR, "Error attempting to allocate memory\n");
        return -ENOMEM;
    }

    memset(file_info, 0, sizeof(struct lofs_file_info));
    lower_dentry = lofs_dentry_to_lower(lofs_dentry);
    rc = lofs_init_persistent_file(lofs_dentry, file->f_mode);
    if (rc) {
        printk(KERN_ERR "%s: Error attempting to initialize "
                "the persistent file for the dentry with name "
                "[%s]; rc = [%d]\n", __func__,
                lofs_dentry->d_name.name, rc);
        kmem_cache_free(lofs_file_info_cache, file_info);
        return rc;
    }

    lofs_set_file_lower(file, lofs_inode_to_private(inode)->lower_file);
    return 0;
}

#ifdef FLUSH_HAS_OWNER
#define FLUSH_ARGS(f, t)        f, t
#else
#define FLUSH_ARGS(f, t)        f
#endif

static int
lofs_flush(FLUSH_ARGS(struct file *file, fl_owner_t td))
{
        int rc = 0;
        struct file *lower_file = NULL;

        lower_file = lofs_file_to_lower(file);
        if (lower_file->f_op && lower_file->f_op->flush) {
            rc = lower_file->f_op->flush(FLUSH_ARGS(lower_file, td));
        }
        return rc;
}

static int lofs_release(struct inode *inode, struct file *file)
{
        kmem_cache_free(lofs_file_info_cache, lofs_file_to_private(file));
        return 0;
}

#if defined(HAVE_VFS_FSYNC) || defined(HAVE_VFS_FSYNC_RANGE)
#if defined(HAVE_OFFSET_IN_FSYNC)
#define FSYNC_ARGS(f, d, s, e, ds)      f, s, e, ds
#define FSYNC_HELPER                    vfs_fsync_range
#elif defined(HAVE_DENTRY_IN_FSYNC)
#define FSYNC_ARGS(f, d, s, e, ds)      f, d, ds
#define FSYNC_HELPER                    vfs_fsync
#else
#define FSYNC_ARGS(f, d, s, e, ds)      f, ds
#define FSYNC_HELPER                    vfs_fsync
#endif

static int
lofs_fsync(FSYNC_ARGS(struct file *file, 
                struct dentry *dentry, 
                loff_t start,
                loff_t end,
                int datasync))
{
    struct file *lower = lofs_file_to_lower(file);
    int result = 0;

    /* Make sure the LOFS pages are flushed out to the lower filesystem.
     * Different kernels have different utility functions to help with
     * this; in the worst case (2.6.9) we roll our own.
     */

#if defined(HAVE_OFFSET_IN_FSYNC)
    /* This is the 3.2.0+ version. */

    result = filemap_write_and_wait_range(file->f_mapping, start, end);
#elif defined(HAVE_FILEMAP_WRITE_AND_WAIT)
    /* This is the 2.6.18 - 3.2.0 version. */

    result = filemap_write_and_wait(file->f_mapping);
#else
    /* This is for versions prior to 2.6.18.  This is basically a copy of
     * the implementation of filemap_write_and_wait, which unfortunately was
     * not added until 2.6.18 or so.
     */

    if (file->f_mapping->nrpages) {
        result = filemap_fdatawrite(file->f_mapping);
        if (result != -EIO) {
            int result2 = filemap_fdatawait(file->f_mapping);
            if (result == 0) {
                result = result2;
            }
        }
    }
#endif
    if (result != 0) {
        return result;
    }

    /* Then give the lower filesystem a chance to do its own sync. */

    return FSYNC_HELPER(FSYNC_ARGS(lower, 
                    lofs_dentry_to_lower(dentry),
                    start,
                    end,
                    datasync));
}
#else
static int
lofs_fsync(struct file *file, struct dentry *dentry, int datasync)
{
    int rc = -EINVAL;
    struct file *lower_file = lofs_file_to_lower(file);
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    struct inode *lower_inode = lower_dentry->d_inode;
    if (lower_file->f_op && lower_file->f_op->fsync) {
        LOCK_INODE(lower_inode);
        rc = lower_file->f_op->fsync(lower_file, lower_dentry, datasync);
        UNLOCK_INODE(lower_inode);
    }
    return rc;
}
#endif

static int lofs_fasync(int fd, struct file *file, int flag)
{
        int rc = 0;
        struct file *lower_file = NULL;

#if defined(HAVE_BIG_KERNEL_LOCK)
        lock_kernel();
#endif

        lower_file = lofs_file_to_lower(file);
        if (lower_file->f_op && lower_file->f_op->fasync)
                rc = lower_file->f_op->fasync(fd, lower_file, flag);

#if defined(HAVE_BIG_KERNEL_LOCK)
        unlock_kernel();
#endif
        return rc;
}

/* lofs_common_ioctl
 *
 *      Common bits of the ioctl implementation -- checks for the lofs-specific
 *      ioctls and returns 1 if the ioctl was handled by this function, zero
 *      otherwise.
 *
 * @file:       The lofs file.
 * @cmd:        The ioctl to invoke.
 * @arg:        The cmd-defined arg to the ioctl.
 */

static inline int lofs_common_ioctl(
    struct file *file, 
    unsigned int cmd, 
    unsigned long arg)
{
    static const int LOFS_IOCTL_PRUNE = _IOR(0x15, 7, int);
    if (cmd == LOFS_IOCTL_PRUNE) {
        /* Prune the dentry cache from the root of this filesystem, so the
         * lofs releases any holds it has on underlying filesystems
         * referenced from here down.
         */

        struct dentry *root = FILE_TO_DENTRY(file)->d_sb->s_root;
        if (root && !(root->d_flags & DCACHE_DISCONNECTED)) {
            shrink_dcache_parent(root);
        }
        return 1;
    }
    return 0;
}

#if HAVE_UNLOCKED_IOCTL

/* lofs_unlocked_ioctl
 *
 *      As of 2.6.38, file_operations.ioctl was replaced by .unlocked_ioctl,
 *      with a different call signature.
 *
 * @file:       The lofs file.
 * @cmd:        The ioctl to invoke.
 * @arg:        The cmd-defined arg to the ioctl.
 */

static long lofs_unlocked_ioctl(
    struct file *file, 
    unsigned int cmd, 
    unsigned long arg)
{
    struct file *lower_file = 0;
    long rc = -ENOTTY;

    if (lofs_common_ioctl(file, cmd, arg)) {
        return 0;
    }

    if (lofs_file_to_private(file)) {
        lower_file = lofs_file_to_lower(file);
    }
    if (lower_file && lower_file->f_op) {
        /* If the lower filesystem has an unlocked_ioctl implementation,
         * we prefer to use that.  Otherwise fall back to the old ioctl
         * implementation, which requires us to grab the big kernel lock.
         *
         * Note that this is a very transitional setup:  Linux will at some
         * point nuke the old ioctl interface altogether.
         */

        const struct file_operations *fop = lower_file->f_op;
        if (fop->unlocked_ioctl) {
            return fop->unlocked_ioctl(lower_file, cmd, arg);
        }

#if defined(HAVE_LOCKED_IOCTL)
        if (fop->ioctl) {
            struct inode *inode = lower_file->f_dentry->d_inode;

            lock_kernel();
            rc = fop->ioctl(inode, lower_file, cmd, arg);
            unlock_kernel();
        }
#endif
    }
    return rc;
}

#else /* !HAVE_UNLOCKED_IOCTL */

/* lofs_ioctl
 *
 *      Invoke an ioctl on a file in the lofs.
 *
 * @inode:      The lofs inode.
 * @file:       The lofs file.
 * @cmd:        The ioctl to invoke.
 * @arg:        The cmd-defined arg to the ioctl.
 */

static int
lofs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
               unsigned long arg)
{
    int rc = -ENOTTY;
    struct file *lower_file = NULL;
    if (lofs_common_ioctl(file, cmd, arg)) {
        return 0;
    }

    if (lofs_file_to_private(file)) {
        lower_file = lofs_file_to_lower(file);
    }
    if (lower_file && lower_file->f_op && lower_file->f_op->ioctl) {
        rc = lower_file->f_op->ioctl(lofs_inode_to_lower(inode),
                lower_file, cmd, arg);
    }
    return rc;
}
#endif

#if defined(CONFIG_COMPAT)
/* lofs_compat_ioctl
 *
 *      Used by 32-bit processes invoking ioctl's on a 64-bit filesystem.
 *
 * @file:       The lofs file.
 * @cmd:        The ioctl to invoke.
 * @arg:        The cmd-defined arg to the ioctl.
 */

static long lofs_compat_ioctl(
    struct file *file,
    unsigned int cmd,
    unsigned long arg)
{
    struct file *lower_file = 0;

    /* If the lower filesystem doesn't implement compat_ioctl, the default
     * return of ENOIOCTLCMD will cause the kernel to fall through as if
     * we had not implemented compat_ioctl either.
     */

    long rc = -ENOIOCTLCMD;

    if (lofs_common_ioctl(file, cmd, arg)) {
        return 0;
    }

    if (lofs_file_to_private(file)) {
        lower_file = lofs_file_to_lower(file);
    }
    if (lower_file && lower_file->f_op && lower_file->f_op->compat_ioctl) {
        rc = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
    }
    return rc;
}
#endif

const struct file_operations lofs_dir_fops = {
        /* TODO: Do we need llseek here? */
        .readdir                = lofs_readdir,
#if HAVE_UNLOCKED_IOCTL
        .unlocked_ioctl         = lofs_unlocked_ioctl,
#else /* !HAVE_UNLOCKED_IOCTL */
        .ioctl                  = lofs_ioctl,
#endif
#if defined(CONFIG_COMPAT)
        .compat_ioctl           = lofs_compat_ioctl,
#endif
        .open                   = lofs_open,
        .release                = lofs_release,
        .fsync                  = lofs_fsync,
        .fasync                 = lofs_fasync,
};

const struct file_operations lofs_main_fops = {
        .llseek                 = generic_file_llseek,
        .read                   = VFS_READ_HELPER,
        .write                  = VFS_WRITE_HELPER,
#ifdef HAVE_FILE_OPERATIONS_AIO_READ
        .aio_read               = generic_file_aio_read,
#endif
#ifdef HAVE_FILE_OPERATIONS_AIO_WRITE
        .aio_write              = generic_file_aio_write,
#endif
#if HAVE_UNLOCKED_IOCTL
        .unlocked_ioctl         = lofs_unlocked_ioctl,
#else /* !HAVE_UNLOCKED_IOCTL */
        .ioctl                  = lofs_ioctl,
#endif
#if defined(CONFIG_COMPAT)
        .compat_ioctl           = lofs_compat_ioctl,
#endif
        .mmap                   = generic_file_mmap,
        .open                   = lofs_open,
        .flush                  = lofs_flush,
        .release                = lofs_release,
        .fsync                  = lofs_fsync,
        .fasync                 = lofs_fasync,
        /* TODO: Add splice_read (for newer kernels)? */
        /* TODO: Add lock? */
};
