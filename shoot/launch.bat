@echo off
chcp 65001 >nul 2>nul
title Shoot Launcher
cd /d "%~dp0"

:: ---------- compiler path (WinLibs gcc installed via winget) ----------
set "GCC=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe"

:: ---------- build any missing exes ----------
if not exist entities.h        call :build gen_entities.c
if not exist gen_entities.exe  call :build gen_entities.c
if not exist shoot.exe         call :build shoot.c
if not exist step1_display.exe call :build step1_display.c
if not exist step2_move.exe    call :build step2_move.c
if not exist step3_shoot.exe   call :build step3_shoot.c
if not exist step4_collide.exe call :build step4_collide.c
if not exist step5_wave.exe    call :build step5_wave.c
if not exist selftest.exe      call :build selftest.c
if not exist sim.exe           call :build sim.c
if not exist preview.exe       call :build preview.c

:menu
cls
echo ============================================
echo   Shoot Launcher
echo ============================================
echo   0  Full game        shoot
echo   1  Step1 draw sprites
echo   2  Step2 move
echo   3  Step3 shoot
echo   4  Step4 collision
echo   5  Step5 waves
echo   6  Self test        22 assertions
echo   7  Simulation       50 auto games
echo   8  Render preview
echo   9  Regenerate assets and projects
echo   q  quit
echo ============================================
set /p CH=Choose 0-9 or q:

if /i "%CH%"=="0" (shoot.exe & call :back)
if /i "%CH%"=="1" (step1_display.exe & call :back)
if /i "%CH%"=="2" (step2_move.exe & call :back)
if /i "%CH%"=="3" (step3_shoot.exe & call :back)
if /i "%CH%"=="4" (step4_collide.exe & call :back)
if /i "%CH%"=="5" (step5_wave.exe & call :back)
if /i "%CH%"=="6" (selftest.exe & call :back)
if /i "%CH%"=="7" (sim.exe & call :back)
if /i "%CH%"=="8" (preview.exe & call :back)
if /i "%CH%"=="9" (call :regen & call :back)
if /i "%CH%"=="q" exit /b
echo Invalid input, try again.
timeout /t 1 >nul
goto menu

:back
echo.
echo [press any key to return to menu]
pause >nul
goto menu

:: ---------- regenerate assets + vs projects ----------
:regen
echo [regen] entities.h ...
gen_entities.exe >nul
echo [regen] vs projects ...
gcc gen_vsproj.c -o gen_vsproj.exe
gen_vsproj.exe
echo [regen] done.
goto :eof

:: ---------- build subroutine ----------
:build
echo [build] %1 ...
if exist "%GCC%" (
    "%GCC%" -O2 -static %1 -o %~n1.exe
) else (
    echo gcc not found. Install WinLibs via winget, or add gcc to PATH.
)
goto :eof
