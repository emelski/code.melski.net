/**
 * lofs: Linux filesystem loopback layer
 * Kernel declarations.
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2008 International Business Machines Corp.
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

#ifndef LOFS_KERNEL_H
#define LOFS_KERNEL_H

#include "kernel_config.h"
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/hash.h>
#include <linux/mutex.h>
#include <linux/fs_stack.h>
#if defined(HAVE_BACKING_DEV)
#include <linux/backing-dev.h>
#endif

/* inode private data. */
struct lofs_inode_info {
        struct inode vfs_inode;
        struct inode *wii_inode;
        struct file *lower_file;
        struct mutex lower_file_mutex;
};

/* dentry private data. Each dentry must keep track of a lower vfsmount too. */
struct lofs_dentry_info {
    struct dentry *lower_dentry;
    struct vfsmount *lower_mnt;
};

/* superblock private data. */
struct lofs_sb_info {
        struct super_block *wsi_sb;
#if defined(HAVE_BACKING_DEV)
    struct backing_dev_info bdi;
#endif
};

/* file private data. */
struct lofs_file_info {
        struct file *wfi_file;
};

static inline struct lofs_file_info *
lofs_file_to_private(struct file *file)
{
        return (struct lofs_file_info *)file->private_data;
}

static inline void
lofs_set_file_private(struct file *file,
                          struct lofs_file_info *file_info)
{
        file->private_data = file_info;
}

static inline struct file *lofs_file_to_lower(struct file *file)
{
        return ((struct lofs_file_info *)file->private_data)->wfi_file;
}

static inline void
lofs_set_file_lower(struct file *file, struct file *lower_file)
{
        ((struct lofs_file_info *)file->private_data)->wfi_file =
                lower_file;
}

static inline struct lofs_inode_info *
lofs_inode_to_private(struct inode *inode)
{
        return container_of(inode, struct lofs_inode_info, vfs_inode);
}

static inline struct inode *lofs_inode_to_lower(struct inode *inode)
{
        return lofs_inode_to_private(inode)->wii_inode;
}

static inline void
lofs_set_inode_lower(struct inode *inode, struct inode *lower_inode)
{
        lofs_inode_to_private(inode)->wii_inode = lower_inode;
}

static inline struct lofs_sb_info *
lofs_superblock_to_private(struct super_block *sb)
{
        return (struct lofs_sb_info *)sb->s_fs_info;
}

static inline void
lofs_set_superblock_private(struct super_block *sb,
                                struct lofs_sb_info *sb_info)
{
        sb->s_fs_info = sb_info;
}

static inline struct super_block *
lofs_superblock_to_lower(struct super_block *sb)
{
        return ((struct lofs_sb_info *)sb->s_fs_info)->wsi_sb;
}

static inline void
lofs_set_superblock_lower(struct super_block *sb,
                              struct super_block *lower_sb)
{
        ((struct lofs_sb_info *)sb->s_fs_info)->wsi_sb = lower_sb;
}

static inline struct lofs_dentry_info *
lofs_dentry_to_private(struct dentry *dentry)
{
        return (struct lofs_dentry_info *)dentry->d_fsdata;
}

static inline void
lofs_set_dentry_private(struct dentry *dentry,
                            struct lofs_dentry_info *dentry_info)
{
        dentry->d_fsdata = dentry_info;
}

static inline struct dentry *
lofs_dentry_to_lower(struct dentry *dentry)
{
        return ((struct lofs_dentry_info *)dentry->d_fsdata)->lower_dentry;
}

static inline void
lofs_set_dentry_lower(struct dentry *dentry, struct dentry *lower_dentry)
{
        ((struct lofs_dentry_info *)dentry->d_fsdata)->lower_dentry =
                lower_dentry;
}

static inline struct vfsmount *
lofs_dentry_to_lower_mnt(struct dentry *dentry)
{
        return ((struct lofs_dentry_info *)dentry->d_fsdata)->lower_mnt;
}

