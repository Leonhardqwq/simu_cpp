Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$release = Split-Path -Parent $MyInvocation.MyCommand.Path
$recordReplay = Split-Path -Parent $release
$jackDir = Split-Path -Parent $recordReplay
$root = Split-Path -Parent $jackDir
Set-Location $root

$venv = Join-Path $release ".venv"
$python = Join-Path $venv "Scripts\python.exe"

function Check-Exit($name) {
    if ($LASTEXITCODE -ne 0) {
        throw "$name failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $python)) {
    python -m venv $venv
    Check-Exit "python venv"
}

& $python -m pip install -r (Join-Path $release "requirements.txt")
Check-Exit "pip install"

$cppSource = Join-Path $recordReplay "Jack_record_replay.cpp"
$cppExe = Join-Path $release "Jack_record_replay.exe"
& g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ $cppSource -o $cppExe
Check-Exit "g++ Jack_record_replay"

& $python -m PyInstaller `
    --onefile `
    --console `
    --clean `
    --noconfirm `
    --paths $root `
    --name JackRecordReplay `
    --distpath $release `
    --workpath (Join-Path $release "build") `
    --specpath (Join-Path $release "spec") `
    --add-binary "$cppExe;." `
    --add-data "$(Join-Path $jackDir 'jack_config.json');Jack" `
    (Join-Path $recordReplay "jack_replay.py")
Check-Exit "PyInstaller JackRecordReplay"

Write-Host "Generated:"
Write-Host "  $(Join-Path $release 'JackRecordReplay.exe')"
