# 构建 slotbound_trainer.exe / slotbound_probe.exe。用 VS Build Tools 自带的 CMake，不需要它在 PATH 里。
# 用法：.\build.ps1            # Release，配置用仓库里的 config/trainer.ini
#       .\build.ps1 -Debug
#       .\build.ps1 -Dist      # 便携版单个 exe → dist\slotbound_trainer.exe，配置用 exe 旁边的 trainer.ini
param([switch]$Debug, [switch]$Dist)

$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { throw "找不到 VS 自带的 cmake：$cmake" }

$config = if ($Debug -and -not $Dist) { 'Debug' } else { 'Release' }
$buildDir = if ($Dist) { "$PSScriptRoot\build-dist" } else { "$PSScriptRoot\build" }
$portable = if ($Dist) { 'ON' } else { 'OFF' }

& $cmake -S $PSScriptRoot -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DSB_PORTABLE=$portable"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$target = if ($Dist) { @('--target', 'slotbound_trainer') } else { @() }
& $cmake --build $buildDir --config $config @target
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if (-not $Dist) {
    Write-Host "`n产物：$buildDir\$config\slotbound_trainer.exe（另有 slotbound_probe.exe）"
    exit 0
}

# 便携版：exe 放进 dist\；第一次打包时把仓库里的配置带过去（已有就不覆盖，保留那边的改动）
$distDir = "$PSScriptRoot\dist"
New-Item -ItemType Directory -Force $distDir | Out-Null
Copy-Item "$buildDir\$config\slotbound_trainer.exe" $distDir -Force
$repoIni = "$PSScriptRoot\config\trainer.ini"
if (-not (Test-Path "$distDir\trainer.ini") -and (Test-Path $repoIni)) {
    Copy-Item $repoIni "$distDir\trainer.ini"
}
Write-Host "`n产物：$distDir\slotbound_trainer.exe（配置：同目录 trainer.ini，没有就用默认值并自动生成）"
