#!/bin/bash -norc
set -x
# overlay mount on top of NFS example

PATH=/sbin:.:/usr/local/fist:${PATH}
export PATH

#make module_install
#make module_install_nocheck
#make install
lsmod
insmod ./base0fs.o || exit
lsmod

#read n
#sleep 1

mount -t base0fs -o dir=/mnt/base0fs /mnt/base0fs /mnt/base0fs || exit

#read n
#sleep 1
fist_ioctl -d /mnt/base0fs ${1:-18} || exit
fist_ioctl -f /mnt/base0fs 1 || exit

if test -f fist_setkey ; then
    read n
    echo abrakadabra | ./fist_setkey /mnt/base0fs
    echo
fi
