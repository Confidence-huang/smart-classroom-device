$ErrorActionPreference = 'Stop'
$appRoot = Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
$evidence = Join-Path (Split-Path $PSScriptRoot) 'bh1750-evidence/2026-11-21'
$null = New-Item -ItemType Directory -Force -Path $evidence
$batch = Join-Path $env:TEMP 'ecovision-bh1750-host.cmd'
$lines = @('@echo off', 'chcp 65001 >nul',
 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul',
 ('cd /d "' + $env:TEMP + '"'),
 ('cl /nologo /utf-8 /D_CRT_SECURE_NO_WARNINGS /TC /std:c11 /W4 /WX /I"' + $PSScriptRoot + '\bh1750_host" /I"' + $appRoot + '\Inc" "' + $PSScriptRoot + '\test_bh1750.c" "' + $appRoot + '\Src\app_bh1750.c" "' + $appRoot + '\Src\lcd_view.c" /Fe:ecovision_bh1750_host.exe'),
 'if errorlevel 1 exit /b 1', 'ecovision_bh1750_host.exe')
[System.IO.File]::WriteAllLines($batch, $lines, (New-Object System.Text.UTF8Encoding($false)))
& cmd.exe /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $evidence 'host.log')
if ($LASTEXITCODE -ne 0) { throw 'BH1750 host test failed' }
