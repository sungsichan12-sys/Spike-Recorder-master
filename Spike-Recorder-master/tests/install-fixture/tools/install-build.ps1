$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$candidate = Join-Path $project 'win/build/SpikeRecorder-next.exe'
$installed = Join-Path $project 'win/build/SpikeRecorder.exe'
if (-not (Test-Path -LiteralPath $candidate)) { exit 0 }
function Get-BuildDigest([string]$path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToBase64String($sha.ComputeHash([IO.File]::ReadAllBytes($path))) }
    finally { $sha.Dispose() }
}
try {
    if ((Test-Path -LiteralPath $installed) -and
        (Get-BuildDigest $candidate) -eq (Get-BuildDigest $installed)) {
        Remove-Item -LiteralPath $candidate
        exit 0
    }
    Copy-Item -LiteralPath $candidate -Destination $installed -Force
    # Consume the staging file so a later direct build cannot be rolled back.
    Remove-Item -LiteralPath $candidate
    exit 0
} catch [System.IO.IOException] {
    Write-Host 'Update is ready. Close Spike Recorder, then run the launcher again.'
    exit 3
} catch [System.UnauthorizedAccessException] {
    Write-Host 'Update is ready. Close Spike Recorder, then run the launcher again.'
    exit 3
} catch {
    Write-Host "Could not apply the application update: $_"
    exit 1
}
