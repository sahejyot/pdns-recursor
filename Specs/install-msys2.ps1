# MSYS2 Installation Script for PowerDNS Recursor Development
# This script helps download and install MSYS2 with required dependencies

Write-Host "=======================================" -ForegroundColor Green
Write-Host "  MSYS2 Setup for PowerDNS Recursor   " -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Warning: Not running as administrator" -ForegroundColor Yellow
    Write-Host "Some operations may require elevation" -ForegroundColor Yellow
}

# Check if MSYS2 is already installed
$msys2Path = "C:\msys64"
if (Test-Path $msys2Path) {
    Write-Host "MSYS2 already installed at: $msys2Path" -ForegroundColor Green
    Write-Host ""
    Write-Host "To update MSYS2, run the following in MSYS2 UCRT64 terminal:" -ForegroundColor Yellow
    Write-Host "  pacman -Syu" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Then continue with dependency installation..." -ForegroundColor Yellow
} else {
    Write-Host "MSYS2 not found at: $msys2Path" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please download and install MSYS2 from:" -ForegroundColor Cyan
    Write-Host "  https://www.msys2.org/" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "After installation, run this script again to install dependencies." -ForegroundColor Yellow
    exit 0
}

# Function to run MSYS2 command
function Run-MSYS2Command {
    param([string]$command)
    
    $script = @"
#!/bin/bash
$command
"@
    
    $script | & "$msys2Path\ucrt64.exe" -c ""
}

# Check if ucrt64.exe exists
if (Test-Path "$msys2Path\ucrt64.exe") {
    Write-Host "UCRT64 found. Checking installed packages..." -ForegroundColor Green
} else {
    Write-Host "Error: UCRT64 not found. Please install MSYS2 completely." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "The following packages will be installed:" -ForegroundColor Yellow
Write-Host "  - MinGW GCC compiler" -ForegroundColor Cyan
Write-Host "  - CMake" -ForegroundColor Cyan
Write-Host "  - Make" -ForegroundColor Cyan
Write-Host "  - Git" -ForegroundColor Cyan
Write-Host "  - Boost libraries" -ForegroundColor Cyan
Write-Host "  - OpenSSL" -ForegroundColor Cyan
Write-Host "  - libevent" -ForegroundColor Cyan
Write-Host ""

$confirmation = Read-Host "Do you want to continue? (y/n)"
if ($confirmation -ne 'y' -and $confirmation -ne 'Y') {
    Write-Host "Installation cancelled." -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Green
Write-Host ""

# Commands to run in MSYS2
$commands = @(
    "# Update system",
    "pacman -Syu --noconfirm",
    
    "# Install compiler toolchain",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc-fortran",
    
    "# Install build tools",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-cmake",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-make",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-pkg-config",
    
    "# Install git and editors",
    "pacman -S --noconfirm git",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-nano",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-vim",
    
    "# Install dependencies for PowerDNS",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-boost",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-openssl",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-libevent",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-curl",
    "pacman -S --noconfirm mingw-w64-ucrt-x86_64-wget",
    
    "# Verify installation",
    "gcc --version",
    "cmake --version",
    "pkg-config --version"
)

Write-Host "Please run these commands in MSYS2 UCRT64 terminal:" -ForegroundColor Yellow
Write-Host "=======================================" -ForegroundColor Green
foreach ($cmd in $commands) {
    Write-Host $cmd -ForegroundColor Cyan
}
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""

Write-Host "IMPORTANT: Run these commands in MSYS2 UCRT64 terminal!" -ForegroundColor Yellow
Write-Host ""
Write-Host "Steps:" -ForegroundColor Yellow
Write-Host "1. Open MSYS2 UCRT64 from Start Menu" -ForegroundColor Cyan
Write-Host "2. Copy and paste the commands above" -ForegroundColor Cyan
Write-Host "3. Wait for installation to complete" -ForegroundColor Cyan
Write-Host "4. Verify with: gcc --version" -ForegroundColor Cyan
Write-Host ""

Write-Host "After installation, you can use MSYS2 in Cursor!" -ForegroundColor Green
Write-Host ""
Write-Host "Read SETUP-MSYS2-CURSOR.md for details." -ForegroundColor Green

