<#
.SYNOPSIS
Builds the standalone Skeet GUI application.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = $PSScriptRoot

Write-Host "Configuring CMake (Win32)..." -ForegroundColor Cyan
cmake -B "$ScriptDir\build" -A Win32 -S "$ScriptDir"

Write-Host "Building $Config..." -ForegroundColor Cyan
cmake --build "$ScriptDir\build" --config $Config

Write-Host "`nBuild succeeded!" -ForegroundColor Green
Write-Host "Executable located at: $ScriptDir\build\$Config\skeet_menu_standalone.exe" -ForegroundColor Yellow
