/*
 * Copyright (c) 1997-2007 Erez Zadok <ezk@cs.stonybrook.edu>
 * Copyright (c) 2001-2007 Stony Brook University
 *
 * For specific licensing information, see the COPYING file distributed with
 * this package, or get one from
 * ftp://ftp.filesystems.org/pub/fistgen/COPYING.
 *
 * This Copyright notice must be kept intact and distributed with all
 * fistgen sources INCLUDING sources generated by fistgen.
 */
/*
 * File: fistgen/templates/Linux-2.6/super.c
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef FISTGEN
# include "fist_base0fs.h"
#endif /* FISTGEN */
#include "fist.h"
#include "base0fs.h"

extern void base0fs_iput(inode_t *inode);

void
base0fs_read_inode(inode_t *inode)
{
        static struct address_space_operations base0fs_empty_aops;

        print_entry_location();

        INODE_TO_LOWER(inode) = NULL;

        inode->i_version++;	/* increment inode version */
        inode->i_op = &base0fs_main_iops;
        inode->i_fop = &base0fs_main_fops;
#if 0
        /*
         * XXX: To export a file system via NFS, it has to have the
         * FS_REQUIRES_DEV flag, so turn it on.  But should we inherit it from
         * the lower file system, or can we allow our file system to be exported
         * even if the lower one cannot be natively exported.
         */
        inode->i_sb->s_type->fs_flags |= FS_REQUIRES_DEV;
        /*
         * OK, the above was a hack, which is now turned off because it may
         * cause a panic/oops on some systems.  The correct way to export a
         * "nodev" filesystem is via using nfs-utils > 1.0 and the "fsid=" export
         * parameter, which requires 2.4.20 or later.
         */
#endif
        /* I don't think ->a_ops is ever allowed to be NULL */
        inode->i_mapping->a_ops = &base0fs_empty_aops;
        fist_dprint(7, "setting inode 0x%p a_ops to empty (0x%p)\n",
                    inode, inode->i_mapping->a_ops);

        print_exit_location();
}


#if defined(FIST_DEBUG) || defined(FIST_FILTER_SCA)
/*
 * No need to call write_inode() on the lower inode, as it
 * will have been marked 'dirty' anyway. But we might need
 * to write some of our own stuff to disk.
 */

#ifdef HAVE_WRITE_INODE_RETURNS_INT
#define WRITE_INODE_RETURN_TYPE int
#else
#define WRITE_INODE_RETURN_TYPE void
#endif

#ifdef HAVE_WRITEBACK_CONTROL_IN_WRITE_INODE
#define WRITE_INODE_ARG_TYPE struct writeback_control *
#else
#define WRITE_INODE_ARG_TYPE int
#endif

static WRITE_INODE_RETURN_TYPE base0fs_write_inode(
    struct inode *inode,
    WRITE_INODE_ARG_TYPE unused)
{
#ifdef HAVE_WRITE_INODE_RETURNS_INT
    int err = 0;
#endif
        print_entry_location();

#ifdef HAVE_WRITE_INODE_RETURNS_INT
        print_exit_status(err);
	return err;
#else
        print_exit_location();
#endif
}
#endif /* defined(FIST_DEBUG) || defined(FIST_FILTER_SCA) */


#ifdef HAVE_PUT_INODE
STATIC void
base0fs_put_inode(inode_t *inode)
{
        print_entry_location();
        fist_dprint(8, "%s i_count = %d, i_nlink = %d\n", 
                __FUNCTION__, atomic_read(&inode->i_count), inode->i_nlink);
        /*
         * This is really funky stuff:
         * Basically, if i_count == 1, iput will then decrement it and this inode will be destroyed.
         * It is currently holding a reference to the lower inode.
         * Therefore, it needs to release that reference by calling iput on the lower inode.
         * iput() _will_ do it for us (by calling our clear_inode), but _only_ if i_nlink == 0.
         * The problem is, NFS keeps i_nlink == 1 for silly_rename'd files.
         * So we must for our i_nlink to 0 here to trick iput() into calling our clear_inode.
         */
        if (atomic_read(&inode->i_count) == 1)
                inode->i_nlink = 0;
        print_exit_location();
}
#endif

