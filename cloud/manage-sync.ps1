param([ValidateSet('Start','Stop')][string]$Mode='Start')
$ErrorActionPreference='Stop'
$scriptPath=Join-Path $PSScriptRoot 'sync_scores.py'
$connectionPath=Join-Path $PSScriptRoot 'private/connection.json'
$pidFile=Join-Path $PSScriptRoot 'sender.pid'
$worker=$null
if(Test-Path -LiteralPath $pidFile) {
    $workerId=0
    if([int]::TryParse([IO.File]::ReadAllText($pidFile).Trim(),[ref]$workerId)) {
        $worker=Get-CimInstance Win32_Process -Filter "ProcessId = $workerId"
        if($worker -and (!$worker.CommandLine -or $worker.CommandLine -notmatch [regex]::Escape($scriptPath))) {
            throw 'Saved PID belongs to a different process. Nothing was stopped.'
        }
    }
}
if($Mode -eq 'Stop') {
    if($worker) {Stop-Process -Id $worker.ProcessId -ErrorAction Stop}
    if(Test-Path -LiteralPath $pidFile) {Remove-Item -LiteralPath $pidFile}
    exit 0
}
if($worker) {Write-Output 'Cloud score sender is already running.';exit 0}
if(!(Test-Path -LiteralPath $connectionPath)) {
    Write-Output 'Cloud API deployment is not connected yet. Game results remain safely queued.'
    Write-Output 'After Google deployment, download the private connection file and use configure_cloud.bat.'
    exit 0
}
$pythonExe=Join-Path $env:LOCALAPPDATA 'Programs/Python/Python313/python.exe'
if(!(Test-Path -LiteralPath $pythonExe)) {
    $python=Get-Command python.exe -ErrorAction Stop
    $pythonExe=$python.Source
}
& $pythonExe -c 'import json,urllib.request,sys;sys.exit(0)' 2>$null
if($LASTEXITCODE -ne 0) {throw 'Python 3 is required for the cloud sender.'}
$logs=Join-Path $PSScriptRoot 'logs'
[IO.Directory]::CreateDirectory($logs) | Out-Null
$child=Start-Process -FilePath $pythonExe -ArgumentList @('-u',('"'+$scriptPath+'"')) -WorkingDirectory (Split-Path $PSScriptRoot -Parent) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'sender.log') -RedirectStandardError (Join-Path $logs 'sender-error.log') -PassThru
Start-Sleep -Milliseconds 400
$child.Refresh()
if($child.HasExited) {throw 'Cloud sender did not start. Check cloud/logs/sender-error.log.'}
[IO.File]::WriteAllText($pidFile,[string]$child.Id,[Text.UTF8Encoding]::new($false))
Write-Output 'Cloud score sender started. No local web server is used.'
