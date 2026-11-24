$ErrorActionPreference='Stop'
$app=Join-Path (Split-Path $PSScriptRoot) 'EcoVision_F413ZH/App'
$out=Join-Path $env:TEMP ('ecovision-opt-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path $out | Out-Null
$lines=@('@echo off','chcp 65001 >nul','call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul',('cd /d "'+$out+'"'))
$cases=@(
 @('vision_protocol','vision_protocol'),@('net_codec','net_codec'),@('net_recovery','net_recovery'),@('net_publish','net_publish'),
 @('control_policy','control_policy'),@('control_adapter','control_policy','app_control'),
 @('ir_pulse_decode','ir_pulse_codec'),@('ir_pulse_encode','ir_pulse_codec'),@('ir_pulse_driver','app_ir_pulse','ir_pulse_codec'),
 @('rx_cursor','rx_cursor'),@('health_policy','health_policy'),@('uart_dma','app_uart_dma','rx_cursor'),@('watchdog','app_watchdog','health_policy'))
foreach($case in $cases) {
 $name=$case[0];$mock=''
 if($name -eq 'control_adapter'){$mock='control_host'}
 if($name -eq 'ir_pulse_driver'){$mock='ir_pulse_host'}
 if($name -eq 'uart_dma'){$mock='uart_dma_host'}
 if($name -eq 'watchdog'){$mock='health_host'}
 $cmd='cl /nologo /utf-8 /TC /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
 if($mock){$cmd+='/I"'+$PSScriptRoot+'\'+$mock+'" '}
 $cmd+='/I"'+$app+'\Inc" "'+$PSScriptRoot+'\test_'+$name+'.c" '
 foreach($src in $case[1..($case.Count-1)]){$cmd+='"'+$app+'\Src\'+$src+'.c" '}
 $lines+=($cmd+'/Fe:'+ $name+'.exe');$lines+='if errorlevel 1 exit /b 1'
 $lines+=($name+'.exe');$lines+='if errorlevel 1 exit /b 1'
}
$batch=Join-Path $out 'run.cmd'
[IO.File]::WriteAllLines($batch,$lines,(New-Object Text.UTF8Encoding($false)))
& cmd /d /c $batch
if($LASTEXITCODE -ne 0){throw 'Optimization host regression failed'}
