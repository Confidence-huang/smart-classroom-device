$ErrorActionPreference='Stop'
$app=Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
$out=Join-Path (Split-Path $PSScriptRoot) 'ir-merge-evidence/2026-11-23'
$tmp=Join-Path $env:TEMP 'ecovision-ir-pulse-tests'
New-Item -ItemType Directory -Force -Path $tmp,$out | Out-Null
$lines=@('@echo off','chcp 65001 >nul',
 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul',
 ('cd /d "'+$tmp+'"'))
foreach($name in @('decode','encode')) {
 $lines+=('cl /nologo /utf-8 /TC /std:c11 /W4 /WX /I"'+$app+'\Inc" "'+$PSScriptRoot+'\test_ir_pulse_'+$name+'.c" "'+$app+'\Src\ir_pulse_codec.c" /Fe:test_'+$name+'.exe')
 $lines+='if errorlevel 1 exit /b 1'
 $lines+=('test_'+$name+'.exe')
 $lines+='if errorlevel 1 exit /b 1'
}
$batch=Join-Path $tmp 'run.cmd'
$lines+=('cl /nologo /utf-8 /TC /std:c11 /W4 /WX /I"'+$PSScriptRoot+'\ir_pulse_host" /I"'+$app+'\Inc" "'+$PSScriptRoot+'\test_ir_pulse_driver.c" "'+$app+'\Src\app_ir_pulse.c" "'+$app+'\Src\ir_pulse_codec.c" /Fe:test_driver.exe')
$lines+='if errorlevel 1 exit /b 1'
$lines+='test_driver.exe'
$lines+='if errorlevel 1 exit /b 1'
[IO.File]::WriteAllLines($batch,$lines,(New-Object Text.UTF8Encoding($false)))
& cmd.exe /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $out 'host-codec.log')
if($LASTEXITCODE -ne 0){throw 'IR pulse tests failed'}
