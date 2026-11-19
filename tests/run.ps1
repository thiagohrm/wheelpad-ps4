# Build and run the host tests with zig cc.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $build | Out-Null

function Resolve-Zig {
    $cmd = Get-Command zig -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $winget = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path $winget) {
        $found = Get-ChildItem $winget -Recurse -Filter zig.exe -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    throw "zig not found. Install it with: winget install --id zig.zig"
}

$zig = Resolve-Zig
$flags = @("-std=c99", "-Wall", "-Wextra")

function Invoke-Test($name, $sources) {
    $exe = Join-Path $build "$name.exe"
    Push-Location $root
    try {
        & $zig cc @flags -o $exe @sources
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }
}

Invoke-Test "test_report" @("src/report.c", "tests/test_report.c")
Invoke-Test "test_mapping" @("src/report.c", "src/mapping.c", "tests/test_mapping.c")
Invoke-Test "test_report_console" @("src/report.c", "src/report_console.c", "src/mapping.c", "tests/test_report_console.c")
