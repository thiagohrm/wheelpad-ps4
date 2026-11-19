# Upload wheelpad.prx to the console and register it in plugins.ini.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File ps4\deploy.ps1 -Title CUSA06238
#   powershell -ExecutionPolicy Bypass -File ps4\deploy.ps1 -Title CUSA00001 -WhatIf
#
# -Title writes or updates a per-title section, so only that game loads the
# plugin. -DefaultSection registers it under [default], which loads it into
# every title including system software: useful for a quick test, bad as a
# resting state.

param(
    [Parameter(Mandatory = $true)]
    [string]$Title,
    [string]$HostAddress = "192.168.1.17",
    [int]$Port = 2121,
    [string]$User = "anonymous",
    [string]$Password = "x",
    [switch]$DefaultSection,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$prx = Join-Path $root "build\ps4\wheelpad.prx"
if (-not (Test-Path $prx)) { throw "missing $prx, run ps4\build.ps1 first" }

$remotePrx = "/data/GoldHEN/plugins/wheelpad.prx"
$remoteIni = "/data/GoldHEN/plugins.ini"
$remoteIniBackup = "/data/GoldHEN/plugins.ini.wheelpad-backup"

function New-FtpRequest($uri, $method) {
    $request = [Net.FtpWebRequest]::Create("ftp://$HostAddress`:$Port$uri")
    $request.Credentials = New-Object Net.NetworkCredential($User, $Password)
    $request.Method = $method
    $request.UsePassive = $true
    $request.UseBinary = $true
    $request.Timeout = 20000
    return $request
}

function Get-FtpFile($uri) {
    $request = New-FtpRequest $uri ([Net.WebRequestMethods+Ftp]::DownloadFile)
    $response = $request.GetResponse()
    $stream = $response.GetResponseStream()
    $buffer = New-Object IO.MemoryStream
    try {
        $stream.CopyTo($buffer)
    }
    finally {
        $stream.Close()
        $response.Close()
    }
    return $buffer.ToArray()
}

function Get-FtpList($uri) {
    $request = New-FtpRequest $uri ([Net.WebRequestMethods+Ftp]::ListDirectory)
    $response = $request.GetResponse()
    $reader = New-Object IO.StreamReader($response.GetResponseStream())
    try {
        return $reader.ReadToEnd()
    }
    finally {
        $reader.Close()
        $response.Close()
    }
}

function Send-FtpFile($uri, $bytes) {
    # The console's FTP server ignores an upload that would overwrite an
    # existing file, so remove the old one first. Verified by reading the
    # file back and comparing its content.
    try {
        $request = New-FtpRequest $uri ([Net.WebRequestMethods+Ftp]::DeleteFile)
        $response = $request.GetResponse()
        $response.Close()
    }
    catch {
        # Missing file is fine.
    }

    $request = New-FtpRequest $uri ([Net.WebRequestMethods+Ftp]::UploadFile)
    $request.ContentLength = $bytes.Length
    $stream = $request.GetRequestStream()
    try {
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally {
        $stream.Close()
    }
    $response = $request.GetResponse()
    $response.Close()
}

# plugins.ini uses a UTF-8 BOM and CRLF or LF depending on the editor. Keep
# whatever is there so GoldHEN keeps parsing it the same way.
$text = [Text.Encoding]::UTF8.GetString((Get-FtpFile $remoteIni))
$hasBom = $text.Length -gt 0 -and $text[0] -eq [char]0xFEFF
if ($hasBom) { $text = $text.Substring(1) }
$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }

$section = if ($DefaultSection) { "default" } else { $Title }
$pluginLine = $remotePrx

Write-Host "plugin:  $prx ($((Get-Item $prx).Length) bytes)"
Write-Host "remote:  $remotePrx"
Write-Host "section: [$section]"

# Split into blocks so an existing section can be found and extended.
$lines = $text -split "`r?`n"
$existingSection = $lines | Where-Object { $_.Trim() -eq "[$section]" }
$alreadyRegistered = $false
if ($existingSection) {
    $start = [Array]::IndexOf($lines, $existingSection)
    for ($i = $start + 1; $i -lt $lines.Count; $i++) {
        $line = $lines[$i].Trim()
        if ($line.StartsWith("[")) { break }
        if ($line -eq $pluginLine) { $alreadyRegistered = $true; break }
    }
}

if ($alreadyRegistered) {
    Write-Host "plugins.ini already lists the plugin in [$section]"
}
else {
    $addition = @()
    if (-not $existingSection) { $addition += "", "[$section]" }
    $addition += $pluginLine

    $updated = ($lines + $addition) -join $newline
    if (-not $updated.EndsWith($newline)) { $updated += $newline }

    if ($WhatIf) {
        Write-Host "would append:"
        $addition | ForEach-Object { Write-Host "  $_" }
    }
    else {
        Send-FtpFile $remoteIniBackup ([Text.Encoding]::UTF8.GetBytes($text))
        $encoding = New-Object Text.UTF8Encoding($hasBom)
        Send-FtpFile $remoteIni $encoding.GetBytes($updated)
        Write-Host "plugins.ini updated (backup at $remoteIniBackup)"
    }
}

if ($WhatIf) {
    Write-Host "would upload $remotePrx"
}
else {
    Send-FtpFile $remotePrx ([IO.File]::ReadAllBytes($prx))
    $listing = Get-FtpList "/data/GoldHEN/plugins"
    if ($listing -notmatch "wheelpad\.prx") { throw "upload finished but wheelpad.prx is not listed" }
    Write-Host "uploaded. Restart the title (or the console) so GoldHEN loads the plugin."
}
