param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\dist')
)
$ErrorActionPreference = 'Stop'
$taskRepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$taskBinary = Join-Path $BuildDirectory 'Release\!00_AE_Locale_Bridge.aex'
if (-not (Test-Path -LiteralPath $taskBinary)) { throw "Build Release first: $taskBinary" }
$taskPackage = Join-Path $OutputDirectory 'AE-Locale-Bridge-0.2.3-win64'
New-Item -ItemType Directory -Path $taskPackage -Force | Out-Null
Copy-Item -LiteralPath $taskBinary -Destination $taskPackage -Force
foreach ($taskName in @('AE-Locale-Bridge.ini', 'README.md', 'BUILDING.md', 'LICENSE', 'UPSTREAM.md', 'AUTHORS.md', 'CHANGELOG.md')) {
    Copy-Item -LiteralPath (Join-Path $taskRepoRoot $taskName) -Destination $taskPackage -Force
}
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'third_party\minhook\LICENSE.txt') -Destination (Join-Path $taskPackage 'MinHook-LICENSE.txt') -Force
$taskHash = Get-FileHash -LiteralPath (Join-Path $taskPackage '!00_AE_Locale_Bridge.aex') -Algorithm SHA256
"$($taskHash.Hash.ToLowerInvariant())  !00_AE_Locale_Bridge.aex" | Set-Content -LiteralPath (Join-Path $taskPackage 'SHA256SUMS.txt') -Encoding ascii
$taskZip = Join-Path $OutputDirectory 'AE-Locale-Bridge-0.2.3-win64.zip'
Compress-Archive -LiteralPath $taskPackage -DestinationPath $taskZip -Force
Write-Output (Get-Item -LiteralPath $taskZip).FullName
