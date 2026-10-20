param([string]$ConnectionFile)
$ErrorActionPreference='Stop'
if(!$ConnectionFile) {$ConnectionFile=Read-Host 'Downloaded private LOTTOL-PC-connection.json path'}
$ConnectionFile=$ConnectionFile.Trim('"')
$data=Get-Content -LiteralPath $ConnectionFile -Raw -Encoding UTF8 | ConvertFrom-Json
if($data.endpoint -notmatch '^https://script\.google\.com/macros/s/[A-Za-z0-9_-]+/exec$' -or
   $data.uploadKey -notmatch '^[A-Za-z0-9_-]{32,256}$') {throw 'Invalid private cloud connection file.'}
& (Join-Path $PSScriptRoot 'manage-sync.ps1') -Mode Stop
$private=Join-Path $PSScriptRoot 'private'
[IO.Directory]::CreateDirectory($private) | Out-Null
$json=@{endpoint=[string]$data.endpoint;uploadKey=[string]$data.uploadKey} | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $private 'connection.json'),$json,[Text.UTF8Encoding]::new($false))
Write-Output 'Private connection saved on this PC. It is not uploaded to DriveX.'
& (Join-Path $PSScriptRoot 'manage-sync.ps1') -Mode Start
