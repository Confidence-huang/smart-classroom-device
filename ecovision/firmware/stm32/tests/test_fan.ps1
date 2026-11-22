param([string]$EvidenceName = '2026-11-21')
$ErrorActionPreference = 'Stop'
$appRoot = Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
$evidence = Join-Path (Split-Path $PSScriptRoot) "tb6612-evidence/$EvidenceName"
$null = New-Item -ItemType Directory -Force -Path $evidence
$batch = Join-Path $env:TEMP 'ecovision-fan-host.cmd'
$batchLines = @('@echo off', 'chcp 65001 >nul',
  'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul',
  ('cd /d "' + $env:TEMP + '"'),
  ('cl /nologo /TC /std:c11 /W4 /WX /I"' + $PSScriptRoot + '\tb6612_host" /I"' + $appRoot + '\Inc" "' + $PSScriptRoot + '\test_tb6612.c" "' + $appRoot + '\Src\app_fan.c" /Fe:ecovision_fan_host.exe'),
  'if errorlevel 1 exit /b 1',
  'ecovision_fan_host.exe')
[System.IO.File]::WriteAllLines($batch, $batchLines, (New-Object System.Text.UTF8Encoding($false)))
& cmd.exe /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $evidence 'host.log')
if ($LASTEXITCODE -ne 0) { throw 'Fan host test failed' }
