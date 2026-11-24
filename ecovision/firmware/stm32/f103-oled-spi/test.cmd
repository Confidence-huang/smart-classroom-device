@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0build"
cl /nologo /utf-8 /TC /std:c11 /W4 /WX /I"%~dp0." "%~dp0test_transport.c" "%~dp0oled_spi.c" /Fe:test_transport.exe
if errorlevel 1 exit /b 1
test_transport.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /TC /std:c11 /W4 /WX /I"%~dp0." /I"%~dp0..\EcoVision_F413ZH\App\Inc" "%~dp0test_receiver.c" "%~dp0ir_receiver.c" "%~dp0..\EcoVision_F413ZH\App\Src\ir_pulse_codec.c" /Fe:test_receiver.exe
if errorlevel 1 exit /b 1
test_receiver.exe
exit /b %errorlevel%
