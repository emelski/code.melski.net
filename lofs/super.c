/**
 * lofs: Linux filesystem loopback layer
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2006 International Business Machines Corp.
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

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include "lofs_kernel.h"

KMEM_CACHE_T *lofs_inode_info_cache;

/**
 * lofs_alloc_inode - allocate an lofs inode
 * @sb: Pointer to the lofs super block
 *
 * Called to bring an inode into existence.
 *
 * Only handle allocation, setting up structures should be done in
 * lofs_read_inode. This is because the kernel, between now and
 * then, will 0 out the private data pointer.
 *
 * Returns a pointer to a newly allocated inode, NULL otherwise
 */
static struct inode *lofs_alloc_inode(struct super_block *sb)
{
        struct lofs_inode_info *inode_info;
        struct inode *inode = NULL;

        inode_info = kmem_cache_alloc(lofs_inode_info_cache, GFP_KERNEL);
        if (unlikely(!inode_info))
                goto out;
        mutex_init(&inode_info->lower_file_mutex);
        inode_info->lower_file = NULL;
        inode = &inode_info->vfs_inode;
out:
        return inode;
}

#ifdef LOOKUP_RCU
static void lofs_i_callback(struct rcu_head *head)
{
    struct inode *inode = container_of(head, struct inode, i_rcu);
    INIT_LIST_HEAD(&inode->i_dentry);
    kmem_cache_free(lofs_inode_info_cache, lofs_inode_to_private(inode));
}
#endif

/**
 * lofs_destroy_inode
 * @inode: The lofs inode
 *
 * This is used during the final destruction of the inode.  All
 * allocation of memory related to the inode will be released here.
 * This function also fput()'s the persistent file for the lower
 * inode. There should be no chance that this deallocation will be
 * missed.
 */
static void lofs_destroy_inode(struct inode *inode)
{
    struct lofs_inode_info *inode_info;

    inode_info = lofs_inode_to_private(inode);
    if (inode_info->lower_file) {
        fput(inode_info->lower_file);
        inode_info->lower_file = NULL;
    }
    mutex_destroy(&inode_info->lower_file_mutex);
#ifdef LOOKUP_RCU
    /* lofs_i_callback will free the inode_info.  It seems like we should
     * be able to do it here after call_rcu() returns, which would allow
     * us to have better code compatibility across kernel versions.
     * Unfortunately, on RCU-enabled kernels, doing the free here after
     * call_rcu causes a kernel panic.
     */

    call_rcu(&inode->i_rcu, lofs_i_callback);
#else
    /* Kernel is not RCU-enabled, so free the inode_info here. */

    kmem_cache_free(lofs_inode_info_cache, inode_info);
#endif
}

#if !defined(HAVE_MOUNT_IN_FS_TYPE)
/**
 * lofs_put_super
 * @sb: Pointer to the lofs super block
 *
 * Final actions when unmounting a file system.
 * This will handle deallocation and release of our private data.
 */
static void lofs_put_super(struct super_block *sb)
{
    struct lofs_sb_info *sb_info = lofs_superblock_to_private(sb);
    kmem_cache_free(lofs_sb_info_cache, sb_info);
    lofs_set_superblock_private(sb, NULL);
}
#endif

/**
 * lofs_statfs
 * @sb: The lofs super block
 * @buf: The struct kstatfs to fill in with stats
 *
 * Get the filesystem statistics. Currently, we let this pass right through
 * to the lower filesystem and take no action ourselves.
 */

#if defined(STATFS_HAS_SUPER_BLOCK)
static int lofs_statfs(struct super_block *arg, struct kstatfs *buf)
{
    return vfs_statfs(lofs_superblock_to_lower(arg), buf);
}
#elif defined(STATFS_HAS_DENTRY)
static int lofs_statfs(struct dentry *arg, struct kstatfs *buf)
{
    return vfs_statfs(lofs_dentry_to_lower(arg), buf);
}
#elif defined(STATFS_HAS_PATH)
static int lofs_statfs(struct dentry *arg, struct kstatfs *buf)
{
    struct path lower;
    lower.dentry = lofs_dentry_to_lower(arg);
    lower.mnt    = lofs_dentry_to_lower_mnt(arg);
    return vfs_statfs(&lower, buf);

}
#endif

/**
 * lofs_clear_inode
 * @inode - The lofs inode
 *
 * Called by iput() when the inode reference count reached zero and the inode
 * is not hashed anywhere.  Used to clear anything that needs to be, before the
 * inode is completely destroyed and put on the inode free list. We use this to
 * drop out reference to the lower inode.
 */
static void lofs_clear_inode(struct inode *inode)
{
#ifdef HAVE_EVICT_INODE
    truncate_inode_pages(&inode->i_data, 0);
    CLEAR_INODE(inode);
#endif
    iput(lofs_inode_to_lower(inode));
}

const struct super_operations lofs_sops = {
        .alloc_inode = lofs_alloc_inode,
        .destroy_inode = lofs_destroy_inode,
        .drop_inode = generic_delete_inode,
#if !defined(HAVE_MOUNT_IN_FS_TYPE)
        .put_super = lofs_put_super,
#endif
        .statfs = lofs_statfs,
        .remount_fs = NULL,
#ifdef HAVE_EVICT_INODE
        .evict_inode = lofs_clear_inode,
#else
        .clear_inode = lofs_clear_inode,
#endif
};
