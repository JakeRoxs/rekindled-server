@echo off

SET ScriptPath=%~dp0
SET RootPath=%ScriptPath%..\
SET BuildPath=%ScriptPath%..\intermediate\vs2022\
SET CMakeExePath=%ScriptPath%Build\cmake\windows\bin\cmake.exe

if not exist "%CMakeExePath%" (
    for /f "delims=" %%i in ('where cmake 2^>nul') do set "CMakeExePath=%%i" & goto :foundCMake
)
:foundCMake
if not exist "%CMakeExePath%" (
    echo ERROR: cmake not found. Install cmake or set PATH to cmake.exe.
    exit /b 1
)

echo Generating %RootPath%
echo %CMakeExePath% -S %RootPath% -B %BuildPath%

"%CMakeExePath%" -S "%RootPath%" -B "%BuildPath%" -G "Visual Studio 17 2022"
