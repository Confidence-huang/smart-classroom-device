$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '../..')
New-Item -ItemType Directory -Force tests/algorithms/build | Out-Null
$vc='C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$line='call "'+$vc+'" >nul && cl /nologo /std:c11 /W4 /WX /O2 /I algorithms tests/algorithms/test_algorithm.c algorithms/ecovision_algorithm.c /Fe:tests/algorithms/build/test_algorithm.exe /Fo:tests/algorithms/build/ /link /INCREMENTAL:NO'
cmd /c $line
if($LASTEXITCODE -ne 0){throw 'Host compilation failed'}
& ./tests/algorithms/build/test_algorithm.exe
if($LASTEXITCODE -ne 0){throw 'Host tests failed'}
$line='call "'+$vc+'" >nul && cl /nologo /std:c11 /W4 /WX /O2 /I algorithms /I firmware/stm32/EcoVision_F413ZH/App/Inc tests/algorithms/test_core.c algorithms/ecovision_algorithm.c algorithms/ecovision_stm32_inputs.c /Fe:tests/algorithms/build/test_core.exe /Fo:tests/algorithms/build/ /link /INCREMENTAL:NO'
cmd /c $line
if($LASTEXITCODE -ne 0){throw 'Core test compilation failed'}
& ./tests/algorithms/build/test_core.exe
if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
$line='call "'+$vc+'" >nul && cl /nologo /D_CRT_SECURE_NO_WARNINGS /std:c11 /W4 /WX /O2 /I algorithms algorithms/replay.c algorithms/ecovision_algorithm.c /Fe:tests/algorithms/build/replay.exe /Fo:tests/algorithms/build/ /link /INCREMENTAL:NO'
cmd /c $line
if($LASTEXITCODE -ne 0){throw 'Replay compilation failed'}
python tests/algorithms/evaluate.py
if($LASTEXITCODE -ne 0){throw 'Replay evaluation failed'}
