param([string]$EvidenceDir='')
$ErrorActionPreference='Stop'
$app=Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
if(-not $EvidenceDir){$EvidenceDir=Join-Path (Split-Path $PSScriptRoot) 'net-evidence/2026-11-22/recovery-build'}
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$EvidenceDir=(Resolve-Path -LiteralPath $EvidenceDir).Path
$batch=Join-Path $env:TEMP ('ecovision-recovery-'+[guid]::NewGuid()+'.cmd')
$lines=@('@echo off','call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul',
 ('cd /d "'+$EvidenceDir+'"'),
 ('cl /nologo /TC /std:c11 /W4 /WX /utf-8 /I"'+$app+'/Inc" "'+$PSScriptRoot+'/test_net_recovery.c" "'+$app+'/Src/net_recovery.c" /Fe:net_recovery.exe'),
 'if errorlevel 1 exit /b 1','net_recovery.exe')
[System.IO.File]::WriteAllLines($batch,$lines,(New-Object System.Text.UTF8Encoding($false)))
& cmd.exe /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $EvidenceDir 'host-at-recovery.log')
if($LASTEXITCODE -ne 0){throw 'AT recovery checks failed'}
