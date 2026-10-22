param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) {
    throw 'Nie znaleziono vswhere.exe. Zainstaluj Visual Studio z narzedziami C++.'
}

$taskInstallation = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskInstallation) {
    throw 'Nie znaleziono instalacji Visual Studio z narzedziami C++.'
}

$taskBuildTool = Join-Path $taskInstallation 'MSBuild\Current\Bin\MSBuild.exe'
$taskProject = Join-Path $PSScriptRoot 'cad_core_tests.vcxproj'
& $taskBuildTool $taskProject /nologo /m /verbosity:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$taskExecutable = Join-Path $PSScriptRoot "$Platform\$Configuration\cad_core_tests.exe"
& $taskExecutable
exit $LASTEXITCODE
