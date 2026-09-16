# Compatibility entry point. Build policy lives in build.ps1 and CMakePresets.json.
#requires -Version 7.0
param(
    [Alias('Configuration')][ValidateSet('Debug', 'Release')][string]$BuildType = 'Release',
    [string]$Preset,
    [string[]]$CMakeArgs = @(),
    [switch]$ConfigureOnly,
    [switch]$Test,
    [switch]$DryRun
)
$buildOptions = @{
    Component = 'Native'
    CMakeArgs = $CMakeArgs
    ConfigureOnly = $ConfigureOnly
    Test = $Test
    DryRun = $DryRun
}
if ($Preset) { $buildOptions.Preset = $Preset }
if ($PSBoundParameters.ContainsKey('BuildType')) { $buildOptions.Configuration = $BuildType }
& "$PSScriptRoot/build.ps1" @buildOptions
