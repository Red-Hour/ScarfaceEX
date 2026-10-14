@echo off
setlocal EnableDelayedExpansion

echo ========================
echo ScarfaceEX: Compiler
echo ========================
echo.

REM Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found in PATH!
    echo Please install CMake from https://cmake.org/download/
    echo.
    pause
    exit /b 1
)

echo [1/4] CMake found: OK
echo.

REM Create build directory
echo [2/4] Creating build directory...
if not exist build mkdir build
cd build

REM Configure with CMake
echo.
echo [3/4] Configuring project with CMake...
echo.

REM Try Visual Studio first
cmake .. -G "Visual Studio 18 2026" -A Win32

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Visual Studio 2026 not found, trying 2022...
    echo.
    
    cmake .. -G "Visual Studio 17 2022" -A Win32
    
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo Visual Studio 2022 not found, trying 2019...
        echo.
        
        cmake .. -G "Visual Studio 16 2019" -A Win32
        
        if %ERRORLEVEL% NEQ 0 (
            echo.
			echo Visual Studio 2019 not found, trying 2017...
			echo.
			
			cmake .. -G "Visual Studio 15 2017" -A Win32
			
			if %ERRORLEVEL% NEQ 0 (
				echo.
				echo All Visual Studio versions failed.
				echo Trying MinGW Makefiles with custom XInput...
				echo.
				
				REM For MinGW, use custom XInput header
				cmake .. -G "MinGW Makefiles" ^
					-DCMAKE_C_FLAGS="-m32" ^
					-DCMAKE_CXX_FLAGS="-m32 -I../include" ^
					-DCMAKE_BUILD_TYPE=Release
				
				if %ERRORLEVEL% NEQ 0 (
					echo.
					echo ========================================
					echo ERROR: CMake configuration failed!
					echo ========================================
					echo.
					echo Please ensure you have either:
					echo   - Visual Studio 2017 or newer, OR
					echo   - MinGW-w64 with make
					echo.
					cd ..
					pause
					exit /b 1
				)
			)
        )
    )
)

echo.
echo ========================================
echo Configuration successful!
echo ========================================
echo.

REM Build the project
echo [4/4] Building Release configuration...
echo.
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo ERROR: Build failed!
    echo ========================================
    echo.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.

REM Find the output file
if exist "bin\Release\ScarfaceEX.asi" (
    echo Output: build\bin\Release\ScarfaceEX.asi
) else if exist "bin\ScarfaceEX.asi" (
    echo Output: build\bin\ScarfaceEX.asi
) else (
    echo Output: Check build\bin\ directory
)

echo.
echo The .asi file can be used with ASI Loader
echo Place it in your Scarface game directory
echo.

cd ..
pause
