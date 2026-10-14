cmake_minimum_required(VERSION 3.15)
project(ScarfaceEX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Force 32-bit build
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project requires 32-bit (x86) build. Please configure with -A Win32")
endif()

# Source files
set(SOURCES
    ScarfaceEX.cpp
)

# Create DLL
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Output as .asi instead of .dll (for ASI Loader compatibility)
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ScarfaceEX"
    SUFFIX ".asi"
    PREFIX ""
)

# Find XInput library
if(MSVC)
    # Visual Studio: XInput is in Windows SDK
    find_library(XINPUT_LIBRARY 
        NAMES xinput xinput1_4 xinput1_3 xinput9_1_0
        PATHS
            "C:/Program Files (x86)/Windows Kits/10/Lib/*/um/x86"
            "C:/Program Files (x86)/Windows Kits/8.1/Lib/winv6.3/um/x86"
            "C:/Program Files (x86)/Microsoft SDKs/Windows/*/Lib/x86"
    )
    
    if(NOT XINPUT_LIBRARY)
        message(STATUS "XInput library not found, using default linking")
        set(XINPUT_LIBRARY "xinput")
    endif()
else()
    # MinGW: Need to find or provide XInput
    find_library(XINPUT_LIBRARY 
        NAMES xinput1_4 xinput1_3 xinput9_1_0 xinput
        PATHS
            ${CMAKE_SOURCE_DIR}/lib
            /mingw32/lib
            /mingw64/lib
    )
    
    if(NOT XINPUT_LIBRARY)
        message(STATUS "XInput library not found in standard locations")
        message(STATUS "Attempting to use system XInput...")
        set(XINPUT_LIBRARY "xinput1_4")
    endif()
endif()

message(STATUS "Using XInput library: ${XINPUT_LIBRARY}")

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Psapi
    ${XINPUT_LIBRARY}
    dinput8
    dxguid
)

# Include directories for XInput headers
if(MINGW)
    # For MinGW, add Windows SDK include paths if available
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        "C:/Program Files (x86)/Windows Kits/10/Include/*/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/*/shared"
    )
endif()

# Compiler flags
if(MSVC)
    # Visual Studio specific flags
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3          # Warning level 3
        /MP          # Multi-processor compilation
        /EHsc        # Exception handling
    )
    
    # Linker flags
    target_link_options(${PROJECT_NAME} PRIVATE
        /MACHINE:X86
    )
else()
    # MinGW/GCC flags
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -m32
    )
    
    target_link_options(${PROJECT_NAME} PRIVATE
        -m32
        -static-libgcc
        -static-libstdc++
    )
    
    # For MinGW, try to link against system XInput
    if(NOT EXISTS ${XINPUT_LIBRARY})
        message(STATUS "Linking against system xinput1_4.dll")
        target_link_options(${PROJECT_NAME} PRIVATE
            -lxinput1_4
        )
    endif()
endif()

# Post-build: Copy config template if it doesn't exist
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<CONFIG>/${PROJECT_NAME}.asi"
)

# Installation (optional)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
