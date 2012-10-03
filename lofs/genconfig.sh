#!/bin/sh

# genconfig.sh
#
#       This script determines what features of the Linux kernel are or are
#       not present, and how to reference those features, in the same way that
#       a classic autoconf generated configure script does:  by constructing
#       simple test cases and trying to build them.  The end output is the file
#       kernel_config.h, which contains several #defines that are referenced
#       in the source code as needed.

KERNELDIR=/lib/modules/`uname -r`/build
PWD=`pwd`
HEADER=kernel_config.h
TESTDIR=__conftest__
LOG=config.log

log()
{
    echo $1 $2
    echo ""    >> $LOG
    echo $1 $2 >> $LOG
}

AddCheck() {
    rm -f $TESTDIR/$1.c $TESTDIR/$1.o
    echo "conftest-objs += $1.o" >> $TESTDIR/Makefile
    echo -n "$2" > $TESTDIR/$1.c
}

rm -rf $TESTDIR $HEADER $LOG
mkdir  $TESTDIR

# Make an empty kernel_config.h, in case none of the tests below cause it to
# be created.

echo "" > $HEADER

# Make a trivial makefile that allows us to integrate with the kernel build
# system.

cat >> $TESTDIR/Makefile << _EOF
conftest-objs := 
obj-m := conftest.o
EXTRA_CFLAGS += -Werror
_EOF

###############################################################################
# Check for fsstack_copy_attr_all

AddCheck fsstack_copy_attr_all_3 "
#include <linux/fs_stack.h>
void dummy(void)
{
    struct inode *dest = 0;
    struct inode *src = 0;
    fsstack_copy_attr_all(dest, src, NULL);
    return;
}
"

AddCheck fsstack_copy_attr_all_2 "
#include <linux/fs_stack.h>
void dummy(void)
{
    struct inode *dest = 0;
    struct inode *src = 0;
    fsstack_copy_attr_all(dest, src);
    return;
}
"

###############################################################################
# Check call signature for posix_lock_file

AddCheck posix_lock_file "
#include <linux/fs.h>
void dummy(void)
{
    (void) posix_lock_file(0, 0, 0);
}
"

###############################################################################
# Check call signature for posix_test_lock

AddCheck posix_test_lock_3 "
#include <linux/fs.h>
void dummy(void)
{
     int (*ptl)(struct file *, struct file_lock *, struct file_lock *) = posix_test_lock;
     (*ptl)(0, 0, 0);
}
"

AddCheck posix_test_lock_2 "
#include <linux/fs.h>
void dummy(void)
{
     int (*ptl)(struct file *, struct file_lock *) = posix_test_lock;
     (*ptl)(0, 0);
}
"

##############################################################################
# Check if struct nameidata uses struct path.

AddCheck nameidata "
#include <linux/namei.h>
struct vfsmount *dummy(void)
{
    struct nameidata tmp;
    tmp.path.mnt = 0;
    return tmp.path.mnt;
}
"

###############################################################################
# Check notify_change call signature.

AddCheck notify_change "
#include <linux/fs.h>
#include <linux/mount.h>
int dummy(void)
{
    struct dentry d;
    struct vfsmount r;
    struct iattr ia;
    return notify_change(&d, &r, &ia);
}
"

###############################################################################
# Check for kmem_cache_t.

AddCheck kmem_cache_t "
#include <linux/slab.h>
kmem_cache_t *dummy(void)
{
    return 0;
}
"

###############################################################################
# Check the call signature of vfs_link.  We use this as a proxy for detecting
# whether the AppArmor security model has been patched into the kernel, which
# requires additional parameters for the vfs_* family of functions.

AddCheck apparmor "
#include <linux/fs.h>
int dummy(void)
{
    return vfs_link(0, 0, 0, 0, 0);
}
"

###########################################################################
# Check the call signature of vfs_symlink.  Some versions of Linux have an
# extra "mode" parameter.  This is the AppArmor-enabled version.

AddCheck apparmor_vfs_symlink "
#include <linux/fs.h>
int dummy(void)
{
    struct inode *i = 0;
    struct dentry *d = 0;
    const char *c = 0;
    int mode = 0;
    struct vfsmount *m = 0;
    return vfs_symlink(i, d, m, c, mode);
}
"

