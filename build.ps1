# 构建 slotbound_trainer.exe 及注入用的 DLL / 工具。优先用 VS Build Tools 自带的 CMake，没有就用 PATH 里的 cmake（CI）。
# 用法：.\build.ps1            # Release，配置用仓库里的 config/trainer.ini
#       .\build.ps1 -Debug
#       .\build.ps1 -Dist      # 便携版 → dist\slotbound_trainer.exe 一个文件（注入 DLL 已内嵌），配置用 exe 旁边的 trainer.ini（改设置时自动生成）
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
$target = if ($Dist) { @('--target', 'slotbound_trainer') } else { @() }
& $cmake --build $buildDir --config $config @target
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if (-not $Dist) {
    Write-Host "`n产物：$buildDir\$config\slotbound_trainer.exe（另有 slotbound_probe.exe、slotbound_lua.dll 等）"
    exit 0
}

# 便携版：只发一个 exe（slotbound_lua.dll 编在里面）。先清掉旧版留下的 DLL / 工具 / 说明，trainer.ini 保留（那是用户的设置）
$distDir = "$PSScriptRoot\dist"
New-Item -ItemType Directory -Force $distDir | Out-Null
Get-ChildItem $distDir -File | Where-Object { $_.Name -ne 'trainer.ini' } | Remove-Item -Force
Copy-Item "$buildDir\$config\slotbound_trainer.exe" $distDir -Force
Write-Host "`n产物：$distDir\slotbound_trainer.exe（单文件；配置是同目录 trainer.ini，没有就用默认值，改设置时自动生成）"
