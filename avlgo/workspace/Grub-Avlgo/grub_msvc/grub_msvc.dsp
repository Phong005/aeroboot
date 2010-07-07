# Microsoft Developer Studio Project File - Name="grub_msvc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=grub_msvc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "grub_msvc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "grub_msvc.mak" CFG="grub_msvc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "grub_msvc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "grub_msvc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "grub_msvc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "grub_msvc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# Begin Custom Build - $(InputPath)
InputPath=.\Debug\grub_msvc.exe
InputName=grub_msvc
SOURCE="$(InputPath)"

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cd .. 
	build.bat 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "grub_msvc - Win32 Release"
# Name "grub_msvc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\stage2\bios.c
# End Source File
# Begin Source File

SOURCE=..\stage2\boot.c
# End Source File
# Begin Source File

SOURCE=..\stage2\builtins.c
# End Source File
# Begin Source File

SOURCE=..\stage2\char_io.c
# End Source File
# Begin Source File

SOURCE=..\stage2\cmdline.c
# End Source File
# Begin Source File

SOURCE=..\stage2\common.c
# End Source File
# Begin Source File

SOURCE=..\stage2\console.c
# End Source File
# Begin Source File

SOURCE=..\stage2\disk_io.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_ext2fs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_fat.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_ffs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_iso9660.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_jfs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_minix.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_ntfs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_reiserfs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_romfs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_ufs2.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_vstafs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\fsys_xfs.c
# End Source File
# Begin Source File

SOURCE=..\stage2\gunzip.c
# End Source File
# Begin Source File

SOURCE=..\stage2\hercules.c
# End Source File
# Begin Source File

SOURCE=..\stage2\Makefile
# End Source File
# Begin Source File

SOURCE=..\stage2\md5.c
# End Source File
# Begin Source File

SOURCE=..\stage2\serial.c
# End Source File
# Begin Source File

SOURCE="..\stage2\smp-imps.c"
# End Source File
# Begin Source File

SOURCE=..\stage2\stage1_5.c
# End Source File
# Begin Source File

SOURCE=..\stage2\stage2.c
# End Source File
# Begin Source File

SOURCE=..\stage2\terminfo.c
# End Source File
# Begin Source File

SOURCE=..\stage2\tparm.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\stage2\apic.h
# End Source File
# Begin Source File

SOURCE=..\stage2\defs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\dir.h
# End Source File
# Begin Source File

SOURCE=..\stage2\disk_inode.h
# End Source File
# Begin Source File

SOURCE=..\stage2\disk_inode_ffs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\fat.h
# End Source File
# Begin Source File

SOURCE=..\stage2\filesys.h
# End Source File
# Begin Source File

SOURCE=..\stage2\freebsd.h
# End Source File
# Begin Source File

SOURCE=..\stage2\fs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\hercules.h
# End Source File
# Begin Source File

SOURCE="..\stage2\i386-elf.h"
# End Source File
# Begin Source File

SOURCE=..\stage2\imgact_aout.h
# End Source File
# Begin Source File

SOURCE=..\stage2\iso9660.h
# End Source File
# Begin Source File

SOURCE=..\stage2\jfs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\mb_header.h
# End Source File
# Begin Source File

SOURCE=..\stage2\mb_info.h
# End Source File
# Begin Source File

SOURCE=..\stage2\md5.h
# End Source File
# Begin Source File

SOURCE=..\stage2\nbi.h
# End Source File
# Begin Source File

SOURCE=..\stage2\pc_slice.h
# End Source File
# Begin Source File

SOURCE=..\stage2\romfs_fs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\serial.h
# End Source File
# Begin Source File

SOURCE=..\stage2\shared.h
# End Source File
# Begin Source File

SOURCE="..\stage2\smp-imps.h"
# End Source File
# Begin Source File

SOURCE=..\stage2\term.h
# End Source File
# Begin Source File

SOURCE=..\stage2\terminfo.h
# End Source File
# Begin Source File

SOURCE=..\stage2\tparm.h
# End Source File
# Begin Source File

SOURCE=..\stage2\ufs2.h
# End Source File
# Begin Source File

SOURCE=..\stage2\vstafs.h
# End Source File
# Begin Source File

SOURCE=..\stage2\xfs.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "GAS files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\stage2\apm.S
# End Source File
# Begin Source File

SOURCE=..\stage2\asm.S
# End Source File
# Begin Source File

SOURCE=..\stage2\nbloader.S
# End Source File
# Begin Source File

SOURCE=..\stage2\pxeloader.S
# End Source File
# Begin Source File

SOURCE=..\stage2\setjmp.S
# End Source File
# Begin Source File

SOURCE=..\stage2\start.S
# End Source File
# Begin Source File

SOURCE=..\stage2\start_eltorito.S
# End Source File
# End Group
# Begin Source File

SOURCE=..\build.bat
# End Source File
# Begin Source File

SOURCE=..\makefile

!IF  "$(CFG)" == "grub_msvc - Win32 Release"

!ELSEIF  "$(CFG)" == "grub_msvc - Win32 Debug"

# Begin Custom Build - $(InputPath)
InputPath=..\makefile
InputName=makefile

"$(InputName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cd .. 
	build.bat 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
