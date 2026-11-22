$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '../../..')
$project=(Resolve-Path 'firmware/stm32/EcoVision_F413ZH/MDK-ARM/EcoVision_F413ZH.uvprojx').Path
$log=Join-Path $env:TEMP 'EcoVision-env-keil.log'
$result=Start-Process -FilePath 'D:/k51/UV4/UV4.exe' -WindowStyle Hidden -Wait -PassThru -WorkingDirectory (Split-Path $project) -ArgumentList "-r `"$project`" -t EcoVision_F413ZH -j0 -o `"$log`""
$result.ExitCode
Get-Content -LiteralPath $log | Where-Object {$_ -notmatch 'License|licence|SN:|LIC='} | Set-Content -Encoding utf8 'firmware/stm32/environment-evidence/keil-build.log'
Select-String -LiteralPath $log -Pattern 'error:|Error:|Program Size|Error\(s\)|0x26F'
if(!(Select-String -LiteralPath $log -Pattern '0 Error\(s\)')) {throw 'Keil build failed'}
