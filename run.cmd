@echo off
set EXE="%~dp0build\Release\skeet_menu_standalone.exe"
if exist %EXE% (
    start "" %EXE%
) else (
    echo Executable not found at %EXE%
    echo Run build.ps1 first.
    pause
)