###########################################################################
# Check the call signature of vfs_symlink.  Some versions of Linux have an
# extra "mode" parameter.  This is the non-AppArmor version.

AddCheck vfs_symlink "
#include <linux/fs.h>
int dummy(void)
{
    struct inode *i = 0;
    struct dentry *d = 0;
    const char *c = 0;
    int mode = 0;
    return vfs_symlink(i, d, c, mode);
}
"

###############################################################################
# Check inode_operations.permission call signature.  This is complex, because
# it has gone through many variations over the years.

AddCheck permission_nameidata "
#include <linux/fs.h>
int dummy(struct inode_operations *iop)
{
    return iop->permission(0, 0, (struct nameidata *) 0);
}
"

AddCheck permission_flags "
#include <linux/fs.h>
int dummy(struct inode_operations *iop)
{
    return iop->permission(0, 0, (int) 0);
}
"

###############################################################################
# Check kmem_cache_create syntax.

AddCheck kmem_cache_create "
#include <linux/slab.h>
void dummy(void)
{
    kmem_cache_create(0, 0, 0, 0, 0, 0);
    return;
}
"

##############################################################################
# Check whether the init param to kmem_cache_create takes one or two args.

AddCheck kmem_cache_create_init "
#include <linux/slab.h>
void dummy(void)
{
    void (*init_once)(struct kmem_cache *, void *) = 0;
    kmem_cache_create(0, 0, 0, 0, init_once);
    return;
}
"

###############################################################################
# Check dentry_open syntax.

AddCheck dentry_open "
#include <linux/fs.h>
void dummy(void)
{
    dentry_open(0, 0, 0, 0);
}
"

###############################################################################
# Check for struct path.

AddCheck struct_path "
#include <linux/path.h>
void dummy(struct path *p)
{
    p->mnt = 0;
    p->dentry = 0;
    return;
}
"

###############################################################################
# Check follow_down syntax.

AddFollowDownCheck() {
    # args: name, use path, follow_down call

    EXTRA_INCLUDES=
    if [ $2 -ne 0 ] ; then
        EXTRA_INCLUDES="#include <linux/path.h>"
    fi
    BODY="
#include <linux/dcache.h>
#include <linux/namei.h>
$EXTRA_INCLUDES
int dummy(void)
{
    return $3;
}
"
    AddCheck $1 "$BODY"
}
AddFollowDownCheck old_follow_down_backport 0 \
    "__follow_down((struct vfsmount **) 0, (struct dentry **) 0, 0)"
AddFollowDownCheck new_follow_down_backport 1 \
    "__follow_down((struct path *) 0, 0)"
AddFollowDownCheck old_follow_down 0 \
    "follow_down((struct vfsmount **) 0, (struct dentry **) 0)"
AddFollowDownCheck follow_down_bool 1 \
    "follow_down((struct path *) 0, 0)"

##############################################################################
# Check whether address_space_operations has prepare_write/commit_write or
# write_begin/write_end.

AddCheck write_begin "
#include <linux/fs.h>
int dummy(void)
{
    struct address_space_operations aops;
    aops.write_begin = 0;
    return aops.write_begin == 0;
}
"

##############################################################################
# Check for RHEL5 backport of write_begin/write_end API.

AddCheck write_begin_backport "
#include <linux/fs.h>
int dummy(void)
{
    struct address_space_operations_ext aops;
    aops.write_begin = 0;
    return aops.write_begin == 0;
}
"

##############################################################################
# Check if struct file uses struct path.

AddCheck file_has_path "
#include <linux/fs.h>
struct vfsmount *dummy(void)
{
    struct file tmp;
    tmp.path.mnt = 0;
    return tmp.path.mnt;
}
"

###############################################################################
# Check for vfs_fsync_range.

AddCheck vfs_fsync_range "
#include <linux/fs.h>
void dummy(void)
{
    vfs_fsync_range((struct file *) 0, (loff_t) 0, (loff_t) 0, 0);
    return;
}
"

###############################################################################
# Check for vfs_fsync

