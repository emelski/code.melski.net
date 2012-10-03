#
# Makefile for the Linux 2.6 lofs
#

ifeq ($(KERNELRELEASE),)

LINUXSRC:=/lib/modules/$(shell uname -r)/build

all: kernel_config.h
	make -C ${LINUXSRC} SUBDIRS=$(PWD) modules

kernel_config.h: genconfig.sh
	sh genconfig.sh

clean:
	make -C ${LINUXSRC} SUBDIRS=$(PWD) clean
	rm -f kernel_config.h config.log

endif

${lofs-objs}: lofs_kernel.h

obj-m += lofs.o

lofs-objs :=		\
	dentry.o	\
	file.o		\
	inode.o		\
	kthread.o	\
	main.o		\
	mmap.o		\
	read_write.o	\
	super.o

