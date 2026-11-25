@echo off
chcp 65001 >nul 2>nul
title Tetris Launcher
cd /d "%~dp0"

:: ---------- compiler path (WinLibs gcc installed via winget) ----------
set "GCC=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe"

:: ---------- build any missing exes ----------
if not exist tetris.exe        call :build tetris.c
if not exist step1_display.exe call :build step1_display.c
if not exist step2_move.exe    call :build step2_move.c
if not exist step3_keys.exe    call :build step3_keys.c
if not exist step4_wall.exe    call :build step4_wall.c
if not exist step5_rotate.exe  call :build step5_rotate.c
if not exist selftest.exe      call :build selftest.c
if not exist sim.exe           call :build sim.c
if not exist preview.exe       call :build preview.c

:menu
cls
echo ============================================
echo   Tetris Launcher
echo ============================================
echo   0  Full game        (tetris)
echo   1  Step1 print/draw blocks
echo   2  Step2 move + no ghost
echo   3  Step3 keyboard control
echo   4  Step4 wall + stop
echo   5  Step5 rotate
echo   6  Self test        (40 assertions)
echo   7  Simulation       (200 auto games)
echo   8  Render preview   (no flicker demo)
echo   q  quit
echo ============================================
set /p CH=Choose 0-8 or q:

if /i "%CH%"=="0" (tetris.exe & call :back)
if /i "%CH%"=="1" (step1_display.exe & call :back)
if /i "%CH%"=="2" (step2_move.exe & call :back)
if /i "%CH%"=="3" (step3_keys.exe & call :back)
if /i "%CH%"=="4" (step4_wall.exe & call :back)
if /i "%CH%"=="5" (step5_rotate.exe & call :back)
if /i "%CH%"=="6" (selftest.exe & call :back)
if /i "%CH%"=="7" (sim.exe & call :back)
if /i "%CH%"=="8" (preview.exe & call :back)
if /i "%CH%"=="q" exit /b
echo Invalid input, try again.
timeout /t 1 >nul
goto menu

:back
echo.
echo [press any key to return to menu]
pause >nul
goto menu

:: ---------- build subroutine ----------
:build
echo [build] %1 ...
if exist "%GCC%" (
    "%GCC%" -O2 -static %1 -o %~n1.exe
) else (
    echo gcc not found. Install WinLibs via winget, or add gcc to PATH.
)
goto :eof