AddCheck vfs_fsync "
#include <linux/fs.h>
void dummy(struct file_operations *tmp)
{
    tmp->fsync = vfs_fsync;
    return;
}
"

###############################################################################
# Check fsync syntax.  This is complex because it's gone through multiple
# variations over the years.

AddCheck fsync_dentry "
#include <linux/fs.h>
void dummy(void)
{
    struct file_operations tmp;
    tmp.fsync = 0;
    tmp.fsync((struct file *) 0, (struct dentry *) 0, 0);
}
"

AddCheck fsync_offsets "
#include <linux/fs.h>
void dummy(void)
{
    struct file_operations tmp;
    tmp.fsync = 0;
    tmp.fsync((struct file *) 0, (loff_t) 0, (loff_t) 0, 0);
}
"

##############################################################################
# Check the vfs_statfs call signature.  It may be
#       vfs_statfs(struct super_block *, struct statfs*)
#       vfs_statfs(struct dentry *, struct statfs *)
#       vfs_statfs(struct path *, struct statfs *)

AddCheck vfs_statfs_super "
#include <linux/fs.h>
int dummy(void)
{
    return vfs_statfs((struct super_block *) 0, 0);
}
"

AddCheck vfs_statfs_dentry "
#include <linux/fs.h>
int dummy(void)
{
    return vfs_statfs((struct dentry *) 0, 0);
}
"

##############################################################################
# Check the file_operations.flush call signature.  Older kernels pass have just
# one parameter; newer have two.

AddCheck flush_has_owner "
#include <linux/fs.h>
int dummy(void)
{
    struct file_operations fop;
    fop.flush = 0;
    return fop.flush((struct file *) 0, (fl_owner_t) 0);
}
"

###############################################################################
# Check for generic_file_read or do_sync_read

AddCheck do_sync_read "
#include <linux/fs.h>
#include <linux/fs_struct.h>
ssize_t dummy(void)
{
    struct file f;
    char *c = 0;
    loff_t o = 0;
    return do_sync_read(&f, c, 0, &o);
}
"

###############################################################################
# Check for generic_file_write or do_sync_write

AddCheck do_sync_write "
#include <linux/fs.h>
#include <linux/fs_struct.h>
ssize_t dummy(void)
{
    struct file f;
    char *c = 0;
    loff_t o = 0;
    return do_sync_write(&f, c, 0, &o);
}
"

###############################################################################
# Check for file_operations.aio_read.

AddCheck aio_read "
#include <linux/fs.h>
#include <linux/fs_struct.h>
void dummy(void)
{
    struct file_operations fop;
    fop.aio_read = 0;
    return;
}
"

###############################################################################
# Check for file_operations.aio_write.

AddCheck aio_write "
#include <linux/fs.h>
#include <linux/fs_struct.h>
void dummy(void)
{
    struct file_operations fop;
    fop.aio_write = 0;
    return;
}
"

##############################################################################
# Check if struct file uses struct path.

AddCheck file_has_path "
#include <linux/fs.h>
void dummy(struct file *f)
{
    f.f_path.dentry = 0;
    f.f_path.mnt    = 0;
}
"

###############################################################################
# Check for inode.i_mutex

AddCheck i_mutex "
#include <linux/fs.h>
void dummy(struct inode *i)
{
    mutex_lock(&i->i_mutex);
    mutex_unlock(&i->i_mutex);
}
"

##############################################################################
# Check deactivate_locked_super.

AddCheck deactive_locked_super "
#include <linux/fs.h>
void dummy(struct super_block *sb)
{
    deactivate_locked_super(sb);
}
"

##############################################################################
# Check whether super_operations has clear_inode or evict_inode.

AddCheck evict_inode "
#include <linux/fs.h>
void dummy(struct super_operations *sop)
{
    sop->evict_inode(0);
    return;
}
"

##############################################################################
# Check whether file_operations has the old ioctl interface.

AddCheck locked_ioctl "
#include <linux/fs.h>
void dummy(struct file_operations *fop)
{
    fop->ioctl = 0;
}
"

##############################################################################
# Check whether file_system_type has the old get_sb interface.

