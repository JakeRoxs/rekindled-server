@echo off
setlocal
:: Assumed to be run from root directory.
pushd "%~dp0\.." >nul 2>nul
if errorlevel 1 (
    echo ERROR: Failed to change directory to repository root.
    exit /b 1
)

if not defined PACKAGE_BUILD_CONFIGURATION set PACKAGE_BUILD_CONFIGURATION=Release
if not defined PACKAGE_PLATFORM set PACKAGE_PLATFORM=x64
if not defined LOADER_WINDOWS_RUNTIME set LOADER_WINDOWS_RUNTIME=win-x64
if not defined LOADER_WINDOWS_PUBLISH_DIR set LOADER_WINDOWS_PUBLISH_DIR=intermediate\publish\canonical\Loader
if not defined LOADER_AVALONIA_WINDOWS_PUBLISH_DIR set LOADER_AVALONIA_WINDOWS_PUBLISH_DIR=intermediate\publish\canonical\Loader.Avalonia

if not defined NATIVE_BUILD_PRESET set NATIVE_BUILD_PRESET=windows-release
if not defined NATIVE_OUTPUT_ROOT set "NATIVE_OUTPUT_ROOT=intermediate\cmake\%NATIVE_BUILD_PRESET%\bin\%PACKAGE_BUILD_CONFIGURATION%"
set "OUTPUT_ROOT=%NATIVE_OUTPUT_ROOT%"
if not exist "%OUTPUT_ROOT%\Server.exe" (
    echo ERROR: Server.exe was not found in %OUTPUT_ROOT%. Build the selected preset first.
    exit /b 1
)

if /I not "%SKIP_DOTNET_PUBLISH%"=="1" (
    echo Publishing loader artifacts to %LOADER_WINDOWS_PUBLISH_DIR%...
    dotnet publish Source\Loader\Loader.csproj --configuration %PACKAGE_BUILD_CONFIGURATION% --runtime %LOADER_WINDOWS_RUNTIME% --self-contained true -p:Platform=%PACKAGE_PLATFORM% -p:UseAppHost=true -p:PublishSingleFile=false -o "%LOADER_WINDOWS_PUBLISH_DIR%"
    if errorlevel 1 exit /b 1

    echo Publishing Loader.Avalonia artifacts to %LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%...
    dotnet publish Source\Loader.Avalonia\Loader.Avalonia.csproj --configuration %PACKAGE_BUILD_CONFIGURATION% --runtime %LOADER_WINDOWS_RUNTIME% --self-contained true -p:Platform=%PACKAGE_PLATFORM% -p:UseAppHost=true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=None -p:DebugSymbols=false -o "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%"
    if errorlevel 1 exit /b 1
)

echo Using package source output path: %OUTPUT_ROOT%
echo Packaging into: %CD%\rekindled-server

if exist "rekindled-server" rmdir /s /q "rekindled-server"
mkdir "rekindled-server"
mkdir "rekindled-server\Loader"
mkdir "rekindled-server\Server"
mkdir "rekindled-server\Prerequisites"
echo Including: Resources\ReadMe.txt -> rekindled-server\ReadMe.txt
copy Resources\ReadMe.txt rekindled-server\ReadMe.txt
if exist "Resources\Prerequisites" (
    echo Including directory: Resources\Prerequisites -> rekindled-server\Prerequisites
    xcopy /s Resources\Prerequisites rekindled-server\Prerequisites
)

set ERR=0

