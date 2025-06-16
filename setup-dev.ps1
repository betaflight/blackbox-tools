# Windows Development Setup Script for Blackbox Tools
# This script helps set up the development environment for building blackbox tools

param(
    [string]$VcpkgPath = "C:\vcpkg",
    [switch]$SkipVcpkg,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
Windows Development Setup for Blackbox Tools

Usage: .\setup-dev.ps1 [options]

Options:
  -VcpkgPath <path>    Path to install vcpkg (default: C:\vcpkg)
  -SkipVcpkg           Skip vcpkg installation and setup
  -Help                Show this help message

Examples:
  .\setup-dev.ps1                           # Standard setup
  .\setup-dev.ps1 -VcpkgPath "D:\tools\vcpkg"  # Custom vcpkg location
  .\setup-dev.ps1 -SkipVcpkg                # Skip vcpkg setup

This script will:
1. Check for Visual Studio installation
2. Install and configure vcpkg (unless -SkipVcpkg is used)
3. Install required dependencies (Cairo, FreeType)
4. Integrate vcpkg with Visual Studio
5. Verify the setup by attempting a test build

"@
    exit 0
}

Write-Host "Setting up Windows development environment for Blackbox Tools..." -ForegroundColor Green

# Check for Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInstallations = & $vsWhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
    if ($vsInstallations.Count -gt 0) {
        $latestVS = $vsInstallations | Sort-Object installationVersion -Descending | Select-Object -First 1
        Write-Host "✓ Found Visual Studio: $($latestVS.displayName) at $($latestVS.installationPath)" -ForegroundColor Green
    } else {
        Write-Error "Visual Studio with C++ tools not found. Please install Visual Studio with 'Desktop development with C++' workload."
        exit 1
    }
} else {
    Write-Warning "Visual Studio Installer not found. Please ensure Visual Studio is installed."
}

if (-not $SkipVcpkg) {
    # Setup vcpkg
    Write-Host "Setting up vcpkg at $VcpkgPath..." -ForegroundColor Yellow
    
    if (-not (Test-Path $VcpkgPath)) {
        Write-Host "Cloning vcpkg..."
        git clone https://github.com/Microsoft/vcpkg.git $VcpkgPath
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to clone vcpkg. Please ensure git is installed and accessible."
            exit 1
        }
    } else {
        Write-Host "vcpkg already exists at $VcpkgPath" -ForegroundColor Yellow
    }
    
    # Bootstrap vcpkg
    $vcpkgExe = Join-Path $VcpkgPath "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Host "Bootstrapping vcpkg..."
        & (Join-Path $VcpkgPath "bootstrap-vcpkg.bat")
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to bootstrap vcpkg."
            exit 1
        }
    }
    
    # Integrate with Visual Studio
    Write-Host "Integrating vcpkg with Visual Studio..."
    & $vcpkgExe integrate install
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to integrate vcpkg with Visual Studio."
        exit 1
    }
    
    # Install dependencies for blackbox_render
    Write-Host "Installing Cairo and FreeType dependencies..."
    Write-Host "This may take several minutes on first run..." -ForegroundColor Yellow
    
    & $vcpkgExe install cairo:x64-windows freetype:x64-windows
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to install some dependencies. blackbox_render may not build correctly."
    } else {
        Write-Host "✓ Dependencies installed successfully" -ForegroundColor Green
    }
    
    # Set environment variable
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgPath, "User")
    $env:VCPKG_ROOT = $VcpkgPath
    Write-Host "✓ Set VCPKG_ROOT environment variable" -ForegroundColor Green
}

# Test build
Write-Host "Testing build setup..." -ForegroundColor Yellow
$solutionPath = "visual-studio\Baseflight blackbox tools.sln"

if (Test-Path $solutionPath) {
    # Try to build blackbox_decode (no external dependencies)
    Write-Host "Testing build of blackbox_decode..."
    $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    if (-not (Test-Path $msbuild)) {
        $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    }
    if (-not (Test-Path $msbuild)) {
        $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    }
    if (-not (Test-Path $msbuild)) {
        Write-Warning "MSBuild not found. Using system PATH."
        $msbuild = "msbuild"
    }
    
    & $msbuild "visual-studio\blackbox_decode\blackbox_decode.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Test build successful!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Setup complete! You can now:" -ForegroundColor Green
        Write-Host "1. Open 'visual-studio\Baseflight blackbox tools.sln' in Visual Studio" -ForegroundColor White
        Write-Host "2. Select Release|x64 configuration for optimal performance" -ForegroundColor White
        Write-Host "3. Build → Build Solution (Ctrl+Shift+B)" -ForegroundColor White
        Write-Host "4. Find executables in the 'obj\' directory" -ForegroundColor White
    } else {
        Write-Warning "Test build failed. Check Visual Studio installation and try building manually."
    }
} else {
    Write-Error "Solution file not found. Please run this script from the project root directory."
    exit 1
}

Write-Host ""
Write-Host "For more information, see BUILD.md" -ForegroundColor Cyan