AddCheck get_sb "
#include <linux/fs.h>
void dummy(struct file_system_type *fs)
{
    fs->get_sb = 0;
    return;
}
"

##############################################################################
# Check whether super_block has s_d_op.

AddCheck s_d_op "
#include <linux/fs.h>
void dummy(struct super_block *sb)
{
    sb->s_d_op = 0;
}
"

##############################################################################
# Check whether file_system_type has the mount field.

AddCheck mount "
#include <linux/fs.h>
void dummy(struct file_system_type *fs)
{
    fs->mount = 0;
}
"

##############################################################################
# Check for the BKL.

AddCheck bkl "
#include <linux/smp_lock.h>
void dummy(int *i)
{
    lock_kernel();
    *i = 0;
    unlock_kernel();
}
"

##############################################################################
# Check whether super_block has the s_bdi field.

AddCheck s_bdi "
#include <linux/fs.h>
#include <linux/backing-dev.h>
void dummy(struct super_block *sb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#error \"Backing dev info does not work in 2.6 kernels.\"
#endif
    struct backing_dev_info bdi;
    bdi_setup_and_register(&bdi, "abc", BDI_CAP_MAP_COPY);
    sb->s_bdi = &bdi;
    return;
}
"

##############################################################################
# Check whether set_nlink is present.

AddCheck set_nlink "
#include <linux/fs.h>
void dummy(struct inode *i)
{
    set_nlink(i, 0);
}
"

##############################################################################
# Check whether filemap_write_and_wait is present.

AddCheck filemap_write_and_wait "
#include <linux/fs.h>
int dummy(struct address_space *as)
{
    return filemap_write_and_wait(as);
}
"

###############################################################################
# Check for use of umode_t in inode_operations.create.

AddCheck umode_in_create "
#include <linux/fs.h>
int mycreate(struct inode *i, struct dentry *d, umode_t m, struct nameidata *n)
{
    return 0;
}

int dummy(struct inode_operations *iop)
{
    iop->create = mycreate;
}
"

###############################################################################
# Check for the name of end_writeback().  It's either that, or clear_inode().

AddCheck end_writeback "
#include <linux/fs.h>
void dummy(struct inode *i)
{
    end_writeback(i);
}
"

###############################################################################
# Check for d_make_root.

AddCheck d_make_root "
#include <linux/fs.h>
struct dentry *dummy(void)
{
    return d_make_root((struct inode *) 0);
}
"

##############################################################################
# Check for freezer.h.

AddCheck freezer_h "
#include <linux/freezer.h>
void dummy(int *d)
{
    *d = 0;
}
"

##############################################################################
# Check for kern_path().

AddCheck kern_path "
#include <linux/namei.h>
int dummy(void)
{
    return kern_path((const char *) 0, 0, (struct path *) 0);
}
"

##############################################################################
# Determine whether or not the kernel supports "managed" dentries.

AddCheck managed_dentries "
#include <linux/dcache.h>
int dummy(void)
{
#if defined(DCACHE_MANAGED_DENTRY) || defined(DMANAGED_MOUNTPOINT)
    return 0;
#else
#error \"No managed dentries\"
#endif
}
"

# Run all the tests in bulk.  This is faster than running "make" for each
# individual test, because the kernel build system has substantial startup
# overhead.

make -C $KERNELDIR M=$PWD/$TESTDIR -k -j 4 modules >> $LOG 2>&1

log -n "Checking for fsstack_copy_attr_all... "
RESULT="not present."
if [ -f $TESTDIR/fsstack_copy_attr_all_3.o ] ; then
    echo "#define HAVE_FSSTACK_COPY_ATTR_ALL_3_ARG" >> $HEADER
    RESULT="3 args."
elif [ -f $TESTDIR/fsstack_copy_attr_all_2.o ] ; then
    echo "#define HAVE_FSSTACK_COPY_ATTR_ALL_2_ARG" >> $HEADER
    RESULT="2 args."
fi
log $RESULT

log -n "Checking posix_lock_file... "
RESULT="2 args."
if [ -f $TESTDIR/posix_lock_file.o ] ; then
    echo "#define HAVE_3_ARG_POSIX_LOCK_FILE" >> $HEADER
    RESULT="3 args."
