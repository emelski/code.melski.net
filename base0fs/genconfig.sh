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

rm -rf $TESTDIR $HEADER
mkdir  $TESTDIR

# Make an empty kernel_config.h, in case none of the tests below cause it to
# be created.

echo "" > $HEADER

# Make a trivial makefile that allows us to integrate with the kernel build
# system.

cat >> $TESTDIR/Makefile << _EOF
conftest-objs := dummy.o
obj-m := conftest.o
EXTRA_CFLAGS += -Werror
_EOF

###############################################################################
# Check for struct inode.i_blksize

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c << _EOF
#include <linux/fs.h>
void dummy(void)
{
    struct inode i;
    i.i_blksize = 0;
    return;
}
_EOF

echo -n "Checking for inode.i_blksize... "
RESULT="yes."
if ! make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_NO_INODE_I_BLKSIZE" >> $HEADER
    RESULT="no."
fi
echo $RESULT

###############################################################################
# Check call signature for posix_lock_file

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c << _EOF
#include <linux/fs.h>
void dummy(void)
{
    (void) posix_lock_file(0, 0);
}
_EOF

echo -n "Checking posix_lock_file... "
RESULT="2 args."
if ! make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_3_ARG_POSIX_LOCK_FILE" >> $HEADER
    RESULT="3 args."
fi
echo $RESULT

###############################################################################
# Check call signature for posix_lock_file
rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c << _EOF
#include <linux/fs.h>
void dummy(void)
{
     int (*ptl)(struct file *, struct file_lock *, struct file_lock *) = posix_test_lock;
     (*ptl)(0, 0, 0);
}
_EOF
echo -n "Checking posix_test_lock... "
RESULT="2 args, returning void."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_3_ARG_INT_POSIX_TEST_LOCK" >> $HEADER
    RESULT="3 args, returning int."
else
    rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
    cat >> $TESTDIR/dummy.c << _EOF
#include <linux/fs.h>
void dummy(void)
{
     int (*ptl)(struct file *, struct file_lock *) = posix_test_lock;
     (*ptl)(0, 0);
}
_EOF
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_2_ARG_INT_POSIX_TEST_LOCK" >> $HEADER
        RESULT="2 args, returning int."
    fi
fi
echo $RESULT

##############################################################################
# Check if struct nameidata uses struct path.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/namei.h>
struct vfsmount *dummy(void)
{
    struct nameidata tmp;
    tmp.path.mnt = 0;
    return tmp.path.mnt;
}
_EOF

echo -n "Checking whether nameidata uses struct path... "
RESULT="no."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define NAMEIDATA_USES_STRUCT_PATH" >> $HEADER
    RESULT="yes."
fi
echo $RESULT

###############################################################################
# Check notify_change call signature.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
#include <linux/mount.h>
int dummy(void)
{
    struct dentry d;
    struct vfsmount r;
    struct iattr ia;
    return notify_change(&d, &r, &ia);
}
_EOF

echo -n "Checking notify_change signature... "
RESULT="2 args."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_3_ARG_NOTIFY_CHANGE" >> $HEADER
    RESULT="3 args."
fi
echo $RESULT

###############################################################################
# Check for kmem_cache_t.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/slab.h>
kmem_cache_t *dummy(void)
{
    return 0;
}
_EOF

echo -n "Checking for kmem_cache_t... "
RESULT="not defined."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_KMEM_CACHE_T" >> $HEADER
    RESULT="defined."
fi
echo $RESULT

###############################################################################
# Check the call signature of vfs_link.  We use this as a proxy for detecting
# whether the AppArmor security model has been patched into the kernel, which
# requires additional parameters for the vfs_* family of functions.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    return vfs_link(0, 0, 0, 0, 0);
}
_EOF

