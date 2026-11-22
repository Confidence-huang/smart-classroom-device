$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '../../..')
$out='firmware/stm32/net-evidence/2026-11-21/build'
New-Item -ItemType Directory -Force $out | Out-Null
$vc='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$line='call "'+$vc+'" >nul && cl /nologo /D_CRT_SECURE_NO_WARNINGS /std:c11 /W4 /WX /O2 /I firmware/stm32/EcoVision_F413ZH/App/Inc firmware/stm32/tests/test_net_codec.c firmware/stm32/EcoVision_F413ZH/App/Src/net_codec.c /Fe:'+ $out+'/test_net.exe /Fo:'+ $out+'/ /link /INCREMENTAL:NO'
cmd /c $line
if($LASTEXITCODE -ne 0){throw 'Host compilation failed'}
& "$out/test_net.exe"
if($LASTEXITCODE -ne 0){throw 'Network codec tests failed'}
