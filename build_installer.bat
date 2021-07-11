@echo off
set INNO_ROOT=C:\Program Files (x86)\Inno Setup 6

set savedir=%cd%
cd /d %~dp0

if not exist build\bin\Release goto buildproject

echo Building 64-bit Windows installer...

cd build\installer-inno
if ERRORLEVEL 1 goto buildproject
"%INNO_ROOT%\ISCC.exe" /Qp barrier.iss
if ERRORLEVEL 1 goto failed

echo Build completed successfully
goto done

:buildproject
echo To build a 64-bit Windows installer:
echo  - set B_BUILD_TYPE=Release in build_env.bat
echo  - also set other environmental overrides necessary for your build environment
echo  - run clean_build.bat to build Barrier and verify that it succeeds
echo  - re-run this script to create the installation package
goto done

:failed
echo Build failed

:done
set INNO_ROOT=

cd /d %savedir%
set savedir=
