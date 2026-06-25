Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$venv = Join-Path $root ".venv-release"
$python = Join-Path $venv "Scripts\python.exe"

if (-not (Test-Path -LiteralPath $python)) {
    python -m venv $venv
}

function Check-Exit($name) {
    if ($LASTEXITCODE -ne 0) {
        throw "$name failed with exit code $LASTEXITCODE"
    }
}

& $python -m pip install -r (Join-Path $root "requirements-release.txt")
Check-Exit "pip install"

function Build-Cpp($source, $output) {
    & g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ `
        (Join-Path $root $source) `
        -o (Join-Path $root $output)
    Check-Exit "g++ $source"
}

function Package-Simulator($dir, $name, $script, $cpp) {
    $distPath = Join-Path $root $dir
    $workPath = Join-Path $root ".pyinstaller\build\$dir"
    $specPath = Join-Path $root ".pyinstaller\spec"
    $scriptPath = Join-Path $root "$dir\$script"
    $cppPath = Join-Path $root "$dir\$cpp"

    & $python -m PyInstaller `
        --onefile `
        --console `
        --clean `
        --noconfirm `
        --paths $root `
        --name $name `
        --distpath $distPath `
        --workpath $workPath `
        --specpath $specPath `
        --hidden-import openpyxl `
        --add-binary "$cppPath;." `
        $scriptPath
    Check-Exit "PyInstaller $name"
}

Build-Cpp "Jack\Jack_cpp_1_0.cpp" "Jack\Jack_cpp_1_0.exe"
Build-Cpp "Enter\Enter_cpp_1_0.cpp" "Enter\Enter_cpp_1_0.exe"
Build-Cpp "Zomboni\Zomboni_cpp_1_0.cpp" "Zomboni\Zomboni_cpp_1_0.exe"

Package-Simulator "Jack" "JackSimulator" "run_jack.py" "Jack_cpp_1_0.exe"
Package-Simulator "Enter" "EnterSimulator" "run_enter.py" "Enter_cpp_1_0.exe"
Package-Simulator "Zomboni" "ZomboniSimulator" "run_zomboni.py" "Zomboni_cpp_1_0.exe"

Write-Host "Release exe generated:"
Write-Host "  Jack\JackSimulator.exe"
Write-Host "  Enter\EnterSimulator.exe"
Write-Host "  Zomboni\ZomboniSimulator.exe"
