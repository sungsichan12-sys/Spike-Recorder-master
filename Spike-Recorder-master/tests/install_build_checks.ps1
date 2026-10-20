$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'install-fixture'
New-Item -ItemType Directory -Force -Path (Join-Path $root 'tools'),(Join-Path $root 'win/build') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../tools/install-build.ps1') -Destination (Join-Path $root 'tools/install-build.ps1') -Force
$installer=Join-Path $root 'tools/install-build.ps1'
$candidate=Join-Path $root 'win/build/SpikeRecorder-next.exe'
$installed=Join-Path $root 'win/build/SpikeRecorder.exe'
function Check([bool]$condition,[string]$message) { if(!$condition) { throw $message }; Write-Host "PASS $message" }
[IO.File]::WriteAllText($installed,'old')
[IO.File]::WriteAllText($candidate,'new')
& powershell -NoProfile -ExecutionPolicy Bypass -File $installer
Check ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($installed) -eq 'new' -and !(Test-Path -LiteralPath $candidate)) 'Install consumes staged build'
[IO.File]::WriteAllText($installed,'newer direct build')
& powershell -NoProfile -ExecutionPolicy Bypass -File $installer
Check ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($installed) -eq 'newer direct build') 'Relaunch cannot roll back a newer original executable'
[IO.File]::WriteAllText($candidate,'newer direct build')
& powershell -NoProfile -ExecutionPolicy Bypass -File $installer
Check ($LASTEXITCODE -eq 0 -and !(Test-Path -LiteralPath $candidate)) 'Identical staged build is consumed'
[IO.File]::WriteAllText($candidate,'pending')
$locked=[IO.File]::Open($installed,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $installer
    Check ($LASTEXITCODE -eq 3 -and (Test-Path -LiteralPath $candidate) -and [IO.File]::ReadAllText($installed) -eq 'newer direct build') 'Locked executable keeps the pending build without changing installed bytes'
} finally { $locked.Dispose() }
& powershell -NoProfile -ExecutionPolicy Bypass -File $installer
Check ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($installed) -eq 'pending') 'Pending update applies after release'
Write-Host 'RESULT 5 passed, 0 failed'
