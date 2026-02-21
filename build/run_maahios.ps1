# MaahiOS Build and Run Script
$ErrorActionPreference = "Stop"

# Ensure we're in the build directory
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host "=== MaahiOS Build & Run ===" -ForegroundColor Yellow
Write-Host "Build directory: $PWD"
Write-Host ""
Write-Host "[1/3] Setting up toolchain PATH..." -ForegroundColor Yellow
$env:PATH = "C:\i686-elf-tools\bin;C:\msys64\usr\bin;$env:PATH"
Write-Host ""
Write-Host "[2/3] Building MaahiOS..." -ForegroundColor Yellow
bash.exe build.sh
if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful" -ForegroundColor Green
} else {
    Write-Host "`nBuild failed - not launching QEMU" -ForegroundColor Red
    exit 1
}
Write-Host ""
Write-Host "[3/3] Checking for existing QEMU..." -ForegroundColor Yellow
$qemuProcess = Get-Process -Name "qemu-system-i386" -ErrorAction SilentlyContinue
if ($qemuProcess) {
    Write-Host "QEMU is already running!" -ForegroundColor Red
    Write-Host "Please close existing QEMU before running this script."
    exit 1
}
Write-Host ""
Write-Host "Launching QEMU..." -ForegroundColor Yellow
Write-Host "Boot ISO: $PWD\boot.iso"
Write-Host "Serial log: $PWD\serial.log"
Write-Host "Debug log: $PWD\qemu_debug.log"
Write-Host "QEMU will run indefinitely. Close window to stop." -ForegroundColor Green
Write-Host ""
if (Test-Path "serial.log") { Remove-Item "serial.log" -Force }
if (Test-Path "qemu_debug.log") { Remove-Item "qemu_debug.log" -Force }
& "C:\Program Files\qemu\qemu-system-i386.exe" -cdrom boot.iso -m 512M -serial file:serial.log -d int,cpu_reset -D qemu_debug.log
Write-Host ""
Write-Host "QEMU exited." -ForegroundColor Yellow
