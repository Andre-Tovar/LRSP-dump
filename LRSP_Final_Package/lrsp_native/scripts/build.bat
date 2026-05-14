@echo off
REM Build mespprc_native using the bundled VS Build Tools toolchain.
REM
REM Usage:
REM   scripts\build.bat              -- configure (if needed) and build
REM   scripts\build.bat clean        -- delete the build/ directory
REM   scripts\build.bat reconfigure  -- delete CMakeCache and reconfigure
REM
REM The script locates vcvarsall.bat by walking known install paths so it
REM works on both stock VS 2022 and VS 2026 BuildTools layouts.

setlocal

set "PKG_DIR=%~dp0.."
pushd "%PKG_DIR%" >nul

set "BUILD_DIR=%PKG_DIR%\build"

if /I "%~1"=="clean" (
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    popd >nul
    exit /b 0
)
if /I "%~1"=="reconfigure" (
    if exist "%BUILD_DIR%\CMakeCache.txt" del /q "%BUILD_DIR%\CMakeCache.txt"
)

REM Locate vcvarsall.bat. Prefer the version actually installed on the host.
set "VCVARS="
for %%P in (
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) do (
  if exist %%~P set "VCVARS=%%~P"
)
if not defined VCVARS (
    echo build.bat: could not locate vcvarsall.bat. Install VS Build Tools first.
    popd >nul
    exit /b 1
)

call "%VCVARS%" x64 >nul
if errorlevel 1 (
    echo build.bat: vcvarsall.bat failed.
    popd >nul
    exit /b 1
)

REM Locate cmake/ninja. Try the bundled VS copies first, then PATH.
set "CMAKE_EXE=cmake"
set "NINJA_EXE=ninja"
for %%P in (
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) do (
  if exist %%~P set "CMAKE_EXE=%%~P"
)
for %%P in (
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
) do (
  if exist %%~P set "NINJA_EXE=%%~P"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    "%CMAKE_EXE%" -S "%PKG_DIR%" -B "%BUILD_DIR%" -G Ninja ^
        -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if errorlevel 1 (
        popd >nul
        exit /b 1
    )
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config RelWithDebInfo
if errorlevel 1 (
    popd >nul
    exit /b 1
)

popd >nul
exit /b 0
