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

#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/slab.h>
#include "lofs_kernel.h"

/**
 * lofs_revalidate_lower - revalidate a lower dentry
 * @lower_dentry: The lower dentry
 * @lofs_nd: The nameidata associated with the LOFS-level operation
 *
 * If the given lower_dentry has a d_revalidate operation, then invoke
 * it and return its result; otherwise return 1.
 *
 * Returns 1 if valid, 0 if invalid, < 0 on revalidation error.
 *
 */
int lofs_revalidate_lower(
    struct dentry *lower_dentry,
    struct vfsmount *lower_mnt,
    struct nameidata *lofs_nd)
{
    int rc = 1;
    LOFS_ND_DECLARATIONS;

    if (!lower_dentry->d_op || !lower_dentry->d_op->d_revalidate)
        return rc;

    if (!lofs_nd) {
        /* On some versions of Linux, unlink calls d_revalidate with a
         * NULL nameidata.  In that case there's nothing to preserve.
         */

        rc = lower_dentry->d_op->d_revalidate(lower_dentry, lofs_nd);
    } else {
        LOFS_ND_SAVE_ARGS(lofs_nd, lower_dentry, lower_mnt);
        rc = lower_dentry->d_op->d_revalidate(lower_dentry, lofs_nd);
        LOFS_ND_RESTORE_ARGS(lofs_nd);
    }

    return rc;
}

/**
 * lofs_d_revalidate - revalidate an lofs dentry
 * @dentry: The lofs dentry
 * @nd: The associated nameidata
 *
 * Called when the VFS needs to revalidate a dentry. This is called whenever a
 * name lookup finds a dentry in the dcache. Most filesystems leave this as
 * NULL, because all their dentries in the dcache are valid.  lofs dentries
 * might be invalid if the lower filesystem considers the corresponding lower
 * dentry invalid, or if the status of the lower dentry has changed (eg, a
 * previously negative dentry has become positive, or vice versa).
 *
 * Returns 1 if valid, 0 if invalid, < 0 on revalidation error.
 *
 */
static int lofs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
    struct dentry *lower_dentry = lofs_dentry_to_lower(dentry);
    struct vfsmount *lower_mnt = lofs_dentry_to_lower_mnt(dentry);
    int rc = lofs_revalidate_lower(lower_dentry, lower_mnt, nd);

    if (rc > 0) {
        if (dentry->d_inode == 0) {
            if (lower_dentry->d_inode != 0) {
                rc = 0;
            }
        } else {
            struct inode *lower_inode;
            lower_inode = lofs_inode_to_lower(dentry->d_inode);
            if (lower_inode != lower_dentry->d_inode
                    || d_unhashed(lower_dentry)) {
                /* The lower dentry now refers to a different inode, or
                 * the lower entry has been invalidated.  In those cases,
                 * we should invalidate the lofs entry as well.
                 */

                rc = 0;
            } else {
                /* Note that we only refresh the inode attributes in this
                 * specific case:  the lookup has been deemed valid, and
                 * both lofs and the lower filesystem agree on the identity
                 * of the lower inode, and of course the lower inode is
                 * non-null.
                 */

                FSSTACK_COPY_ATTR_ALL(dentry->d_inode, lower_inode);
            }
        }
    }
    return rc;
}

KMEM_CACHE_T *lofs_dentry_info_cache;

/**
 * lofs_d_release
 * @dentry: The lofs dentry
 *
 * Called when a dentry is really deallocated.
 */
static void lofs_d_release(struct dentry *dentry)
{
    if (lofs_dentry_to_private(dentry)) {
        if (lofs_dentry_to_lower(dentry)) {
            dput(lofs_dentry_to_lower(dentry));
            mntput(lofs_dentry_to_lower_mnt(dentry));
        }
        kmem_cache_free(lofs_dentry_info_cache,lofs_dentry_to_private(dentry));
    }
    return;
}

const struct dentry_operations lofs_dops = {
        .d_revalidate = lofs_d_revalidate,
        .d_release = lofs_d_release,
};
