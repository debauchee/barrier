@echo off
REM Use setlocal to automatically unset variables
setlocal

REM Inno Setup folders. Defined in order of preference.
set INNO6_USER_ROOT=%LOCALAPPDATA%\Programs\Inno Setup 6
set INNO6_ROOT=%ProgramFiles(x86)%\Inno Setup 6
SET INNO5_ROOT=%ProgramFiles(x86)%\Inno Setup 5
set INNO_ROOTS="%INNO6_USER_ROOT%" "%INNO6_ROOT%" "%INNO5_ROOT%"
set INNO_EXE=ISCC.exe
set INNO_EXE_PATH=""

set savedir=%cd%
cd /d %~dp0

if not exist build\bin\Release goto buildproject

echo Building 64-bit Windows installer...

REM Find where Inno Setup is installed
for %%r in (%INNO_ROOTS%) do (
    if exist "%%~r\%INNO_EXE%" (
        set INNO_EXE_PATH="%%~r\%INNO_EXE%"
        goto endcheckinnosetuploop
    )
)
:endcheckinnosetuploop

REM Check that Inno Setup is installed
if %INNO_EXE_PATH%=="" (
    echo Could not find Inno Setup.
    echo Check if Inno Setup is installed.
    goto failed
)

cd build\installer-inno
if ERRORLEVEL 1 goto buildproject
%INNO_EXE_PATH% /Qp barrier.iss
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
cd /d %savedir%
