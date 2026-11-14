# 构建 slotbound_trainer.exe 及注入用的 DLL / 工具。优先用 VS Build Tools 自带的 CMake，没有就用 PATH 里的 cmake（CI）。
# 用法：.\build.ps1            # Release，配置用仓库里的 config/trainer.ini
#       .\build.ps1 -Debug
#       .\build.ps1 -Dist      # 便携版 → dist\：修改器 + slotbound_lua.dll + 工具 + trainer.ini，配置用 exe 旁边的 trainer.ini
param([switch]$Debug, [switch]$Dist)

$ErrorActionPreference = 'Stop'
$cmake = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $candidate = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $candidate) { $cmake = $candidate }
    }
}
if (-not $cmake) {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $onPath) { throw '找不到 cmake：装 VS Build Tools（含 C++ CMake 工具）或把 cmake 加进 PATH' }
    $cmake = $onPath.Source
}

$config = if ($Debug -and -not $Dist) { 'Debug' } else { 'Release' }
$buildDir = if ($Dist) { "$PSScriptRoot\build-dist" } else { "$PSScriptRoot\build" }
$portable = if ($Dist) { 'ON' } else { 'OFF' }

& $cmake -S $PSScriptRoot -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DSB_PORTABLE=$portable"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
# 便携版不要 slotbound_probe（开发期诊断工具）
$target = if ($Dist) { @('--target', 'slotbound_trainer', 'slotbound_lua', 'slotbound_inject', 'slotbound_lua_cli') } else { @() }
& $cmake --build $buildDir --config $config @target
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if (-not $Dist) {
    Write-Host "`n产物：$buildDir\$config\slotbound_trainer.exe（另有 slotbound_probe.exe、slotbound_lua.dll 等）"
    exit 0
}

# 便携版：修改器找注入 DLL 是在 exe 同目录，所以 DLL 必须一起放
$distDir = "$PSScriptRoot\dist"
New-Item -ItemType Directory -Force $distDir | Out-Null
foreach ($f in 'slotbound_trainer.exe', 'slotbound_lua.dll', 'slotbound_inject.exe', 'slotbound_luacmd.exe') {
    Copy-Item "$buildDir\$config\$f" $distDir -Force
}
Copy-Item "$PSScriptRoot\README.md" $distDir -Force
# 配置：dist 里已有就不覆盖（保留那边的改动）；本机优先带仓库里的 trainer.ini，CI 上没有就用示例
if (-not (Test-Path "$distDir\trainer.ini")) {
    $repoIni = "$PSScriptRoot\config\trainer.ini"
    $src = if (Test-Path $repoIni) { $repoIni } else { "$PSScriptRoot\config\trainer.example.ini" }
    Copy-Item $src "$distDir\trainer.ini"
}
Write-Host "`n产物：$distDir\（slotbound_trainer.exe + slotbound_lua.dll + 工具；配置是同目录 trainer.ini）"
