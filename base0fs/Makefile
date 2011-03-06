#
# Makefile for the Linux 2.6 base0fs
# $Id: Makefile,v 1.16 2006/09/26 01:16:46 jsipek Exp $
#

# this is where your kernel sources are
LINUXSRC = /lib/modules/$(shell uname -r)/build
# this should point to where your kernel headers are
TOPINC = -I${LINUXSRC}/include

# Fedora Core has the annoying habit of changing the kernel prototypes on us.
FEDORAVER=$(shell rpm -q fedora-release | cut -d- -f3)
KERNELEXTRA=x$(shell uname -r | cut -d- -f 2 -s)
ifeq ($(FEDORAVER),2)
ifneq ($(KERNELEXTRA),x)
EXTRA_CFLAGS += -DFEDORACORE2
endif
endif

# allow custmom override of TOPINC and LINUXSRC
-include fistdev.mk

CC	= gcc
EXTRA_CFLAGS += -DFISTGEN -I. ${TOPINC} ${EXTRACFLAGS} -O2 -Wall -Wno-unused -fno-strict-aliasing -Werror
UCFLAGS = -DFISTGEN -I. ${TOPINC} -g -O2 -Wall -Wno-unused # -Werror

obj-m := base0fs.o
ADD_M_SRC =  
base0fs-objs :=    fist_base0fs.o \
	dentry.o file.o inode.o main.o super.o print.o \
	${ADD_M_SRC:.c=.o}

BINS	= fist_ioctl 

all:	kernel_config.h ${BINS}
	make -C ${LINUXSRC} SUBDIRS=$(PWD) modules


kernel_config.h: genconfig.sh
	sh genconfig.sh

fist_ioctl: fist_ioctl.c
	${CC} -o $@ $? ${UCFLAGS}



memtest:
	perl match-malloc.pl log.txt

TAGS:
	etags -t *.[hc]

tags:
	ctags *.[hc]


clean:
	rm -f ${obj-m:.o=.ko} ${base0fs-objs} *.d .*.o.cmd .*.flags \#* *~
	rm -f ${BINS} TAGS
# dependencies
${base0fs-objs}: base0fs.h fist.h fist_base0fs.h lock.h 

# additional makefile rules, if any