echo -n "Checking for AppArmor security module... "
RESULT="not present."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_APP_ARMOR_SECURITY" >> $HEADER
    echo "present."

    ###########################################################################
    # Check the call signature of vfs_symlink.  Some versions of Linux have an
    # extra "mode" parameter.

    rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
    cat >> $TESTDIR/dummy.c <<_EOF
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
_EOF

    echo -n "Checking vfs_symlink call signature... "
    RESULT="does not have mode."
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_MODE_IN_VFS_SYMLINK" >> $HEADER
        RESULT="has mode."
    fi
    echo $RESULT

else
    echo "not present."
    
    ###########################################################################
    # Check the call signature of vfs_symlink.  Some versions of Linux have an
    # extra "mode" parameter.

    rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
    cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct inode *i = 0;
    struct dentry *d = 0;
    const char *c = 0;
    int mode = 0;
    return vfs_symlink(i, d, c, mode);
}
_EOF

    echo -n "Checking vfs_symlink call signature... "
    RESULT="does not have mode."
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_MODE_IN_VFS_SYMLINK" >> $HEADER
        RESULT="has mode."
    fi
    echo $RESULT
fi

###############################################################################
# Figure out the call parameters for inode_operations.permission.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct inode_operations i_op;
    int (*perm)(struct inode *, int, struct nameidata *) = 0;
    i_op.permission = perm;
    return i_op.permission(0, 0, 0);
}
_EOF

echo -n "Checking inode_operations.permission signature... "
RESULT="2 args."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_3_ARG_PERMISSION" >> $HEADER
    RESULT="3 args."
fi
echo $RESULT

###############################################################################
# Figure out the call parameters for super_operations.umount_begin.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
struct super_operations *dummy(struct super_operations *s_op)
{
    void (*ub)(struct super_block *) = 0;
    s_op->umount_begin = ub;
    return s_op;
}
_EOF

echo -n "Checking super_operations.umount_begin signature... "
RESULT="1 arg."
if ! make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_2_ARG_UMOUNT_BEGIN" >> $HEADER
    RESULT="2 args."
fi
echo $RESULT

###############################################################################
# Figure out the call parameters for d_path.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/path.h>
#include <linux/dcache.h>
char *dummy(void)
{
    struct path p;
    return d_path(&p, 0, 0); 
}
_EOF

echo -n "Checking d_path signature... "
RESULT="4 args."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_PATH_IN_D_PATH" >> $HEADER
    RESULT="3 args."
fi
echo $RESULT

###############################################################################
# Check for super_operations.put_inode.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
void dummy(struct super_operations *s_op)
{
    s_op->put_inode = 0;
    return;
}
_EOF

echo -n "Checking for super_operations.put_inode... "
RESULT="not present."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_PUT_INODE" >> $HEADER
    RESULT="present."
fi
echo $RESULT

###############################################################################
# Check kmem_cache_create syntax.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/slab.h>
void dummy(void)
{
    kmem_cache_create(0, 0, 0, 0, 0, 0);
    return;
}
_EOF

echo -n "Checking kmem_cache_create syntax... "
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_CLEANUP_IN_KMEM_CACHE_CREATE" >> $HEADER
    echo "has cleanup."
else
    echo "no cleanup."

    # Now check whether the init param to kmem_cache_create takes one or
    # two args.

    echo -n "Checking kmem_cache_create init syntax... "
    rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
    cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/slab.h>
void dummy(void)
{
    void (*init_once)(struct kmem_cache *, void *) = 0;
    kmem_cache_create(0, 0, 0, 0, init_once);
    return;
}
_EOF
    RESULT="no kmem_cache param."
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_KMEM_CACHE_IN_KMEM_CACHE_CREATE_INIT" >> $HEADER
        RESULT="has kmem_cache param."
    fi
    echo $RESULT
fi

###############################################################################
# Check dentry_open syntax.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
void dummy(void)
{
    dentry_open(0, 0, 0, 0);
    return;
}
_EOF

