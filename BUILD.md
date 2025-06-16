# Building Blackbox Tools

This document explains how to build the Blackbox Tools locally and understand the CI/CD pipeline.

## Local Development (Windows)

### Prerequisites

1. **Visual Studio 2019 or later** (Community Edition is sufficient)
   - Install the "Desktop development with C++" workload
   - Make sure Windows 10/11 SDK is installed

2. **vcpkg (Recommended for dependencies)**
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg integrate install
   ```

### Building with Visual Studio

1. **Open the solution**
   ```
   visual-studio/Baseflight blackbox tools.sln
   ```

2. **Configure vcpkg (if using dependencies)**
   - The blackbox_render project requires Cairo and FreeType libraries
   - If vcpkg is installed and integrated, dependencies will be downloaded automatically
   - Otherwise, ensure libraries are available in `lib/win32/`

3. **Build Configuration**
   - Select `Release|x64` for optimal performance
   - Select `Debug|x64` for development/debugging
   - Win32 configurations are also available for 32-bit builds

4. **Build the projects**
   - Build → Build Solution (Ctrl+Shift+B)
   - Executables will be placed in `obj/` directory

### Building with MSBuild (Command Line)

```cmd
# Build all projects (Release x64)
msbuild "visual-studio/Baseflight blackbox tools.sln" /p:Configuration=Release /p:Platform=x64

# Build specific project
msbuild "visual-studio/blackbox_decode/blackbox_decode.vcxproj" /p:Configuration=Release /p:Platform=x64

# Build with vcpkg integration
msbuild "visual-studio/blackbox_render/blackbox_render.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=true
```

## Dependencies

### Core Tools (blackbox_decode, encoder_testbed)
- **No external dependencies** - these build with just the Windows SDK
- Uses included `getopt` library for command-line parsing

### Rendering Tool (blackbox_render)
- **Cairo** - 2D graphics library
- **FreeType** - Font rendering
- **Recommended**: Use vcpkg for automatic dependency management
- **Alternative**: Manually place libraries in `lib/win32/`

### vcpkg Dependencies
If using vcpkg, the following packages will be automatically installed:
```json
{
  "cairo": ["fontconfig", "freetype"],
  "freetype": ["brotli", "bzip2", "png", "zlib"]
}
```

## Local Development (Linux/macOS)

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential pkg-config libfreetype6-dev libcairo2-dev

# macOS (with Homebrew)
brew install pkg-config freetype cairo
```

### Building
```bash
make
```

Executables will be placed in the root directory.

## CI/CD Pipeline

### GitHub Actions Workflow

The CI pipeline builds for three platforms:
- **Linux** (Ubuntu latest)
- **macOS** (macOS latest) 
- **Windows** (Windows Server 2022)

### Windows CI Build Process

1. **Setup vcpkg** with specific commit for reproducibility
2. **Configure MSBuild** with VS2022 toolchain
3. **Build sequence**:
   - blackbox_decode (no dependencies)
   - encoder_testbed (no dependencies)
   - blackbox_render (with Cairo/FreeType via vcpkg)
4. **Artifact collection** includes executables and DLLs

### Build Artifacts

Artifacts are uploaded with platform-specific names:
- `blackbox-tools-Linux`
- `blackbox-tools-macOS` 
- `blackbox-tools-Windows`

Each artifact contains:
- All built executables for the platform
- Required runtime libraries (Windows DLLs)
- 30-day retention period

## Troubleshooting

### Common Issues

1. **Missing Windows SDK**
   - Install via Visual Studio Installer
   - Target Platform Version is automatically detected

2. **vcpkg integration not working**
   ```cmd
   .\vcpkg\vcpkg integrate install
   ```

3. **Cairo/FreeType not found**
   - Ensure vcpkg is properly integrated
   - Or manually copy libraries to `lib/win32/`

4. **Build fails with "unresolved external symbol"**
   - Check that all source files are included in the project
   - Verify platform toolset (v143 for VS2022)

### Build Configurations

| Configuration | Use Case | Optimizations |
|---------------|----------|---------------|
| Debug \| x64 | Development, debugging | None, full debug info |
| Release \| x64 | Production, distribution | Full optimization, minimal debug info |
| Debug \| Win32 | Legacy 32-bit debugging | None, full debug info |
| Release \| Win32 | Legacy 32-bit distribution | Full optimization |

### Performance Notes

- **x64 builds** are recommended for modern systems
- **Release builds** include whole program optimization
- **Static runtime** linking reduces dependencies
- **Link-time code generation** improves performance

## Modern Standards Compliance

### Project Files
- Updated to Visual Studio 2022 format (ToolsVersion="Current")
- Modern Platform Toolset (v143)
- Support for both x64 and Win32 platforms
- Conditional vcpkg integration

### CI/CD
- Uses latest GitHub Actions versions
- Reproducible builds with locked vcpkg commit
- Proper artifact retention and naming
- Cross-platform compatibility testing

### Code Quality
- Modern C compiler flags enabled
- SDL security checks enabled
- Conformance mode for strict C++ compliance
- Warning level 3 for all projects
