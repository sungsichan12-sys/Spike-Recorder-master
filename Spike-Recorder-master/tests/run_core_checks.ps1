$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $project
$env:PATH='C:\msys64\mingw32\bin;' + $env:PATH
$objects=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $project 'Makefile')),'(?m)^\s*(\./(?:src|support)/[^\s\\]+\.(?:cpp|c))') | ForEach-Object {
    [IO.Path]::ChangeExtension($_.Groups[1].Value,'.o')
} | Where-Object { $_ -ne './src/main.o' -and $_ -ne './src/native/PathsWin.o' })
if(!$objects.Count) { throw 'No application engine objects found.' }
$flags=@('-std=gnu++11','-O2','-Isrc','-Isrc/engine','-Isrc/engine/firmware','-Isrc/libraries','-Isrc/widgets','-Isupport','-Isupport/SDL2/include/SDL2','-Isupport/curl/include','tests/core_regression.cpp')
$libs=@('-Lsupport/SDL2/lib','-Lsupport/SDL2Image/lib','-LC:/msys64/mingw32/lib','-Lsupport','-lSDL2','-lSDL2_image','-luser32','-lgdi32','-lwinmm','-lsetupapi','-ldxguid','-lopengl32','-lglu32','-lcomdlg32','-lhid','-lhidapi','-lBSL430','-lbass','-lcurl','-static-libstdc++','-static-libgcc')
& 'C:\msys64\mingw32\bin\g++.exe' @flags @objects @libs -o win/build/core-regression.exe
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$output=Join-Path $project 'tests/core-artifacts'
[IO.Directory]::CreateDirectory($output) | Out-Null
Set-Location -LiteralPath (Join-Path $project 'win/build')
& .\core-regression.exe $output | Tee-Object -FilePath (Join-Path $output 'result.txt')
exit $LASTEXITCODE
