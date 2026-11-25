@echo off
chcp 65001 >nul 2>nul
cd /d "%~dp0"
echo ============================================
echo  Shoot - full game
echo  Arrows or A D: move   SPACE: shoot   X: bomb   Q: quit
echo ============================================
echo.
shoot.exe
echo.
echo [game ended] press any key to close
pause >nul
