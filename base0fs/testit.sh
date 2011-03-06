#!/bin/bash -norc
set -x

#strace -v ./mapper1 /mnt/base0fs/BAR.TXT 0 20

/tmp/fsx-linux -N 1000 /mnt/base0fs/foo.$$

exit 0

# attach a node so /mnt/base0fs/abc -> /n/fist/zadok
./fist_ioctl +a /mnt/base0fs abc /n/fist/zadok
#read n
#./fist_ioctl +a /mnt/base0fs XyZ23q /some/place
exit 1

#ls -l /mnt/base0fs/uname

#read n
file=foo-$RANDOM

## shift inward test
#echo 'abcdefghijklmnopqrstuvwxyz0123456789' > /mnt/base0fs/$file
#read n
#echo 'XXXXXXXXXX' | ~ib42/c/test/write /mnt/base0fs/$file 10

## shift outward test
#echo 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' > /mnt/base0fs/$file
cp /bin/ls /mnt/base0fs/$file
read n
echo '1234567890' | ~ib42/c/test/write /mnt/base0fs/$file 4110

read n
cat <<EOF
aaaaaaaaaa1234567890
aaaaaaaaaaaaaaa
EO>> /mnt/base0fs/G

#  touch /mnt/base0fs/$file
#  ls -l /mnt/base0fs/$file
#  read n
#  #perl -e "print 'a' x 80" >> /mnt/base0fs/$file
#  ~ib42/c/test/truncate /mnt/base0fs/$file 100
#  read n
#  #perl -e "print 'b' x 4900" >> /mnt/base0fs/$file
#  ~ib42/c/test/truncate /mnt/base0fs/$file 5000
#  read n
#  date >> /mnt/base0fs/$file
#  read n
#  hexdump /mnt/base0fs/$file


#echo
#cp /etc/termcap /mnt/base0fs/$file
#read n
#od -Ax -h /n/fist/base0fs/$file
#ls -l /mnt/base0fs/$file