fi
log $RESULT

log -n "Checking posix_test_lock... "
RESULT="2 args, returning void."
if [ -f $TESTDIR/posix_test_lock_3.o ] ; then
    echo "#define HAVE_3_ARG_INT_POSIX_TEST_LOCK" >> $HEADER
    RESULT="3 args, returning int."
elif [ -f $TESTDIR/posix_test_lock_2.o ] ; then 
    echo "#define HAVE_2_ARG_INT_POSIX_TEST_LOCK" >> $HEADER
    RESULT="2 args, returning int."
fi
log $RESULT

log -n "Checking whether nameidata uses struct path... "
RESULT="no."
if [ -f $TESTDIR/nameidata.o ] ; then
    echo "#define NAMEIDATA_USES_STRUCT_PATH" >> $HEADER
    RESULT="yes."
fi
log $RESULT

log -n "Checking notify_change signature... "
RESULT="2 args."
if [ -f $TESTDIR/notify_change.o ] ; then
    echo "#define HAVE_3_ARG_NOTIFY_CHANGE" >> $HEADER
    RESULT="3 args."
fi
log $RESULT

log -n "Checking for kmem_cache_t... "
RESULT="not defined."
if [ -f $TESTDIR/kmem_cache_t.o ] ; then
    echo "#define HAVE_KMEM_CACHE_T" >> $HEADER
    RESULT="defined."
fi
log $RESULT

log -n "Checking for AppArmor security module... "
if [ -f $TESTDIR/apparmor.o ] ; then
    echo "#define HAVE_APP_ARMOR_SECURITY" >> $HEADER
    log "present."
    log -n "Checking vfs_symlink call signature... "
    RESULT="does not have mode."
    if [ -f $TESTDIR/apparmor_vfs_symlink.o ] ; then
        echo "#define HAVE_MODE_IN_VFS_SYMLINK" >> $HEADER
        RESULT="has mode."
    fi
    log $RESULT
else
    log "not present."
    log -n "Checking vfs_symlink call signature... "
    RESULT="does not have mode."
    if [ -f $TESTDIR/vfs_symlink.o ] ; then
        echo "#define HAVE_MODE_IN_VFS_SYMLINK" >> $HEADER
        RESULT="has mode."
    fi
    log $RESULT
fi

log -n "Checking inode_operations.permission signature... "
RESULT="simple"
if [ -f $TESTDIR/permission_nameidata.o ] ; then
    echo "#define HAVE_NAMEIDATA_IN_PERMISSION" >> $HEADER
    RESULT="has nameidata."
elif [ -f $TESTDIR/permission_flags.o ] ; then
    echo "#define HAVE_FLAGS_IN_PERMISSION" >> $HEADER
    RESULT="has flags."
fi
log $RESULT


log -n "Checking kmem_cache_create syntax... "
if [ -f $TESTDIR/kmem_cache_create.o ] ; then
    echo "#define HAVE_CLEANUP_IN_KMEM_CACHE_CREATE" >> $HEADER
    log "has cleanup."
else
    log "no cleanup."
    log -n "Checking kmem_cache_create init syntax... "
    RESULT="no kmem_cache param."
    if [ -f $TESTDIR/kmem_cache_create_init.o ] ; then
        echo "#define HAVE_KMEM_CACHE_IN_KMEM_CACHE_CREATE_INIT" >> $HEADER
        RESULT="has kmem_cache param."
    fi
    log $RESULT
fi

log -n "Checking dentry_open syntax... "
RESULT="3 args."
if [ -f $TESTDIR/dentry_open.o ] ; then
    echo "#define HAVE_CRED_IN_DENTRY_OPEN" >> $HEADER
    RESULT="4 args."
fi
log $RESULT

log -n "Checking for struct path... "
RESULT="not present."
if [ -f $TESTDIR/struct_path.o ] ; then
    echo "#define HAVE_STRUCT_PATH" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking follow_down syntax... "
if [ -f $TESTDIR/old_follow_down_backport.o ] ; then
    log "vfsmount/dentry backport."
    echo "#define HAVE_OLD_FOLLOW_DOWN_BACKPORT" >> $HEADER