#if defined(FIST_DEBUG) || defined(FIST_FILTER_SCA)
/*
 * we now define delete_inode, because there are two VFS paths that may
 * destroy an inode: one of them calls clear inode before doing everything
 * else that's needed, and the other is fine.  This way we truncate the inode
 * size (and its pages) and then clear our own inode, which will do an iput
 * on our and the lower inode.
 */
STATIC void
base0fs_delete_inode(inode_t *inode)
{
        print_entry_location();

        fist_checkinode(inode, "base0fs_delete_inode IN");
        truncate_inode_pages(&inode->i_data, 0);
        clear_inode(inode);

        print_exit_location();
}
#endif /* defined(FIST_DEBUG) || defined(FIST_FILTER_SCA) */


/* final actions when unmounting a file system */
STATIC void
base0fs_put_super(super_block_t *sb)
{
        print_entry_location();

        if (SUPERBLOCK_TO_PRIVATE(sb)) {
                KFREE(SUPERBLOCK_TO_PRIVATE(sb));
                SUPERBLOCK_TO_PRIVATE_SM(sb) = NULL;
        }
        fist_dprint(6, "base0fs: released super\n");

        print_exit_location();
}


#ifdef NOT_NEEDED
/*
 * This is called in do_umount before put_super.
 * The superblock lock is not held yet.
 * We probably do not need to define this or call write_super
 * on the lower_sb, because sync_supers() will get to lower_sb
 * sooner or later.  But it is also called from file_fsync()...
 */
STATIC void
base0fs_write_super(super_block_t *sb)
{
        return;
}
#endif /* NOT_NEEDED */


STATIC int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
base0fs_statfs(super_block_t *sb, struct kstatfs *buf)  // statfs to kstatfs
#else
base0fs_statfs(struct dentry *dentry, struct kstatfs *buf)  // statfs to kstatfs
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) */
{
        int err = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
        super_block_t *lower_sb;
#else
        struct dentry *lower_dentry;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) */

        print_entry_location();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
        lower_sb = SUPERBLOCK_TO_LOWER(sb);
        err = vfs_statfs(lower_sb, buf);
#else
        lower_dentry = DENTRY_TO_LOWER(dentry);
        err = vfs_statfs(lower_dentry, buf);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) */

        print_exit_status(err);
        return err;
}


/*
 * XXX: not implemented.  This is not allowed yet.
 * Should we call this on the lower_sb?  Probably not.
 */
STATIC int
base0fs_remount_fs(super_block_t *sb, int *flags, char *data)
{
        return -ENOSYS;
}


/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
STATIC void
base0fs_clear_inode(inode_t *inode)
{

        print_entry_location();

        fist_checkinode(inode, "base0fs_clear_inode IN");
        /*
         * Decrement a reference to a lower_inode, which was incremented
         * by our read_inode when it was created initially.
         */
        iput(INODE_TO_LOWER(inode));
        // XXX: why this assertion fails?
        // because it doesn't like us
        // BUG_ON((inode->i_state & I_DIRTY) != 0);

        print_exit_location();
}


/*
 * Called in do_umount() if the MNT_FORCE flag was used and this
 * function is defined.  See comment in linux/fs/super.c:do_umount().
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 *
 * PS. I wonder if this is somehow useful to undo damage that was
 * left in the kernel after a user level file server (such as amd)
 * dies.
 */

#ifndef HAVE_2_ARG_UMOUNT_BEGIN
STATIC void
base0fs_umount_begin(super_block_t *sb)
{
        super_block_t *lower_sb;

        print_entry_location();

        lower_sb = SUPERBLOCK_TO_LOWER(sb);

        if (lower_sb->s_op->umount_begin)
                lower_sb->s_op->umount_begin(lower_sb);

        print_exit_location();
}
#else /* HAVE_2_ARG_UMOUNT_BEGIN */
STATIC void
base0fs_umount_begin(struct vfsmount *vfsmnt, int flags)
{
        super_block_t *sb;
        super_block_t *lower_sb;
        struct vfsmount *lower_vfsmnt;

        print_entry_location();

        sb = vfsmnt->mnt_sb;
        lower_sb = SUPERBLOCK_TO_LOWER(sb);
        lower_vfsmnt = DENTRY_TO_LVFSMNT(sb->s_root);

        if (lower_sb->s_op->umount_begin)
                lower_sb->s_op->umount_begin(lower_vfsmnt, flags);

        print_exit_location();
}
#endif /* HAVE_2_ARG_UMOUNT_BEGIN */

