
    Grub_Avlgo: Avlgo  源代码编译说明
    ================================================================

        这个文件夹用于创建 Avlgo 的二进制镜像。

    如何编译
    =====================
        1. 生成 romfs.bin

           由于 Avlgo 有使用了 romfs 支持内建 ISOEmu/Memdisk 的功能，
           所以请到 romfs_app/ReadMe.txt 获取帮助。

        2. 生成 Avlgo
       
           Avlgo 是基于 Grub 的。它能在 Windows 上编译，得益于 Djgpp
         的使用。当然，当你从我们的网站获取了这个“编译环境 + 代码”
         包后，需要作的，就是双击 ./build.bat 即可。它会为你生成 
         ./Avlgo.sys 。

        3. 几点说明

        > a. 为了方便修改源代码，请使用 Visual C++ 作为开发环境。当然
         使用 UEStudio 也是不错的选择。我仅就使用 Visual C++ 的情况作
         一说明。请使用 VC++ 打开 ./grub_msvc/grub_msvc.dsp，然后切换
         到 "File View"。你可以看到文件的列表，然后，请在 makefile 文
         件上使用右键，选择 "Compile makefile" 即可。参见 ./msvc_dev.jpg

        > b. 代码的使用权利。由于是 Grub 的一个派生版本，所以，它是受 
         GPL 保护的。请注意这个问题。

        > c. 我为这个版本加入了 romfs 的支持，以及 pkzip 文件的支持。
         这些代码也仅仅是我整理出来的，他们的权利是其原始作者们的。谢
         谢他们的努力。

        > e. 我一直都是在对 Grub 作一些改进、增加一些功能。也就这些吧 ...


    如何测试
    =====================
    
    将 Avlgo.sys, Alvdr.bin 复制到 c:\;
    
    修改 c:\boot.ini, 添加一行 "c:\avldr.bin="Avlgo - demo"" 即可。
    
    
    + ------------------------------------------- +
    | 昨天是个特别的日子，可是今天及将来更伟大！  |
    |                                             |
    | 让我们注视未来，但切莫忽视过去 － 天佑勇者  |
    + ------------------------------------------- +

    ================================================================
    Gandalf (ganstein@gmail.com), Hnlyzhd (hnlyzhd@gmail.com)
    http://sysoft.zdwx.com/forum/
    http://sysoft.zdwx.com/forum/forumdisplay.php?fid=28
    10:10 2006-3-10 