static inline void
lofs_set_dentry_lower_mnt(struct dentry *dentry, struct vfsmount *lower_mnt)
{
        ((struct lofs_dentry_info *)dentry->d_fsdata)->lower_mnt = lower_mnt;
}

#if defined(HAVE_FSSTACK_COPY_ATTR_ALL_3_ARG)
#define FSSTACK_COPY_ATTR_ALL(d, s)     fsstack_copy_attr_all((d), (s), NULL)
#elif defined(HAVE_FSSTACK_COPY_ATTR_ALL_2_ARG)
#define FSSTACK_COPY_ATTR_ALL(d, s) fsstack_copy_attr_all((d), (s))
#endif

#define lofs_printk(type, fmt, arg...) \
        __lofs_printk(type "%s: " fmt, __func__, ## arg);
void __lofs_printk(const char *fmt, ...);

#ifdef HAVE_ADDRESS_SPACE_OPS_EXT
#define ADDRESS_SPACE_OPS_T struct address_space_operations_ext
#else
#define ADDRESS_SPACE_OPS_T struct address_space_operations
#endif

extern const struct file_operations lofs_main_fops;
extern const struct file_operations lofs_dir_fops;
extern const struct inode_operations lofs_main_iops;
extern const struct inode_operations lofs_dir_iops;
extern const struct inode_operations lofs_symlink_iops;
extern const struct super_operations lofs_sops;
extern const struct dentry_operations lofs_dops;
extern const ADDRESS_SPACE_OPS_T lofs_aops;
extern int lofs_verbosity;

#ifdef HAVE_KMEM_CACHE_T
#define KMEM_CACHE_T kmem_cache_t
#else
#define KMEM_CACHE_T struct kmem_cache
#endif

extern KMEM_CACHE_T *lofs_file_info_cache;
extern KMEM_CACHE_T *lofs_dentry_info_cache;
extern KMEM_CACHE_T *lofs_inode_info_cache;
extern KMEM_CACHE_T *lofs_sb_info_cache;
extern KMEM_CACHE_T *lofs_lookup_req_cache;

struct lofs_lookup_req {
#define LOFS_REQ_PROCESSED      0x1
#define LOFS_REQ_ZOMBIE         0x2
#define LOFS_REQ_ERROR          0x4
    u32 flags;                          /* Flags, like *_ZOMBIE. */
    char *name;                         /* Name to be looked up. */
    struct path *p;                     /* Lookup result goes here. */
    wait_queue_head_t wait;             /* Signalled when the req is done. */
    struct mutex mux;                   /* Protects access to the struct. */
    struct list_head kthread_ctl_list;  /* Used to add this req to the list
                                         * of pending reqs. */
};

#ifndef HAVE_STRUCT_PATH
struct path {
    struct vfsmount *mnt;
    struct dentry   *dentry;
};
#endif    

int lofs_revalidate_lower(struct dentry *lower_dentry,
        struct vfsmount *lower_mnt, struct nameidata *lofs_nd);
struct inode *lofs_get_inode(struct inode *lower_inode,
        struct super_block *sb);
int lofs_inode_test(struct inode *inode, void *candidate_lower_inode);
int lofs_inode_set(struct inode *inode, void *lower_inode);
void lofs_init_inode(struct inode *inode, struct inode *lower_inode);
int lofs_write_lower(struct inode *lofs_inode, char *data,
                         loff_t offset, size_t size);
int lofs_write_lower_page_segment(struct inode *lofs_inode,
                                      struct page *page_for_lower,
                                      size_t offset_in_page, size_t size);
int lofs_read_lower_page(struct page *page_for_lofs,
        pgoff_t page_index, struct inode *lofs_inode);
struct page *lofs_get_locked_page(struct file *file, loff_t index);
int lofs_init_persistent_file(struct dentry *lofs_dentry, mode_t mode);

int lofs_init_kthread(void);
void lofs_destroy_kthread(void);
int lofs_lookup_managed(char *name, struct path *path);

#ifdef NAMEIDATA_USES_STRUCT_PATH
#define NAMEIDATA_TO_DENTRY(_ndp)       (_ndp)->path.dentry
#define NAMEIDATA_TO_VFSMNT(_ndp)       (_ndp)->path.mnt
#else  /* !NAMEIDATA_USES_STRUCT_PATH */
#define NAMEIDATA_TO_DENTRY(_ndp)       (_ndp)->dentry
#define NAMEIDATA_TO_VFSMNT(_ndp)       (_ndp)->mnt
#endif /* NAMEIDATA_USES_STRUCT_PATH */

#define LOFS_ND_DECLARATIONS    struct dentry *saved_dentry = 0; \
                                struct vfsmount *saved_vfsmount = 0
#define LOFS_ND_SAVE_ARGS(_nd, _lower_dentry, _lower_mount)                 \
                                saved_dentry   = NAMEIDATA_TO_DENTRY(_nd);  \
                                saved_vfsmount = NAMEIDATA_TO_VFSMNT(_nd);  \
                                NAMEIDATA_TO_DENTRY(_nd) = (_lower_dentry); \
                                NAMEIDATA_TO_VFSMNT(_nd) = (_lower_mount)
#define LOFS_ND_RESTORE_ARGS(_nd)                                           \
                                NAMEIDATA_TO_DENTRY(_nd) = saved_dentry;    \
                                NAMEIDATA_TO_VFSMNT(_nd) = saved_vfsmount;

#ifdef HAVE_3_ARG_NOTIFY_CHANGE
#define NOTIFY_CHANGE(d, a, m)  notify_change((d), (m), (a))
#else
#define NOTIFY_CHANGE(d, a, m)  notify_change((d), (a))
#endif

#ifdef FILE_USES_STRUCT_PATH
#define FILE_TO_DENTRY(_f)              (_f)->path.dentry
#define FILE_TO_VFSMNT(_f)              (_f)->path.mnt
#else
#define FILE_TO_DENTRY(_f)              (_f)->f_dentry
#define FILE_TO_VFSMNT(_f)              (_f)->f_vfsmnt
#endif

#ifdef HAVE_INODE_I_MUTEX
#define LOCK_INODE(x)                   mutex_lock(&((x)->i_mutex))
#define UNLOCK_INODE(x)                 mutex_unlock(&((x)->i_mutex))
#else
#define LOCK_INODE(x)                   down(&((x)->i_sem))
#define UNLOCK_INODE(x)                 up(&((x)->i_sem))
#endif

/*
 * If the kernel has been patched to support the AppArmor security module,
 * then vfs_link, vfs_symlink, etc, take additional "struct vfsmount *" args.
 * We don't have an easy way to get the "correct" value for these parameters,
 * so we just pass NULL.  That's good enough for our needs at this time.
 */

#ifdef HAVE_APP_ARMOR_SECURITY
#define VFS_LINK(d1, i1, d2)    vfs_link((d1), 0, (i1), (d2), 0)
#define VFS_MKDIR(i, d, f)      vfs_mkdir((i), (d), 0, (f))
#define VFS_MKNOD(i, d, f, dev) vfs_mknod((i), (d), 0, (f), (dev))
#define VFS_RENAME(i1,d1,i2,d2) vfs_rename((i1), (d1), 0, (i2), (d2), 0)
#define VFS_RMDIR(i, d)         vfs_rmdir((i), (d), 0)
#define VFS_UNLINK(i, d)        vfs_unlink((i), (d), 0)
#else
#define VFS_LINK(d1, i1, d2)    vfs_link((d1), (i1), (d2))
#define VFS_MKDIR(i, d, f)      vfs_mkdir((i), (d), (f))
#define VFS_MKNOD(i, d, f, dev) vfs_mknod((i), (d), (f), (dev))
#define VFS_RENAME(i1,d1,i2,d2) vfs_rename((i1), (d1), (i2), (d2))
#define VFS_RMDIR(i, d)         vfs_rmdir((i), (d))
#define VFS_UNLINK(i, d)        vfs_unlink((i), (d))
#endif

/* 
 * There's a lot of variety in the humble vfs_symlink() API.  If the AppArmor
 * security module patches have been applied, then vfs_symlink takes an extra
 * vfsmount argument, just like the other vfs_* API's.  However, some versions
 * of Linux also have a patch that modifies vfs_symlink() to take an integer
 * "mode" argument.
 */

#ifdef HAVE_APP_ARMOR_SECURITY
#ifdef HAVE_MODE_IN_VFS_SYMLINK
#    define VFS_SYMLINK(i, d, c, m)     vfs_symlink((i), (d), 0, (c), (m))
#  else
#    define VFS_SYMLINK(i, d, c, m)     vfs_symlink((i), (d), 0, (c))
#  endif
#else
#ifdef HAVE_MODE_IN_VFS_SYMLINK
#    define VFS_SYMLINK(i, d, c, m)     vfs_symlink((i), (d), (c), (m))
#  else
#    define VFS_SYMLINK(i, d, c, m)     vfs_symlink((i), (d), (c))
#  endif
#endif

#ifdef HAVE_DO_SYNC_READ
#define VFS_READ_HELPER         do_sync_read
#else
#define VFS_READ_HELPER         generic_file_read
#endif

#ifdef HAVE_DO_SYNC_WRITE
#define VFS_WRITE_HELPER        do_sync_write
#else
#define VFS_WRITE_HELPER        generic_file_write
#endif

#ifdef HAVE_SET_NLINK
#define SET_NLINK(i, n)         set_nlink((i), (n))
#else
#define SET_NLINK(i, n)         (i)->i_nlink = (n)
#endif

#ifdef HAVE_UMODE_IN_CREATE
#    define CREATE_MODE_TYPE            umode_t
#else
#    define CREATE_MODE_TYPE            int
#endif

#ifdef HAVE_END_WRITEBACK
#    define CLEAR_INODE                 end_writeback
#else
#    define CLEAR_INODE                 clear_inode
#endif

#if defined(HAVE_OLD_FOLLOW_DOWN_BACKPORT)
#define FOLLOW_DOWN(p)                  __follow_down(&(p)->mnt,&(p)->dentry,0)
#elif defined(HAVE_FOLLOW_DOWN_BACKPORT)
#define FOLLOW_DOWN(p)                  __follow_down((p), 0)
#elif defined(HAVE_OLD_FOLLOW_DOWN)
#define FOLLOW_DOWN(p)                  follow_down(&(p)->mnt, &(p)->dentry)
#elif defined(HAVE_BOOL_IN_FOLLOW_DOWN)
#define FOLLOW_DOWN(p)                  follow_down((p), 0)
#else
#define FOLLOW_DOWN(p)                  follow_down((p))
#endif

#if defined(HAVE_STRUCT_PATH)
#define D_PATH(p, c, s)                 d_path((p), (c), (s))
#else
#define D_PATH(p, c, s)                 d_path((p)->dentry, (p)->mnt, (c), (s))
#endif

#if defined(DCACHE_MANAGED_DENTRY)
#define LOFS_MANAGED_DENTRY(d)          ((d)->d_flags & DCACHE_MANAGED_DENTRY)
#else
#define LOFS_MANAGED_FLAGS              (DMANAGED_MOUNTPOINT \
                                         |DMANAGED_AUTOMOUNT \
                                         |DMANAGED_TRANSIT)
#define LOFS_MANAGED_DENTRY(d)          ((d)->d_mounted & LOFS_MANAGED_FLAGS)
#endif

#endif /* #ifndef LOFS_KERNEL_H */
