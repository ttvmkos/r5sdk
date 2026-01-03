@echo off
setlocal

set BUILDDIR=build_intermediate
set BINDIR=game

REM Check for Visual Studio versions in order (17, 16, 15)
for %%V in (17 16 15) do (
    reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.%%V.0" >> nul 2>&1
    if NOT ERRORLEVEL 1 (
        if "%%V"=="17" (
            set "CMAKE_GENERATOR=Visual Studio 17 2022"
            set "CMAKE_TOOLSET=-T v142"
        ) else if "%%V"=="16" (
            set "CMAKE_GENERATOR=Visual Studio 16 2019"
            set "CMAKE_TOOLSET=-T v142"
        ) else if "%%V"=="15" (
            set "CMAKE_GENERATOR=Visual Studio 15 2017"
            set "CMAKE_TOOLSET="
        )
        echo Using %CMAKE_GENERATOR% %CMAKE_TOOLSET%.
        goto :build
    )
)

echo Could not find a supported version of Visual Studio; exiting...
exit /b 1

:build
if not exist "%BUILDDIR%" (
    mkdir "%BUILDDIR%"
)
if not exist "%BINDIR%" (
    mkdir "%BINDIR%"
)

cd "%BUILDDIR%"
cmake .. -G"%CMAKE_GENERATOR%" %CMAKE_TOOLSET% -A"x64" -DOPTION_CERTAIN=ON
cd ..

echo Finished generating solution files.
