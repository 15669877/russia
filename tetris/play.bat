@echo off
chcp 65001 >nul 2>nul
cd /d "%~dp0"
echo ============================================
echo  Tetris - full game
echo  Arrows or WASD: move   Up/W: rotate
echo  Down/S: soft drop   Space: hard drop   Q: quit
echo ============================================
echo.
tetris.exe
echo.
echo [game ended] press any key to close
pause >nul
