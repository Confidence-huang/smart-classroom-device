$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '../..')
New-Item -ItemType Directory -Force tests/algorithms/build | Out-Null
$cc='D:\k51\ARM\ARMCLANG\bin\armclang.exe'
& $cc --version
if($LASTEXITCODE -ne 0){throw 'AC6 unavailable'}
& $cc --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -std=c99 -Oz -Wall -Wextra -Werror -I algorithms -c algorithms/ecovision_algorithm.c -o tests/algorithms/build/ecovision_algorithm.arm.o
if($LASTEXITCODE -ne 0){throw 'ARM object compilation failed'}
& $cc --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -std=c99 -Oz -Wall -Wextra -Werror -I algorithms -I firmware/stm32/EcoVision_F413ZH/App/Inc -c algorithms/ecovision_stm32_inputs.c -o tests/algorithms/build/ecovision_stm32_inputs.arm.o
if($LASTEXITCODE -ne 0){throw 'ARM STM32 adapter object compilation failed'}
python tests/algorithms/arm_resources.py
if($LASTEXITCODE -ne 0){throw 'ARM object inspection failed'}