elif [ -f $TESTDIR/follow_down_backport.o ] ; then
    log "path/bool backport."
    echo "#define HAVE_FOLLOW_DOWN_BACKPORT" >> $HEADER
elif [ -f $TESTDIR/old_follow_down.o ] ; then
    log "vfsmount/dentry."
    echo "#define HAVE_OLD_FOLLOW_DOWN" >> $HEADER
elif [ -f $TESTDIR/follow_down_bool.o ] ; then
    log "path/bool."
    echo "#define HAVE_BOOL_IN_FOLLOW_DOWN" >> $HEADER
else
    log "path."
fi

log -n "Checking address_space_operations... "
RESULT="prepare_write/commit_write."
if [ -f $TESTDIR/write_begin.o ] ; then
    echo "#define HAVE_WRITE_BEGIN 1" >> $HEADER
    RESULT="write_begin/write_end."
elif [ -f $TESTDIR/write_begin_backport.o ] ; then
    echo "#define HAVE_WRITE_BEGIN 1" >> $HEADER
    echo "#define HAVE_ADDRESS_SPACE_OPS_EXT 1" >> $HEADER
    RESULT="write_begin/write_end backport."
fi
log $RESULT

log -n "Checking whether file uses struct path... "
RESULT="no."
if [ -f $TESTDIR/file_has_path.o ] ; then
    echo "#define HAVE_PATH_IN_STRUCT_FILE" >> $HEADER
    RESULT="yes."
fi
log $RESULT

log -n "Checking for vfs_fsync_range or vfs_fsync... "
RESULT="not present."
if [ -f $TESTDIR/vfs_fsync_range.o ] ; then
    echo "#define HAVE_VFS_FSYNC_RANGE" >> $HEADER
    RESULT="have vfs_fsync_range."
elif [ -f $TESTDIR/vfs_fsync.o ] ; then
    echo "#define HAVE_VFS_FSYNC" >> $HEADER
    RESULT="have vfs_fsync."
fi
log $RESULT

log -n "Checking file_operations.fsync syntax... "
RESULT="simple."
if [ -f $TESTDIR/fsync_dentry.o ] ; then
    echo "#define HAVE_DENTRY_IN_FSYNC" >> $HEADER
    RESULT="has dentry."
elif [ -f $TESTDIR/fsync_offsets.o ] ; then
    echo "#define HAVE_OFFSET_IN_FSYNC" >> $HEADER
    RESULT="has offsets."
fi
log $RESULT

log -n "Checking vfs_statfs signature..."
if [ -f $TESTDIR/vfs_statfs_super.o ] ; then
    echo "#define STATFS_HAS_SUPER_BLOCK" >> $HEADER
    RESULT="struct super_block."
elif [ -f $TESTDIR/vfs_statfs_dentry.o ] ; then
    echo "#define STATFS_HAS_DENTRY" >> $HEADER
    RESULT="struct dentry."
else
    echo "#define STATFS_HAS_PATH" >> $HEADER
    RESULT="struct path."
fi
log $RESULT

log -n "Checking file_operations.flush signature..."
RESULT="no owner parameter."
if [ -f $TESTDIR/flush_has_owner.o ] ; then
    echo "#define FLUSH_HAS_OWNER 1" >> $HEADER
    RESULT="has owner parameter."
fi
log $RESULT

log -n "Checking for vfs read helper... "
RESULT="generic_file_read."
if [ -f $TESTDIR/do_sync_read.o ] ; then
    echo "#define HAVE_DO_SYNC_READ" >> $HEADER
    RESULT="do_sync_read."
fi
log $RESULT

log -n "Checking for vfs write helper... "
RESULT="generic_file_write."
if [ -f $TESTDIR/do_sync_write.o ] ; then
    echo "#define HAVE_DO_SYNC_WRITE" >> $HEADER
    RESULT="do_sync_write."
fi
log $RESULT

log -n "Checking for file_operations.aio_read... "
RESULT=no.
if [ -f $TESTDIR/aio_read.o ] ; then
    echo "#define HAVE_FILE_OPERATIONS_AIO_READ" >> $HEADER
    RESULT=yes.
