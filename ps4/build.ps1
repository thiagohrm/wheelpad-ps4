# Build wheelpad.prx with the OpenOrbis toolchain, without WSL or make.
#
# Needs:
#   - OpenOrbis PS4 Toolchain with lib/ and bin/windows/ extracted
#   - GoldHEN Plugin SDK sources
#   - zig 0.17+ on PATH or in the WinGet packages directory (used as clang)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File ps4/build.ps1
#
# Set OO_PS4_TOOLCHAIN and GOLDHEN_SDK to override the default locations.

param(
    [switch]$Minimal,
    [string]$Source,
    [string]$OutputName = "wheelpad"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "build\ps4"
New-Item -ItemType Directory -Force -Path $out | Out-Null

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

$toolchain = $env:OO_PS4_TOOLCHAIN
if (-not $toolchain) {
    $toolchain = "C:\Users\workstation\tools\orbis\OpenOrbis\PS4Toolchain"
}
$sdk = $env:GOLDHEN_SDK
if (-not $sdk) {
    $sdk = "C:\Users\workstation\tools\GoldHEN_Plugins_SDK"
}

foreach ($path in @($toolchain, $sdk)) {
    if (-not (Test-Path $path)) { throw "missing: $path" }
}
$fself = Join-Path $toolchain "bin\windows\create-fself.exe"
if (-not (Test-Path $fself)) { throw "missing: $fself" }

$zig = Resolve-Zig
$cc = @(
    "--target=x86_64-freestanding",
    "-fPIC",
    "-funwind-tables",
    "-std=c11",
    "-D__WCHAR_TYPE__=unsigned short",
    "-O2",
    "-Wall",
    "-Wextra",
    "-D__FINAL__=1",
    "-D__USE_PRINTF__",
    "-isysroot", $toolchain,
    "-isystem", (Join-Path $toolchain "include"),
    "-isystem", (Join-Path $toolchain "include\orbis\_types"),
    "-I", (Join-Path $sdk "include"),
    "-I", (Join-Path $root "src"),
    "-I", (Join-Path $root "ps4\source")
)

$pluginSource = if ($Source) { $Source } elseif ($Minimal) { "wheelpad_min.c" } else { "wheelpad.c" }

$sources = @(
    # crtprx.o defines _init and must come first: the PS4 loader jumps to the
    # entry point right after the /libexec/ld-elf.so.1 header that link.x
    # emits, so _init has to be the first function in .text.
    (Join-Path $sdk "crt\crtprx.c"),
    (Join-Path $sdk "source\Detour.c"),
    (Join-Path $sdk "source\GoldHEN.c"),
    (Join-Path $sdk "source\HDE64.c"),
    (Join-Path $sdk "source\Patcher.c"),
    (Join-Path $sdk "source\Syscall.c"),
    (Join-Path $sdk "source\Utilities.c"),
    (Join-Path $root "src\report.c"),
    (Join-Path $root "src\report_console.c"),
    (Join-Path $root "src\mapping.c"),
    (Join-Path $root "ps4\source\wheel_usb.c"),
    (Join-Path $root "ps4\source\$pluginSource")
)

function Read-U16($bytes, [int]$offset) {
    return [BitConverter]::ToUInt16($bytes, $offset)
}

function Read-U32($bytes, [int]$offset) {
    return [BitConverter]::ToUInt32($bytes, $offset)
}

function Read-U64($bytes, [int]$offset) {
    return [BitConverter]::ToUInt64($bytes, $offset)
}

# The PS4 loader starts a PRX at the entry point that sits right after the
# 32 byte /libexec/ld-elf.so.1 header that link.x puts at the front of .text.
# If the linker orders another function first, the loader jumps into the
# middle of unrelated code and the plugin never runs its init. Check it here
# instead of on the console.
function Assert-EntryPoint($oelfPath) {
    $bytes = [IO.File]::ReadAllBytes($oelfPath)

    $shoff = [int64](Read-U64 $bytes 0x28)
    $shentsize = Read-U16 $bytes 0x3a
    $shnum = Read-U16 $bytes 0x3c
    $shstrndx = Read-U16 $bytes 0x3e

    $sections = @()
    for ($i = 0; $i -lt $shnum; $i++) {
        $base = [int]$shoff + $i * $shentsize
        $sections += [pscustomobject]@{
            NameOffset = [int64](Read-U32 $bytes $base)
            Type       = [int64](Read-U32 $bytes ($base + 4))
            Address    = [int64](Read-U64 $bytes ($base + 16))
            Offset     = [int64](Read-U64 $bytes ($base + 24))
            Size       = [int64](Read-U64 $bytes ($base + 32))
            Link       = [int64](Read-U32 $bytes ($base + 40))
            EntrySize  = [int64](Read-U64 $bytes ($base + 56))
        }
    }

    $symtab = $sections | Where-Object { $_.Type -eq 2 } | Select-Object -First 1
    if (-not $symtab) { throw "no symbol table in $oelfPath" }
    $symstr = $sections[$symtab.Link]

    $initAddress = $null
    $firstFunction = $null

    for ($i = 0; $i -lt [int]($symtab.Size / $symtab.EntrySize); $i++) {
        $base = [int]$symtab.Offset + $i * [int]$symtab.EntrySize
        $nameOffset = [int64](Read-U32 $bytes $base)
        $shndx = Read-U16 $bytes ($base + 6)
        $value = [int64](Read-U64 $bytes ($base + 8))

        $start = [int]$symstr.Offset + [int]$nameOffset
        $end = $start
        while ($bytes[$end] -ne 0) { $end++ }
        $name = [Text.Encoding]::ASCII.GetString($bytes, $start, $end - $start)

        if ($shndx -eq 1 -and $value -ne 0) {
            if ($firstFunction -eq $null -or $value -lt $firstFunction) {
                $firstFunction = $value
            }
            if ($name -eq "_init") { $initAddress = $value }
        }
    }

    if ($null -eq $initAddress) { throw "_init not found in $oelfPath" }
    if ($null -ne $firstFunction -and $initAddress -ne $firstFunction) {
        $message = "_init is at 0x{0:x} but the first function in .text is at 0x{1:x}. " -f $initAddress, $firstFunction
        $message += "Link crtprx.o first so _init lands at the start of .text."
        throw $message
    }
    Write-Host ("entry point ok: _init at 0x{0:x}" -f $initAddress)
}

$objects = @()
Push-Location $root
try {
    foreach ($source in $sources) {
        $name = [IO.Path]::GetFileNameWithoutExtension($source)
        $object = Join-Path $out "$name.o"
        & $zig cc @cc -c -o $object $source
        if ($LASTEXITCODE -ne 0) { throw "compile failed: $source" }
        $objects += $object
    }

    $elf = Join-Path $out "$OutputName.elf"
    $prx = Join-Path $out "$OutputName.prx"
    $oelf = Join-Path $out "$OutputName.oelf"

    $link = @(
        "-nostdlib",
        "-Wl,--no-dynamic-linker",
        "-Wl,--script=$(Join-Path $toolchain 'link.x')",
        "-Wl,--entry=_init",
        "-Wl,--eh-frame-hdr",
        "-L", (Join-Path $toolchain "lib")
    )
    & $zig cc --target=x86_64-linux-gnu -pie @link @objects -o $elf `
        -lSceLibcInternal -lSceUsbd -lkernel -lScePad
    if ($LASTEXITCODE -ne 0) { throw "link failed" }

    $env:OO_PS4_TOOLCHAIN = $toolchain
    & $fself "-in=$elf" "-out=$oelf" "--lib=$prx" "--paid" "0x3800000000000011"
    if ($LASTEXITCODE -ne 0) { throw "create-fself failed" }

    Assert-EntryPoint $oelf

    $size = (Get-Item $prx).Length
    Write-Host "built $prx ($size bytes)"
}
finally {
    Pop-Location
}
