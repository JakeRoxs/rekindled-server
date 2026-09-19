# Initialize the installed x64 MSVC toolchain and persist changes for later CI steps.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'Could not locate Visual Studio with the x64 C++ tools.'
}

$previousEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $previousEnvironment[$_.Name] = $_.Value }
& (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'MSVC x64 environment initialization failed.'
}

if ($env:GITHUB_ENV) {
    Get-ChildItem Env: | Where-Object { $previousEnvironment[$_.Name] -cne $_.Value } | ForEach-Object {
        $delimiter = "MSVC_$([Guid]::NewGuid().ToString('N'))"
        "$($_.Name)<<$delimiter`n$($_.Value)`n$delimiter" | Out-File -LiteralPath $env:GITHUB_ENV -Encoding utf8 -Append
    }
}
