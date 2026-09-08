#requires -Version 7.0
<#
.SYNOPSIS
Build native CMake targets and .NET loaders using the same entry point as CI.
.EXAMPLE
pwsh ./Tools/build.ps1 -Configuration Release -Test
.EXAMPLE
pwsh ./Tools/build.ps1 -Component Native -Preset windows-vs2026-debug
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateSet('All', 'Native', 'Managed')][string]$Component = 'All',
    [ValidateSet('windows-debug', 'windows-release', 'windows-vs2026-debug', 'windows-vs2026-release', 'linux-debug', 'linux-release')]
    [string]$Preset,
    [ValidateRange(1, 1024)][int]$Jobs = [Environment]::ProcessorCount,
    [string[]]$CMakeArgs = @(),
    [switch]$ConfigureOnly,
    [switch]$Test,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-BuildCommand {
    param([string]$Command, [string[]]$Arguments)
    Write-Host "$Command $($Arguments -join ' ')"
    if (-not $DryRun) {
        & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Command failed with exit code $LASTEXITCODE"
        }
    }
}

if ($ConfigureOnly -and ($Test -or $Component -eq 'Managed')) {
    throw '-ConfigureOnly requires native builds and cannot be combined with -Test.'
}

Push-Location $repoRoot
try {
    if ($Component -ne 'Managed') {
        if (-not $Preset) {
            if ($IsWindows) {
                $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
                if (-not (Test-Path -LiteralPath $vswhere)) {
                    throw 'Install Visual Studio 2022 or 2026 with Desktop development with C++.'
                }
                $vsVersion = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
                if (-not $vsVersion) { throw 'Visual Studio C++ tools were not found.' }
                $prefix = if ($vsVersion -match '^18\.') { 'windows-vs2026' } else { 'windows' }
            } elseif ($IsLinux) {
                $prefix = 'linux'
            } else {
                throw 'Native builds currently support Windows x64 and Linux x64.'
            }
            $Preset = "$prefix-$($Configuration.ToLowerInvariant())"
        }
        $presetConfiguration = if ($Preset.EndsWith('-debug')) { 'Debug' } else { 'Release' }
        if ($PSBoundParameters.ContainsKey('Configuration') -and $Configuration -ne $presetConfiguration) {
            throw "Preset '$Preset' conflicts with configuration '$Configuration'."
        }
        $Configuration = $presetConfiguration
        Invoke-BuildCommand cmake (@('--preset', $Preset) + $CMakeArgs)
        if (-not $ConfigureOnly) {
            Invoke-BuildCommand cmake @('--build', '--preset', $Preset, '--parallel', "$Jobs")
            if ($Test) { Invoke-BuildCommand ctest @('--preset', $Preset) }
        }
        Write-Host "Native output: intermediate/cmake/$Preset/bin/$Configuration"
    }

    if ($Component -ne 'Native' -and -not $ConfigureOnly) {
        $managedArgs = @('--configuration', $Configuration, '-p:Platform=x64')
        if ($IsWindows -and $Component -eq 'All') {
            $nativeOutput = Join-Path $repoRoot "intermediate/cmake/$Preset/bin/$Configuration"
            $managedArgs += "-p:RekindledNativeOutputDir=$nativeOutput"
        }
        $projects = @('Source/Loader.Avalonia/Loader.Avalonia.csproj')
        if ($IsWindows) { $projects = @('Source/Loader/Loader.csproj') + $projects }
        foreach ($project in $projects) {
            # dotnet build restores automatically; tests reuse that exact configuration.
            Invoke-BuildCommand dotnet (@('build', $project) + $managedArgs)
            if ($Test) {
                $projectDirectory = Split-Path -Parent $project
                $projectName = [IO.Path]::GetFileNameWithoutExtension($project)
                $testProject = "$projectDirectory/Tests/$projectName.Tests.csproj"
                Invoke-BuildCommand dotnet (@('test', $testProject) + $managedArgs + @(
                    '--verbosity', 'normal', '--blame-hang-timeout', '2m', '--blame-hang-dump-type', 'none'
                ))
            }
        }
    }
} finally {
    Pop-Location
}