if exist "%OUTPUT_ROOT%\steam_appid.txt" (
    xcopy /s "%OUTPUT_ROOT%\steam_appid.txt" rekindled-server\Server\
) else if exist "Resources\steam_appid.txt" (
    xcopy /s "Resources\steam_appid.txt" rekindled-server\Server\
) else (
    echo WARNING: steam_appid.txt not found in %OUTPUT_ROOT% or Resources
    set ERR=1
)
if exist "%OUTPUT_ROOT%\steam_api64.dll" (
    xcopy /s "%OUTPUT_ROOT%\steam_api64.dll" rekindled-server\Server\
) else if exist "Source\ThirdParty\steam\redistributable_bin\win64\steam_api64.dll" (
    xcopy /s "Source\ThirdParty\steam\redistributable_bin\win64\steam_api64.dll" rekindled-server\Server\
) else (
    echo WARNING: steam_api64.dll not found in %OUTPUT_ROOT% or source tree
    set ERR=1
)
if exist "%OUTPUT_ROOT%\WebUI" (
    xcopy /s "%OUTPUT_ROOT%\WebUI\" rekindled-server\Server\WebUI\
) else if exist "Source\WebUI" (
    xcopy /s "Source\WebUI\" rekindled-server\Server\WebUI\
) else (
    echo WARNING: WebUI folder not found in %OUTPUT_ROOT% or Source\WebUI
    set ERR=1
)
if exist "%OUTPUT_ROOT%\Server.exe" (
    copy /y "%OUTPUT_ROOT%\Server.exe" "rekindled-server\Server\"
) else (
    echo ERROR: Server.exe not found in %OUTPUT_ROOT%
    set ERR=1
)
if exist "%OUTPUT_ROOT%\Server.pdb" (
    copy /y "%OUTPUT_ROOT%\Server.pdb" "rekindled-server\Server\"
) else (
    echo INFO: Server.pdb not found in %OUTPUT_ROOT%
)

if exist "%LOADER_WINDOWS_PUBLISH_DIR%\Loader.exe" (
    xcopy /s "%LOADER_WINDOWS_PUBLISH_DIR%\*" rekindled-server\Loader\
    goto :loader_done
) else if exist "%LOADER_WINDOWS_PUBLISH_DIR%\Loader.dll" (
    xcopy /s "%LOADER_WINDOWS_PUBLISH_DIR%\*" rekindled-server\Loader\
    goto :loader_done
) else (
    echo WARNING: Loader.exe or Loader.dll not found in canonical publish dir: %LOADER_WINDOWS_PUBLISH_DIR%
    set ERR=1
)
:loader_done

mkdir rekindled-server\Loader.Avalonia

if exist "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\Loader.Avalonia.exe" (
    xcopy /s "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\*" rekindled-server\Loader.Avalonia\
    goto :loader_avalonia_done
) else if exist "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\Loader.Avalonia.dll" (
    xcopy /s "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\*" rekindled-server\Loader.Avalonia\
    goto :loader_avalonia_done
) else (
    echo WARNING: Loader.Avalonia.exe or Loader.Avalonia.dll not found in canonical publish dir: %LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%
    set ERR=1
)
:loader_avalonia_done

if exist "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\Loader.Avalonia.pdb" (
    xcopy /s "%LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%\Loader.Avalonia.pdb" rekindled-server\Loader.Avalonia\
) else (
    echo INFO: Loader.Avalonia.pdb not found in canonical publish dir %LOADER_AVALONIA_WINDOWS_PUBLISH_DIR%; continuing without symbols.
)

if exist "%OUTPUT_ROOT%\Injector.pdb" (
    copy /y "%OUTPUT_ROOT%\Injector.pdb" "rekindled-server\Loader\"
) else (
    echo INFO: Injector.pdb not found in %OUTPUT_ROOT%
)
if exist "%OUTPUT_ROOT%\Injector.dll" (
    copy /y "%OUTPUT_ROOT%\Injector.dll" "rekindled-server\Loader\"
    copy /y "%OUTPUT_ROOT%\Injector.dll" "rekindled-server\Loader.Avalonia\"
) else (
    echo ERROR: Injector.dll not found in %OUTPUT_ROOT%
    set ERR=1
)

if "%ERR%"=="1" (
    echo ERROR: One or more required files were missing. Check build output path and CMake configuration.
    dir /b "%OUTPUT_ROOT%" 2>nul
    popd >nul 2>nul
    exit /b 1
) else (
    echo Package preparation succeeded.
    echo Packaged files being added to the zip:
    dir /b /s rekindled-server
    popd >nul 2>nul
)