/* Called to print options in /proc/mounts */
static int base0fs_show_options(struct seq_file *m, struct vfsmount *mnt) {
        struct super_block *sb = mnt->mnt_sb;
        int ret = 0;
        unsigned long tmp = 0;
        char *path;
        struct dentry *lower_dentry = DENTRY_TO_LOWER(sb->s_root);
        struct vfsmount *lower_mnt  = DENTRY_TO_LVFSMNT(sb->s_root);

        tmp = __get_free_page(GFP_KERNEL);
        if (!tmp) {
                ret = -ENOMEM;
                goto out;
        }

        D_PATH(path, lower_dentry, lower_mnt, (char *) tmp, PAGE_SIZE);

        seq_printf(m, ",dir=%s", path);
        seq_printf(m, ",debug=%d", fist_get_debug_value());

out:
        if (tmp) {
                free_page(tmp);
        }
        return ret;
}

/* declared as extern in base0fs.h */
KMEM_CACHE_T *base0fs_inode_cachep;

static struct inode* base0fs_alloc_inode(struct super_block* sb)
{
        struct base0fs_inode_info *wi;
        wi = kmem_cache_alloc(base0fs_inode_cachep, GFP_KERNEL);
        if (!wi)
                return NULL;
        wi->vfs_inode.i_version = 1;
        return &wi->vfs_inode;
}

static void base0fs_destroy_inode(struct inode *inode)
{
        kmem_cache_free(base0fs_inode_cachep, BASE0FS_I(inode));
}

#ifdef HAVE_CLEANUP_IN_KMEM_CACHE_CREATE /* Implies 3-arg version of init. */
static void init_once(void * foo, KMEM_CACHE_T * cachep, unsigned long flags)
{
        struct base0fs_inode_info *wi = foo;

#if defined(SLAB_CTOR_VERIFY) && defined(SLAB_CTOR_CONSTRUCTOR)
        if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
            SLAB_CTOR_CONSTRUCTOR) {
                inode_init_once(&wi->vfs_inode);
        }
#else
	inode_init_once(&wi->vfs_inode);
#endif
}
#else /* !HAVE_CLEANUP_IN_KMEM_CACHE_CREATE */
static void init_once(
#ifdef HAVE_KMEM_CACHE_IN_KMEM_CACHE_CREATE_INIT
    KMEM_CACHE_T *cachep,
#endif
    void * foo)
{
        struct base0fs_inode_info *wi = foo;
	inode_init_once(&wi->vfs_inode);
}
#endif /* !HAVE_CLEANUP_IN_KMEM_CACHE_CREATE */

int base0fs_init_inodecache(void)
{
    unsigned long flags = SLAB_RECLAIM_ACCOUNT;
#ifdef SLAB_MEM_SPREAD
    flags |= SLAB_MEM_SPREAD;
#endif

    base0fs_inode_cachep = kmem_cache_create("base0fs_inode_cache",
            sizeof(struct base0fs_inode_info),
            0, flags,
            init_once 
#ifdef HAVE_CLEANUP_IN_KMEM_CACHE_CREATE
            , NULL
#endif
        );

        if (base0fs_inode_cachep == NULL)
                return -ENOMEM;
        return 0;
}

void base0fs_destroy_inodecache(void)
{
        kmem_cache_destroy(base0fs_inode_cachep);
}

struct super_operations base0fs_sops =
{
        alloc_inode:    base0fs_alloc_inode,
        destroy_inode:  base0fs_destroy_inode,
#if defined(FIST_DEBUG) || defined(FIST_FILTER_SCA)
        write_inode:    base0fs_write_inode,
#endif /* defined(FIST_DEBUG) || defined(FIST_FILTER_SCA) */
#if defined(HAVE_PUT_INODE)
        put_inode:      base0fs_put_inode,
#endif /* defined HAVE_PUT_INODE */
#if defined(FIST_DEBUG) || defined(FIST_FILTER_SCA)
        delete_inode:   base0fs_delete_inode,
#endif /* defined(FIST_DEBUG) || defined(FIST_FILTER_SCA) */
        put_super:      base0fs_put_super,
        statfs:         base0fs_statfs,
        remount_fs:     base0fs_remount_fs,
        clear_inode:    base0fs_clear_inode,
        umount_begin:   base0fs_umount_begin,
        show_options:   base0fs_show_options,
};

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */