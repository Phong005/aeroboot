
    Grub_Avlgo: Avlgo Source Code and Compiling Notes
    ================================================================

        This is a statement for how to creating Avlgo binary.

    How to Compile
    =====================
        1. Build romfs.bin

           For Avlgo used 'romfs' to support isoemu/memdisk, please 
           refer to romfs_app/ReadMe.txt for more information.

        2. Build Avlgo
       
           Avlgo is based on Grub. It is profit from Djgpp that we
         can compile Alvgo on Windows OS. Of course, when you get the
         'Compiling Envirenment + Source Code' from our site, you can
         simply dbl-click on the ./build.bat to get the great result:
                  ./Avlgo.sys.  ;^)

        3. Other Notes

        > a. To eaiser modify this source code, Plz choose an Windows' 
         IDE to do so, as I know, Visual C++ / UEStudio / EditPlus can
         be a best choice. Here, I want to use Visual C++ as an example:
         
             1st, please Open ./grub_msvc/grub_msvc.dsp using VC++, and
         switch to "File View". You could see a files' list, now, right
         click on makefile at the last, and choose "Compile makefile"
         to go. Refer to ./msvc_dev.jpg for a snapshot.

        > b. The copyright of this code. As we know, Avlgo is based on
         Grub, Avlgo should be protected by the GPL just like Grub. 
         Pay close attention to this!

        > c. I addd 'romfs'/pkzip file support to Avlgo. These codes 
         are all from the internet, I just order them, and we must say
         thanks to them.

        > d. I am modifying/improving Grub, since 0.95, and till now,
         what I can do just these...


    How to Test
    =====================
      
      1. Copy Avlgo.sys, Avldr.bin to C:\
      
      2. modify your boot.ini, and add one line:
      
            c:\avldr.bin="Avlgo - Demo"
            
            That's all.
    
    + --------------------------------------------------------------------- +
    |                       Fortune Favours the bold                        |
    |                                                                       |
    | Yestoday is a special day, but Today and Tomorrow will be more great! |
    |                                                                       |
    | To reach into the future, yet never neglect the past.                 |
    + --------------------------------------------------------------------- +

    ================================================================
    Gandalf (ganstein@gmail.com), Hnlyzhd (hnlyzhd@gmail.com)
    http://sysoft.zdwx.com/forum/
    http://sysoft.zdwx.com/forum/forumdisplay.php?fid=28
    10:10 2006-3-10 
