[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$taskRepository = Split-Path -Parent $PSScriptRoot
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) {
    throw 'Nie znaleziono vswhere.exe. Do odswiezenia EXE potrzebne jest Visual Studio z narzedziami C++.'
}
$taskInstallation = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskInstallation) {
    throw 'Nie znaleziono instalacji Visual Studio z narzedziami C++.'
}
$taskBuildTool = Join-Path $taskInstallation 'MSBuild\Current\Bin\MSBuild.exe'
$taskProject = Join-Path $taskRepository 'Projecr1_GK.vcxproj'
& $taskBuildTool $taskProject /nologo /m /verbosity:minimal /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "Budowanie Release x64 nie powiodlo sie (kod $LASTEXITCODE). Dotychczasowa paczka pozostala bez zmian."
}

$taskSource = Join-Path $taskRepository 'x64\Release\Projecr1_GK.exe'
if (-not (Test-Path -LiteralPath $taskSource)) {
    throw 'Po kompilacji brakuje oczekiwanego pliku EXE.'
}
$taskPackage = Join-Path $taskRepository 'dist\windows-x64'
[System.IO.Directory]::CreateDirectory($taskPackage) | Out-Null
$taskDestination = Join-Path $taskPackage 'Projecr1_GK.exe'
Copy-Item -LiteralPath $taskSource -Destination $taskDestination -Force
$taskHash = (Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path $taskPackage 'SHA256SUMS.txt'),
    "$taskHash  Projecr1_GK.exe`r`n", [System.Text.Encoding]::ASCII)
Write-Output "Gotowy program: $taskDestination"
Write-Output "SHA256: $taskHash"
