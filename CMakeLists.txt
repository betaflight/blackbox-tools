cmake_minimum_required(VERSION 3.15)
project(blackbox-tools VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set static linking for Windows MSVC
if(WIN32 AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    
    # Add Windows-specific definitions
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DWIN32)
    add_definitions(-D_WIN32)
endif()

# Find packages - vcpkg with fallbacks for different package names
if(WIN32 AND MSVC)
    message(STATUS "=== Windows MSVC Build with vcpkg ===")
    
    # Find FreeType first (we know this works)
    set(FREETYPE_FOUND FALSE)
    find_package(Freetype QUIET)
    if(Freetype_FOUND)
        set(FREETYPE_FOUND TRUE)
        set(FREETYPE_LIBRARIES Freetype::Freetype)
        message(STATUS "✓ FreeType found via Freetype: ${FREETYPE_LIBRARIES}")
    else()
        find_package(freetype CONFIG QUIET)
        if(freetype_FOUND)
            set(FREETYPE_FOUND TRUE)
            set(FREETYPE_LIBRARIES freetype)
            message(STATUS "✓ FreeType found via freetype: ${FREETYPE_LIBRARIES}")
        endif()
    endif()
    
    # Try to find Cairo with exhaustive search
    set(CAIRO_FOUND FALSE)
    
    # Method 1: Try the most common vcpkg names
    set(CAIRO_PACKAGE_NAMES "unofficial-cairo" "cairo" "Cairo" "cairo-gobject" "libcairo")
    foreach(PACKAGE_NAME ${CAIRO_PACKAGE_NAMES})
        if(NOT CAIRO_FOUND)
            message(STATUS "Trying to find package: ${PACKAGE_NAME}")
            find_package(${PACKAGE_NAME} CONFIG QUIET)
            
            # Check different possible target names for each package
            if(TARGET unofficial::cairo::cairo)
                set(CAIRO_FOUND TRUE)
                set(CAIRO_LIBRARIES unofficial::cairo::cairo)
                message(STATUS "✓ Cairo found via ${PACKAGE_NAME}: unofficial::cairo::cairo")
            elseif(TARGET cairo::cairo)
                set(CAIRO_FOUND TRUE)
                set(CAIRO_LIBRARIES cairo::cairo)
                message(STATUS "✓ Cairo found via ${PACKAGE_NAME}: cairo::cairo")
            elseif(TARGET Cairo::Cairo)
                set(CAIRO_FOUND TRUE)
                set(CAIRO_LIBRARIES Cairo::Cairo)
                message(STATUS "✓ Cairo found via ${PACKAGE_NAME}: Cairo::Cairo")
            elseif(TARGET cairo)
                set(CAIRO_FOUND TRUE)
                set(CAIRO_LIBRARIES cairo)
                message(STATUS "✓ Cairo found via ${PACKAGE_NAME}: cairo")
            elseif(TARGET Cairo)
                set(CAIRO_FOUND TRUE)
                set(CAIRO_LIBRARIES Cairo)
                message(STATUS "✓ Cairo found via ${PACKAGE_NAME}: Cairo")
            endif()
        endif()
    endforeach()
    
    # Method 3: Manual setup if libraries exist but CMake targets don't work
    if(NOT CAIRO_FOUND)
        message(STATUS "Trying manual Cairo setup...")
        
        # Look for Cairo libraries directly
        find_library(CAIRO_LIB NAMES cairo libcairo
            PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/lib"
            NO_DEFAULT_PATH)
            
        find_path(CAIRO_INCLUDE_DIR cairo/cairo.h
            PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/include"
            NO_DEFAULT_PATH)
            
        if(CAIRO_LIB AND CAIRO_INCLUDE_DIR)
            set(CAIRO_FOUND TRUE)
            
            # Create an imported target for consistency
            add_library(cairo_manual UNKNOWN IMPORTED)
            set_target_properties(cairo_manual PROPERTIES
                IMPORTED_LOCATION ${CAIRO_LIB}
                INTERFACE_INCLUDE_DIRECTORIES ${CAIRO_INCLUDE_DIR}
            )
            set(CAIRO_LIBRARIES cairo_manual)
            set(CAIRO_INCLUDE_DIRS ${CAIRO_INCLUDE_DIR})
            message(STATUS "✓ Cairo found manually: ${CAIRO_LIB}")
            message(STATUS "✓ Cairo include directory: ${CAIRO_INCLUDE_DIR}")
        endif()
    endif()
    
    # Method 4: Try PkgConfig as final fallback
    if(NOT CAIRO_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(CAIRO QUIET cairo)
            if(CAIRO_FOUND)
                message(STATUS "✓ Cairo found via pkg-config: ${CAIRO_LIBRARIES}")
            endif()
        endif()
    endif()
    
    # Check final results
    if(NOT CAIRO_FOUND)
        message(FATAL_ERROR "❌ Cairo not found! 
        
Tried packages: ${CAIRO_PACKAGE_NAMES}
Checked targets: unofficial::cairo::cairo, cairo::cairo, Cairo::Cairo, cairo, Cairo
Also tried: pkg-config

Possible solutions:
1. vcpkg install cairo:x64-windows-static
2. vcpkg install libcairo:x64-windows-static  
3. Check if Cairo was installed but with different target name")
    endif()
    
    if(NOT FREETYPE_FOUND)
        message(FATAL_ERROR "❌ FreeType not found! Install via: vcpkg install freetype:x64-windows-static")
    endif()
    
else()
    # Use pkg-config on Unix systems
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    message(STATUS "✓ Unix system - using pkg-config for graphics libraries")
endif()

message(STATUS "✓ All graphics dependencies resolved - blackbox_render will be built")

# Source files
set(COMMON_SOURCES
    src/parser.c
    src/tools.c
    src/platform.c
    src/stream.c
    src/decoders.c
    src/units.c
    src/blackbox_fielddefs.c
)

# Windows-specific sources
if(WIN32)
    list(APPEND COMMON_SOURCES lib/getopt_mb_uni/getopt.c)
    include_directories(lib/getopt_mb_uni)
endif()

# Decoder sources
set(DECODER_SOURCES
    ${COMMON_SOURCES}
    src/blackbox_decode.c
    src/gpxwriter.c
    src/imu.c
    src/battery.c
    src/stats.c
)

# Renderer sources
set(RENDERER_SOURCES
    ${COMMON_SOURCES}
    src/blackbox_render.c
    src/datapoints.c
    src/embeddedfont.c
    src/expo.c
    src/imu.c
)

# Encoder testbed sources
set(ENCODER_TESTBED_SOURCES
    ${COMMON_SOURCES}
    src/encoder_testbed.c
    src/encoder_testbed_io.c
)

# Include directories
include_directories(src)

# Blackbox decode executable
add_executable(blackbox_decode ${DECODER_SOURCES})

# Blackbox render executable - graphics libraries are required
add_executable(blackbox_render ${RENDERER_SOURCES})
message(STATUS "✓ blackbox_render target created")
    
# Link graphics libraries to renderer
if(WIN32 AND MSVC)
    # Collect all libraries first, then link in one consolidated call
    set(ALL_CAIRO_LIBS ${CAIRO_LIBRARIES} ${FREETYPE_LIBRARIES})
    set(ALL_SYSTEM_LIBS gdi32 user32 ole32 oleaut32 uuid)
    
    # Add standard vcpkg libraries that were found during configuration
    find_package(ZLIB REQUIRED)
    find_package(BZip2 REQUIRED)
    find_package(PNG REQUIRED)
    find_package(EXPAT REQUIRED)
    
    # Only add libraries that were actually found
    if(ZLIB_FOUND AND ZLIB_LIBRARIES)
        list(APPEND ALL_CAIRO_LIBS ${ZLIB_LIBRARIES})
    endif()
    if(BZIP2_FOUND AND BZIP2_LIBRARIES)
        list(APPEND ALL_CAIRO_LIBS ${BZIP2_LIBRARIES})
    endif()
    if(PNG_FOUND AND PNG_LIBRARIES)
        list(APPEND ALL_CAIRO_LIBS ${PNG_LIBRARIES})
    endif()
    if(EXPAT_FOUND AND EXPAT_LIBRARIES)
        list(APPEND ALL_CAIRO_LIBS ${EXPAT_LIBRARIES})
    endif()
    
    # CRITICAL FIX: Set include directories multiple ways to ensure MSVC finds headers
    set(VCPKG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/include")
    
    # Method 1: Standard include directories
    if(CAIRO_INCLUDE_DIRS)
        target_include_directories(blackbox_render PRIVATE ${CAIRO_INCLUDE_DIRS})
        message(STATUS "✓ Cairo include directory set: ${CAIRO_INCLUDE_DIRS}")
    endif()
    if(FREETYPE_INCLUDE_DIRS)
        target_include_directories(blackbox_render PRIVATE ${FREETYPE_INCLUDE_DIRS})
        message(STATUS "✓ FreeType include directory set: ${FREETYPE_INCLUDE_DIRS}")
    endif()
    
    # Method 2: Direct vcpkg include path (not as external)
    if(EXISTS ${VCPKG_INCLUDE_DIR})
        target_include_directories(blackbox_render PRIVATE ${VCPKG_INCLUDE_DIR})
        message(STATUS "✓ vcpkg include directory added as PRIVATE: ${VCPKG_INCLUDE_DIR}")
    endif()
    
    # Method 3: Force compiler flags for includes (bypass external issue)
    if(EXISTS ${VCPKG_INCLUDE_DIR})
        target_compile_options(blackbox_render PRIVATE "/I${VCPKG_INCLUDE_DIR}")
        message(STATUS "✓ Direct /I compiler flag added: /I${VCPKG_INCLUDE_DIR}")
        
        # Also add direct include for cairo subdirectory if it exists
        set(CAIRO_SUBDIR "${VCPKG_INCLUDE_DIR}/cairo")
        if(EXISTS ${CAIRO_SUBDIR})
            target_compile_options(blackbox_render PRIVATE "/I${CAIRO_SUBDIR}")
            message(STATUS "✓ Direct /I compiler flag for cairo subdir: /I${CAIRO_SUBDIR}")
        endif()
    endif()
    
    # Method 4: Ensure cairo.h can be found with explicit path verification
    set(CAIRO_HEADER_FILE "${VCPKG_INCLUDE_DIR}/cairo/cairo.h")
    set(CAIRO_HEADER_FILE_DIRECT "${VCPKG_INCLUDE_DIR}/cairo.h")
    if(EXISTS ${CAIRO_HEADER_FILE})
        message(STATUS "✓ VERIFIED: cairo.h exists at ${CAIRO_HEADER_FILE}")
        # Also add the cairo subdirectory to includes so #include <cairo.h> works
        target_include_directories(blackbox_render PRIVATE "${VCPKG_INCLUDE_DIR}/cairo")
        message(STATUS "✓ Added cairo subdirectory to includes: ${VCPKG_INCLUDE_DIR}/cairo")
        
        # Collect additional static linking dependencies
        set(ADDITIONAL_CAIRO_LIBS)
        
        # Add Cairo dependency libraries for Windows static linking
        if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES "static")
            message(STATUS "✓ Adding Windows static dependencies for Cairo")
            
            # Set the correct vcpkg library path
            set(VCPKG_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows-static/lib")
            
            # Pixman library (required by Cairo)
            find_library(PIXMAN_LIBRARY 
                NAMES pixman-1 pixman
                PATHS ${VCPKG_LIB_DIR}
                NO_DEFAULT_PATH
            )
            
            if(PIXMAN_LIBRARY)
                list(APPEND ADDITIONAL_CAIRO_LIBS ${PIXMAN_LIBRARY})
                message(STATUS "✓ Pixman library found: ${PIXMAN_LIBRARY}")
            else()
                message(STATUS "⚠ Pixman library not found, may cause linking issues")
            endif()
            
            # FontConfig library (required by Cairo)
            find_library(FONTCONFIG_LIBRARY 
                NAMES fontconfig
                PATHS ${VCPKG_LIB_DIR}
                NO_DEFAULT_PATH
            )
            if(FONTCONFIG_LIBRARY)
                list(APPEND ADDITIONAL_CAIRO_LIBS ${FONTCONFIG_LIBRARY})
                message(STATUS "✓ FontConfig library found: ${FONTCONFIG_LIBRARY}")
            else()
                message(STATUS "⚠ FontConfig library not found, may cause linking issues")
            endif()
            
            # Expat is already found via find_package(EXPAT REQUIRED) above
            message(STATUS "✓ Expat library found: ${EXPAT_LIBRARIES}")
            
            # Add additional libraries to the main list
            list(APPEND ALL_CAIRO_LIBS ${ADDITIONAL_CAIRO_LIBS})
            message(STATUS "✓ Windows system libraries added for Cairo")
        endif()
        
        # SINGLE CONSOLIDATED LINKING CALL - all libraries at once
        target_link_libraries(blackbox_render PRIVATE 
            ${ALL_CAIRO_LIBS}
            ${ALL_SYSTEM_LIBS}
        )
    else()
        message(STATUS "❌ Cairo not found - blackbox_render will not be built")
    endif()
else()
    # Unix systems with pkg-config
    target_link_libraries(blackbox_render PRIVATE ${CAIRO_LIBRARIES} ${FREETYPE_LIBRARIES})
    target_include_directories(blackbox_render PRIVATE ${CAIRO_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
    target_compile_options(blackbox_render PRIVATE ${CAIRO_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})
endif()

message(STATUS "✓ blackbox_render linking configured")

# Encoder testbed executable
add_executable(encoder_testbed ${ENCODER_TESTBED_SOURCES})

# Common linking for all executables
if(WIN32 AND MSVC)
    # Windows system libraries
    target_link_libraries(blackbox_decode ws2_32)
    target_link_libraries(encoder_testbed ws2_32)
else()
    # Unix-like systems
    target_link_libraries(blackbox_decode m pthread)
    target_link_libraries(encoder_testbed m pthread)
    target_link_libraries(blackbox_render m pthread)
endif()

# Set output directory
set_target_properties(blackbox_decode encoder_testbed blackbox_render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Version information
if(BLACKBOX_VERSION)
    target_compile_definitions(blackbox_decode PRIVATE BLACKBOX_VERSION=${BLACKBOX_VERSION})
    target_compile_definitions(encoder_testbed PRIVATE BLACKBOX_VERSION=${BLACKBOX_VERSION})
    target_compile_definitions(blackbox_render PRIVATE BLACKBOX_VERSION=${BLACKBOX_VERSION})
endif()
