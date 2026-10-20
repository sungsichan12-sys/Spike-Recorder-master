param([ValidateSet('Check','Record')][string]$Mode = 'Check')
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $project 'win/build/SpikeRecorder.exe'
$stateFile = Join-Path $project 'win/build/build-state.txt'
try {
    if (-not (Test-Path -LiteralPath $executable)) { exit 1 }
    $inputs = @()
    foreach ($folder in @('src','data','game','support')) {
        $base = Join-Path $project $folder
        if (-not (Test-Path -LiteralPath $base)) { continue }
        $inputs += Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object {
            if ($_.Extension -in @('.o','.d','.log','.pyc')) { return $false }
            if ($folder -eq 'src') { return $_.Extension -in @('.c','.cpp','.h','.hpp','.cc','.inc') }
            # A source-built game DLL is an output, not a new build input.
            if ($folder -eq 'game' -and $_.Extension -eq '.dll' -and
                (Test-Path -LiteralPath ([IO.Path]::ChangeExtension($_.FullName,'.cpp')))) { return $false }
            return $true
        }
    }
    foreach ($name in @('Makefile','build_windows.bat','tools/build-state.ps1','tools/install-build.ps1')) {
        $inputs += Get-Item -LiteralPath (Join-Path $project $name)
    }
    $snapshot = @($inputs | Sort-Object FullName | ForEach-Object {
        '{0}|{1}|{2}' -f $_.FullName.Substring($project.Length + 1), $_.Length, $_.LastWriteTimeUtc.Ticks
    }) -join "`n"
    if ($Mode -eq 'Record') {
        [IO.File]::WriteAllText($stateFile,$snapshot,[Text.UTF8Encoding]::new($false))
        exit 0
    }
    if (-not (Test-Path -LiteralPath $stateFile)) { exit 1 }
    if ([IO.File]::ReadAllText($stateFile) -ceq $snapshot) { exit 0 }
    exit 1
} catch {
    Write-Error $_
    exit 2
}
