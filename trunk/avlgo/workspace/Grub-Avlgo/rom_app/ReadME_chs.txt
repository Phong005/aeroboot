
    Rom_App: 为 Avlgo 准备内建的 romfs 影像文件
    ================================================================

        这个文件夹用于创建内建的 romfs 影像文件。我们知道 Avlgo 会
    使用 ISOEmu/Memdisk 去实现磁盘虚拟的功能。

        在使用的时候，请将你需要内置的文件放到 `Input` 目录, 推荐使用
    gzip.exe 对其进行压缩。然后请使用 make-romfs_ntfs.bat 生成 
    `romfs.bin` 到 ../stage2 即可。
    
        --------- --------- --------- --------- --------- -------

    ================================================================
    Gandalf (ganstein@gmail.com)
    http://sysoft.zdwx.com/forum/
    http://sysoft.zdwx.com/forum/forumdisplay.php?fid=28
