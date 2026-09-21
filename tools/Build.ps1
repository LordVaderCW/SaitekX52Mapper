param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [switch]$Hardware)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ x64 tools were not found.' }
$msbuild=Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
Push-Location $root
try {
    & $msbuild SaitekX52Mapper.sln /m "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw "Build failed ($LASTEXITCODE)" }
    $tests=Join-Path $root "out\x64\$Configuration\X52Tests.exe"
    if ($Hardware) { & $tests --hardware } else { & $tests }
    if ($LASTEXITCODE) { throw "Tests failed ($LASTEXITCODE)" }
} finally { Pop-Location }
