$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '../../..')
New-Item -ItemType Directory -Force firmware/stm32/lcd-evidence/build | Out-Null
$vc='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$line='call "'+$vc+'" >nul && cl /nologo /D_CRT_SECURE_NO_WARNINGS /std:c11 /W4 /WX /O2 /I firmware/stm32/EcoVision_F413ZH/App/Inc firmware/stm32/tests/test_lcd_view.c firmware/stm32/EcoVision_F413ZH/App/Src/lcd_view.c /Fe:firmware/stm32/lcd-evidence/build/test_lcd.exe /Fo:firmware/stm32/lcd-evidence/build/ /link /INCREMENTAL:NO'
cmd /c $line
if($LASTEXITCODE -ne 0){throw 'LCD host compilation failed'}
& ./firmware/stm32/lcd-evidence/build/test_lcd.exe
if($LASTEXITCODE -ne 0){throw 'LCD host tests failed'}
