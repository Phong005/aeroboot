@echo off

rem
rem (r) Sysoft Space (http://sysoft.zdwx.com/)
rem 

echo.
echo : Building Avlgo Stub ... ...
echo.
set DJGPP=D:\DJGPP\DJGPP.ENV
set PATH=D:\DJGPP\BIN;%PATH%

set MAKE_PWD=%cd%
echo src_dir = %MAKE_PWD%> make.pwd
make all
