param(
    [string]$EvidenceDir = '',
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)
$ErrorActionPreference='Stop'
$app=Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
if(-not $EvidenceDir) { $EvidenceDir=Join-Path (Split-Path $PSScriptRoot) 'control-evidence/2026-11-22/build' }
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$EvidenceDir=(Resolve-Path -LiteralPath $EvidenceDir).Path
if(-not (Test-Path -LiteralPath $VcVars)) { throw 'MSVC vcvars64.bat missing; pass -VcVars with the local tool path' }
$batch=Join-Path $env:TEMP ('ecovision-control-'+[guid]::NewGuid()+'.cmd')
$lines=@('@echo off',('call "'+$VcVars+'" >nul'),('cd /d "'+$EvidenceDir+'"'),
 ('cl /nologo /TC /std:c11 /W4 /WX /utf-8 /I"'+$app+'/Inc" "'+$PSScriptRoot+'/test_control_policy.c" "'+$app+'/Src/control_policy.c" /Fe:control_policy.exe'),
 'if errorlevel 1 exit /b 1','control_policy.exe','if errorlevel 1 exit /b 1',
 ('cl /nologo /TC /std:c11 /W4 /WX /utf-8 /I"'+$PSScriptRoot+'/control_host" /I"'+$app+'/Inc" "'+$PSScriptRoot+'/test_control_adapter.c" "'+$app+'/Src/control_policy.c" "'+$app+'/Src/app_control.c" /Fe:control_adapter.exe'),
 'if errorlevel 1 exit /b 1','control_adapter.exe')
[System.IO.File]::WriteAllLines($batch,$lines,(New-Object System.Text.UTF8Encoding($false)))
& cmd.exe /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $EvidenceDir 'host-control.log')
if($LASTEXITCODE -ne 0) { throw 'Control checks failed; see host-control.log' }
