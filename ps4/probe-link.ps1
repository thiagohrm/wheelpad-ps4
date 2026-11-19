# Throwaway: link the same objects with -pie vs -shared and report the PLT.
$ErrorActionPreference = "Stop"
$root = "C:\Users\workstation\Documents\volante-elf"
$t = "C:\Users\workstation\tools\orbis\OpenOrbis\PS4Toolchain"
$zig = (Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter zig.exe |
        Select-Object -First 1).FullName

$objs = Get-ChildItem "$root\build\ps4\*.o" |
    Where-Object { $_.Name -ne "t.o" } |
    ForEach-Object { $_.FullName }

Push-Location $root
try {
    foreach ($mode in @("-pie", "-shared")) {
        $name = $mode.TrimStart("-")
        $out = "build\ps4\probe_$name.elf"
        $args = @(
            "cc", "--target=x86_64-linux-gnu", $mode, "-nostdlib",
            "-Wl,--no-dynamic-linker",
            "-Wl,--script=$t\link.x",
            "-Wl,--entry=_init",
            "-Wl,--eh-frame-hdr",
            "-Wl,-z,now",
            "-L", "$t\lib"
        ) + $objs + @("-o", $out, "-lSceLibcInternal", "-lkernel", "-lScePad")
        & $zig @args
        Write-Host "mode=$mode exit=$LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
