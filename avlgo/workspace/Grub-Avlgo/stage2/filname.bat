关于Acronis True Image Server软件的一些建议! 
http://www.wilderssecurity.com/showthread.php?p=654682#post654682

--------------------------------------------------------------------------------

管理员:
你好,通过一些资料了解了Acronis True Image Server的性能,在试用过程有一些建议想提一下.(我的英文水平有限,只能看论坛的一些内容而已,发贴子我还是用中文吧,如果此贴没能得到回复,本人以后将认为本论坛不接受中文贴子,也不会再发类似贴子)

1.运行TI_DOS.EXE程序感觉备份速度比较慢,能否加快磁盘读取速度备份速度大约为100M/Min,恢复速度大约为130M/Min.

2.目标分区可以接受/partition:1-x 这样的磁盘ID,而镜像源分区要用C、D、E、等来标示。能否也用硬盘ID 1－X来进行标记保存和恢复呢？

3.TI_DOS.EXE程序如何从隐藏分区来还原以前备份的镜像呢，隐藏分区中的AAA.TIB文件能否也被TI_DOS.EXE识别并恢复呢？

4.TI_DOS.EXE程序能否加上创建隐藏分区的参数如： TI_DOS /SOURCE
从隐藏分区恢复AAA.TIB时用 TI_DOS /DEPLOY /PARTITION:1-1 /FILENAME:SOURCE:\AAA.TIB

5.在WIN下用程序分区仍然能够用磁盘管理工具所看到，能否改进一下，使用ATA或者SATA规范创建一个HPA分区，这样更安全！

6.在WIN下创建SAFE软盘，启动后无法识别光驱，能否在BOOTWIZ.SYS中加入光驱驱动！

7.创建 RECOVERY CD的启动方式感觉占用太多空间，完全可以在KERNEL.DAT加载之后，用RAMDISK.EXE来运行光盘根目录下的RAMDISK.DAT文件，这也是在BOOTWIZ.SYS的同一个问题。

8.希望开发一个可以直接在DOS下运行的SHELL程序支持光驱和USB设备，并且支持备份恢复目录和文件，这样GHOST将再也没有地位可言。