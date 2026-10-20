param([ValidateSet('Start','Stop')][string]$Mode='Start')
$ErrorActionPreference='Stop'
$serverRoot=$PSScriptRoot
$projectRoot=Split-Path $serverRoot -Parent
$serverScript=Join-Path $serverRoot 'ranking_server.py'
$pidFile=Join-Path $serverRoot 'server.pid'
$serverUrl='http://127.0.0.1:8765/'
function Read-Health {
    try {
        $result=Invoke-RestMethod -Uri ($serverUrl+'api/health') -TimeoutSec 2
        return $result.app -eq 'lottol-ranking' -and $result.status -eq 'ok'
    } catch { return $false }
}
if($Mode -eq 'Stop') {
    if(!(Test-Path -LiteralPath $pidFile)) { Write-Output 'No managed ranking server PID exists.'; exit 0 }
    $serverPid=0
    if(![int]::TryParse([IO.File]::ReadAllText($pidFile).Trim(),[ref]$serverPid)) { throw 'Invalid ranking server PID file.' }
    $processInfo=Get-CimInstance Win32_Process -Filter "ProcessId = $serverPid"
    if($processInfo) {
        if(!$processInfo.CommandLine -or $processInfo.CommandLine -notmatch [regex]::Escape($serverScript)) {
            throw 'PID belongs to a different process. Nothing was stopped.'
        }
        Stop-Process -Id $serverPid -ErrorAction Stop
    }
    Remove-Item -LiteralPath $pidFile
    Write-Output 'LOTTOL ranking server stopped. Saved scores are preserved.'
    exit 0
}
if(Read-Health) { Write-Output ('LOTTOL ranking server is ready: '+$serverUrl); exit 0 }
$probe=New-Object Net.Sockets.TcpClient
try {
    $attempt=$probe.ConnectAsync('127.0.0.1',8765)
    if($attempt.Wait(300) -and $probe.Connected) { throw 'Port 8765 is used by a different service.' }
} catch {
    if($_.Exception.Message -like '*different service*') { throw }
} finally { $probe.Dispose() }
$pythonCommand=Get-Command python.exe -ErrorAction SilentlyContinue
$candidates=@((Join-Path $env:LOCALAPPDATA 'Programs/Python/Python313/python.exe'))
if($pythonCommand) { $candidates += $pythonCommand.Source }
$pythonExe=$null
foreach($candidate in ($candidates | Select-Object -Unique)) {
    if(Test-Path -LiteralPath $candidate) {
        try { & $candidate -c 'import sqlite3, http.server' 2>$null; if($LASTEXITCODE -eq 0) { $pythonExe=$candidate; break } } catch {}
    }
}
if(!$pythonExe) { throw 'Python 3 is required to run the ranking server.' }
$logDir=Join-Path $serverRoot 'logs'
[IO.Directory]::CreateDirectory($logDir) | Out-Null
$child=Start-Process -FilePath $pythonExe -ArgumentList @('-u',('"'+$serverScript+'"')) -WorkingDirectory $projectRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logDir 'server.log') -RedirectStandardError (Join-Path $logDir 'server-error.log') -PassThru
for($step=0;$step -lt 20;$step++) {
    Start-Sleep -Milliseconds 250
    if(Read-Health) {
        [IO.File]::WriteAllText($pidFile,[string]$child.Id,[Text.UTF8Encoding]::new($false))
        Write-Output ('LOTTOL ranking server is ready: '+$serverUrl)
        exit 0
    }
    $child.Refresh()
    if($child.HasExited) { break }
}
$child.Refresh()
if(!$child.HasExited) { Stop-Process -Id $child.Id }
throw ('Ranking server did not start. See '+(Join-Path $logDir 'server-error.log'))
