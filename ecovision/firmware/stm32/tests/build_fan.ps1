param([string]$EvidenceName = '2026-11-21')
$ErrorActionPreference = 'Stop'
$stm32 = Split-Path $PSScriptRoot
$project = Join-Path $stm32 'EcoVision_F413ZH/MDK-ARM/EcoVision_F413ZH.uvprojx'
$evidence = Join-Path $stm32 "tb6612-evidence/$EvidenceName"
$null = New-Item -ItemType Directory -Force -Path $evidence
$raw = Join-Path $env:TEMP ('ecovision-fan-' + [guid]::NewGuid() + '.log')
try {
    $result = Start-Process -FilePath 'D:/k51/UV4/UV4.exe' -WindowStyle Hidden -Wait -PassThru -WorkingDirectory (Split-Path $project) -ArgumentList "-r `"$project`" -t EcoVision_F413ZH -j0 -o `"$raw`""
    $lines = Get-Content -LiteralPath $raw
    $lines | Where-Object { $_ -notmatch '(?i)License|licence|SN:|LIC=' } | Set-Content -Encoding utf8 (Join-Path $evidence 'keil.log')
    $summary = $lines | Select-String 'Program Size|Error\(s\)|0x26F'
    $summary | ForEach-Object { Write-Output $_.Line }
    if ($result.ExitCode -gt 1 -or !($lines -match ' - 0 Error\(s\)')) { throw 'Keil fan build failed; see keil.log' }
    $hex = Join-Path (Split-Path $project) 'EcoVision_F413ZH/EcoVision_F413ZH.hex'
    $copy = Join-Path $evidence 'EcoVision_F413ZH-fan-rebuild.hex'
    Copy-Item -LiteralPath $hex -Destination $copy
    Get-FileHash -LiteralPath $copy -Algorithm SHA256 | Format-List | Out-File -Encoding utf8 (Join-Path $evidence 'sha256.txt')
} finally {
    if (Test-Path -LiteralPath $raw) { Remove-Item -LiteralPath $raw }
}