echo -n "Checking dentry_open syntax... "
RESULT="3 args."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_CRED_IN_DENTRY_OPEN" >> $HEADER
    RESULT="4 args."
fi
echo $RESULT

###############################################################################
# Check follow_down syntax.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/dcache.h>
#include <linux/namei.h>
int dummy(void)
{
    return follow_down((struct vfsmount **) 0, (struct dentry **) 0);
}
_EOF

echo -n "Checking follow_down syntax... "
RESULT="2 arg."
if ! make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_PATH_IN_FOLLOW_DOWN" >> $HEADER
    RESULT="1 arg."
fi
echo $RESULT

##############################################################################
# Check whether address_space_operations has prepare_write/commit_write or
# write_begin/write_end.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct address_space_operations aops;
    aops.write_begin = 0;
    return aops.write_begin == 0;
}
_EOF

echo -n "Checking address_space_operations... "
RESULT="prepare_write/commit_write."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_WRITE_BEGIN 1" >> $HEADER
    RESULT="write_begin/write_end."
else
    # Check for RHEL5 backport of write_begin/write_end API.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct address_space_operations_ext aops;
    aops.write_begin = 0;
    return aops.write_begin == 0;
}
_EOF
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_WRITE_BEGIN 1" >> $HEADER
        echo "#define HAVE_ADDRESS_SPACE_OPS_EXT 1" >> $HEADER
        RESULT="write_begin/write_end backport."
    fi
fi
echo $RESULT

##############################################################################
# Check whether file_operations has sendfile.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct file_operations fops;
    fops.sendfile = 0;
    return fops.sendfile == 0;
}
_EOF

echo -n "Checking file_operations... "
RESULT="sendfile absent."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_SENDFILE 1" >> $HEADER
    RESULT="sendfile present."
fi
echo $RESULT

##############################################################################
# Check if struct file uses struct path.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
struct vfsmount *dummy(void)
{
    struct file tmp;
    tmp.path.mnt = 0;
    return tmp.path.mnt;
}
_EOF

echo -n "Checking whether file uses struct path... "
RESULT="no."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_PATH_IN_STRUCT_FILE" >> $HEADER
    RESULT="yes."
fi
echo $RESULT

###############################################################################
# Check fsync syntax.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct file_operations tmp;
    tmp.fsync = 0;
    return tmp.fsync((struct file *) 0, 0);
}
_EOF

echo -n "Checking file_operations.fsync syntax... "
RESULT="2 arg."
if ! make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_DENTRY_IN_FSYNC" >> $HEADER
    RESULT="3 arg."
fi
echo $RESULT

###############################################################################
# Check write_inode syntax.

rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct super_operations tmp;
    tmp.write_inode = 0;
    return tmp.write_inode((struct inode *) 0, (struct writeback_control *) 0);
}
_EOF

echo -n "Checking super_operations.write_inode syntax... "
RESULT="void return."
if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
    echo "#define HAVE_WRITEBACK_CONTROL_IN_WRITE_INODE" >> $HEADER
    echo "#define HAVE_WRITE_INODE_RETURNS_INT" >> $HEADER
    RESULT="writeback_control."
else
    # Try write_inode(struct inode *, int sync) returning int.

    rm -f $TESTDIR/dummy.c $TESTDIR/dummy.o $TESTDIR/conftest.o
    cat >> $TESTDIR/dummy.c <<_EOF
#include <linux/fs.h>
int dummy(void)
{
    struct super_operations tmp;
    tmp.write_inode = 0;
    return tmp.write_inode((struct inode *) 0, (int) 0);
}
_EOF
    if make -C $KERNELDIR M=$PWD/$TESTDIR modules > /dev/null 2>&1 ; then
        echo "#define HAVE_WRITE_INODE_RETURNS_INT" >> $HEADER
        RESULT="int return."
    fi
fi
echo $RESULT

rm -rf $PWD/$TESTDIR