fi
log $RESULT

log -n "Checking for file_operations.aio_write... "
RESULT=no.
if [ -f $TESTDIR/aio_write.o ] ; then
    echo "#define HAVE_FILE_OPERATIONS_AIO_WRITE" >> $HEADER
    RESULT=yes.
fi
log $RESULT

log -n "Checking whether file uses struct path... "
RESULT="no."
if [ -f $TESTDIR/file_has_path.o ] ; then
    echo "#define FILE_USES_STRUCT_PATH" >> $HEADER
    RESULT="yes."
fi
log $RESULT

log -n "Checking inode lock member name... "
RESULT="i_sem."
if [ -f $TESTDIR/i_mutex.o ] ; then
    echo "#define HAVE_INODE_I_MUTEX" >> $HEADER
    RESULT="i_mutex."
fi
log $RESULT

log -n "Checking for deactivate_locked_super..."
RESULT="not present."
if [ -f $TESTDIR/deactive_locked_super.o ] ; then
    echo "#define HAVE_DEACTIVATE_LOCKED_SUPER" >> $HEADER
    RESULT="yes."
fi
log $RESULT

log -n "Checking for super_operations.evict_inode..."
RESULT="not present."
if [ -f $TESTDIR/evict_inode.o ] ; then
    echo "#define HAVE_EVICT_INODE" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for file_operations.ioctl..."
RESULT="not present."
if [ -f $TESTDIR/locked_ioctl.o ] ; then
    echo "#define HAVE_LOCKED_IOCTL" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for file_system_type.get_sb..."
RESULT="not present."
if [ -f $TESTDIR/get_sb.o ] ; then
    echo "#define HAVE_GET_SB" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for super_block.s_d_op..."
RESULT="not present."
if [ -f $TESTDIR/s_d_op.o ] ; then
    echo "#define HAVE_S_D_OP" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for file_system_type.mount..."
RESULT="not present."
if [ -f $TESTDIR/mount.o ] ; then
    echo "#define HAVE_MOUNT_IN_FS_TYPE" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for the big kernel lock..."
RESULT="not present."
if [ -f $TESTDIR/bkl.o ] ; then
    echo "#define HAVE_BIG_KERNEL_LOCK" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for super_block.s_bdi and bdi_setup_and_register..."
RESULT="not present."
if [ -f $TESTDIR/s_bdi.o ] ; then
    echo "#define HAVE_BACKING_DEV" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for set_nlink..."
RESULT="not present."
if [ -f $TESTDIR/set_nlink.o ] ; then
    echo "#define HAVE_SET_NLINK" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for filemap_write_and_wait..."
RESULT="not present."
if [ -f $TESTDIR/filemap_write_and_wait.o ] ; then
    echo "#define HAVE_FILEMAP_WRITE_AND_WAIT" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking inode_operations.create signature... "
RESULT="uses int."
if [ -f $TESTDIR/umode_in_create.o ] ; then
    echo "#define HAVE_UMODE_IN_CREATE" >> $HEADER
    RESULT="uses umode_t."
fi
log $RESULT

log -n "Checking for end_writeback... "
RESULT="not present."
if [ -f $TESTDIR/end_writeback.o ] ; then
    echo "#define HAVE_END_WRITEBACK" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for d_make_root... "
RESULT="not present."
if [ -f $TESTDIR/d_make_root.o ] ; then
    echo "#define HAVE_D_MAKE_ROOT" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for freezer.h... "
RESULT="not present."
if [ -f $TESTDIR/freezer_h.o ] ; then
    echo "#define HAVE_FREEZER_H" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for kern_path... "
RESULT="not present."
if [ -f $TESTDIR/kern_path.o ] ; then
    echo "#define HAVE_KERN_PATH" >> $HEADER
    RESULT="present."
fi
log $RESULT

log -n "Checking for managed dentries... "
RESULT="not present."
if [ -f $TESTDIR/managed_dentries.o ] ; then
    echo "#define HAVE_MANAGED_DENTRIES" >> $HEADER
    RESULT="present."
fi
log $RESULT

####################
# FINAL CLEANUP

rm -rf $PWD/$TESTDIR
