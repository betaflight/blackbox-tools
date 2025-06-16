# This file declares the vcpkg toolchain integration
# Place this in your root directory to enable vcpkg integration

vcpkg_minimum_required(VERSION 2024-04-23)

vcpkg_install(
    OUT_SOURCE_PATH CURRENT_PACKAGES_DIR
    PACKAGES
        cairo[core,fontconfig,freetype]:x64-windows
        freetype[core,brotli,bzip2,png,zlib]:x64-windows
    OPTIONS
        "VCPKG_TARGET_TRIPLET=x64-windows"
)
