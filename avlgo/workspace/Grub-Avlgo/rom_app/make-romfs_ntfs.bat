REM Create a romfs file system image. Do not force exec flag.

gzip -9 Input\memdisk
gzip -9 Input\isoemu
make-romfs -d Input -f romfs.bin -a 512 -V 'UI.Demo'
 


