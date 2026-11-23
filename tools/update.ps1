param([int]$GamePid, [string]$RunningVersion,
      [ValidateSet('Check','Install')][string]$Phase='Check',
      [string]$RequestId, [switch]$Automatic,
      [string]$TargetVersion, [string]$ExpectedArchiveHash)

$ErrorActionPreference='Stop'
$repo='bervictus/LfsPad'
$versionPattern='(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})'
$maxArchiveSize=24MB

function Get-ReleaseAsset($Release, [version]$Current) {
    if ($Release.draft -ne $false -or $Release.prerelease -isnot [bool] -or
        $Release.tag_name -cnotmatch "^v($versionPattern)$") { return }
    $version=$Matches[1]
    if ([version]$version -le $Current) { return }
    $name="LfsPad-$version-rfs-win32.zip"
    $url="https://github.com/$repo/releases/download/$($Release.tag_name)/$name"
    $assets=@($Release.assets | Where-Object { $_.name -ceq $name })
    if ($assets.Count -ne 1) { throw 'The release has no unique Windows package.' }
    $asset=$assets[0]
    if ($asset.state -ne 'uploaded' -or $asset.browser_download_url -cne $url -or
        ($asset.size -isnot [int] -and $asset.size -isnot [long]) -or
        $asset.size -le 0 -or $asset.size -gt $maxArchiveSize -or
        $asset.digest -cnotmatch '^sha256:([0-9a-fA-F]{64})$') {
        throw 'GitHub did not provide a valid Windows package checksum and size.'
    }
    @{Uri=[uri]$url; Hash=$Matches[1].ToLowerInvariant(); Size=[long]$asset.size; Version=$version}
}

function Receive-ReleaseFile([uri]$Uri, [IO.Stream]$Output, [long]$Limit) {
    $allowedHosts=@('api.github.com','github.com','release-assets.githubusercontent.com','objects.githubusercontent.com')
    $deadline=[DateTime]::UtcNow.AddSeconds(60)
    for ($redirect=0; $redirect -lt 5; $redirect++) {
        if ($Uri.Scheme -ne 'https' -or !$Uri.IsDefaultPort -or $Uri.UserInfo -or $Uri.Host -notin $allowedHosts) {
            throw 'The update request redirected outside GitHub.'
        }
        $request=[Net.HttpWebRequest]::Create($Uri)
        $request.AllowAutoRedirect=$false
        $request.Timeout=15000
        $request.ReadWriteTimeout=15000
        $request.UserAgent="LfsPad/$RunningVersion"
        $request.Accept='application/vnd.github+json, application/octet-stream'
        $response=$request.GetResponse()
        try {
            if ([int]$response.StatusCode -in 301,302,303,307,308) {
                $location=$response.Headers['Location']
                if (!$location) { throw 'GitHub returned an empty redirect.' }
                $Uri=[uri]::new($Uri,$location)
                continue
            }
            if ($response.StatusCode -ne [Net.HttpStatusCode]::OK -or $response.ContentLength -gt $Limit) {
                throw 'GitHub returned an unexpected response.'
            }
            $inputStream=$response.GetResponseStream()
            try {
                $buffer=New-Object byte[] 32768
                $total=0L
                while (($count=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0) {
                    if ([DateTime]::UtcNow -gt $deadline) { throw 'The update download timed out.' }
                    $total+=$count
                    if ($total -gt $Limit) { throw 'The update response exceeded its size limit.' }
                    $Output.Write($buffer,0,$count)
                }
                return $total
            } finally { $inputStream.Dispose() }
        } finally { $response.Dispose() }
    }
    throw 'Too many redirects checking for an update.'
}

function Get-ReleaseMetadata([string]$Suffix) {
    $metadata=[IO.MemoryStream]::new()
    try {
        Receive-ReleaseFile "https://api.github.com/repos/$repo/releases$Suffix" $metadata 1MB | Out-Null
        $payload=[Text.Encoding]::UTF8.GetString($metadata.ToArray()) | ConvertFrom-Json
        return $payload
    } finally { $metadata.Dispose() }
}

function Copy-UpdateEntries([string]$ArchivePath, [string]$Destination, [string]$Version) {
    Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $prefix="LfsPad-$Version-rfs/"
        $entries=[ordered]@{'dxgi.dll'=16MB; 'tools/install-common.ps1'=128KB;
            'tools/install.ps1'=128KB; 'tools/update.ps1'=128KB}
        if ($archive.Entries.Count -gt 256) { throw 'Too many archive entries.' }
        foreach ($relative in $entries.Keys) {
            $matchesEntry=@($archive.Entries | Where-Object { $_.FullName.Replace('\','/') -ceq ($prefix+$relative) })
            if ($matchesEntry.Count -ne 1 -or !$matchesEntry[0].Length -or
                    $matchesEntry[0].Length -gt $entries[$relative]) {
                throw "Invalid update archive entry: $relative"
            }
            $entry=$matchesEntry[0]
            $target=Join-Path $Destination ([IO.Path]::GetFileName($relative))
            $inputStream=$entry.Open()
            try {
                $outputStream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                try {
                    $buffer=New-Object byte[] 32768
                    $total=0L
                    while (($count=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0) {
                        $total+=$count
                        if ($total -gt $entry.Length) { throw "Archive entry exceeds declared size: $relative" }
                        $outputStream.Write($buffer,0,$count)
                    }
                    if ($total -ne $entry.Length) { throw "Truncated archive entry: $relative" }
                } finally { $outputStream.Dispose() }
            } finally { $inputStream.Dispose() }
        }
    } finally { $archive.Dispose() }
}

function Write-UpdateSignal([string]$Path, [string]$Value) {
    Assert-SafePath $Path
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try {
        $bytes=[Text.Encoding]::ASCII.GetBytes($Value)
        $stream.Write($bytes,0,$bytes.Length)
    } finally { $stream.Dispose() }
}

if ($MyInvocation.InvocationName -eq '.') { return } # Allow isolated metadata/archive tests.
# A game launched from PowerShell 7 can inherit its module path. Use the
# Windows PowerShell modules beside this worker's executable.
$env:PSModulePath=Join-Path $PSHOME 'Modules'
if ($RunningVersion -cnotmatch "^$versionPattern$" -or $RequestId -cnotmatch '^[0-9]{1,10}-[0-9]{1,20}-[0-9]{1,10}$') {
    exit 2
}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$lock=$game=$null
$stage=$null
$result=Join-Path $PSScriptRoot "request-$RequestId.result"
$ready=Join-Path $PSScriptRoot "request-$RequestId.ready"
$status=Join-Path $PSScriptRoot 'status.log'
$reopen=$false
$outcome='error'
. "$PSScriptRoot\install-common.ps1"
try {
    Assert-SafePath $PSScriptRoot -Directory
    Assert-SafePath $status
    foreach ($name in 'LFS.exe','LfsPad.ini','dxgi.dll','addons\LfsPad\installation.json') {
        Assert-SafePath (Join-Path $root $name)
    }
    $gameExe=Join-Path $root 'LFS.exe'
    $game=Get-Process -Id $GamePid -ErrorAction SilentlyContinue
    if (!$game -or $game.Path -ne $gameExe) { throw 'The current LFS process could not be verified.' }
    $null=$game.Handle
    if ($Automatic -and !(Test-AutoUpdateEnabled $root)) { throw 'Automatic update checks are disabled.' }
    $lockPath=Join-Path $PSScriptRoot 'check.lock'
    Assert-SafePath $lockPath
    $lock=[IO.FileStream]::new($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,
        [IO.FileShare]::None,1,[IO.FileOptions]::DeleteOnClose)
    $manifest=Join-Path $root 'addons\LfsPad\installation.json'
    $installation=Read-Installation $root $manifest
    if ($installation.Legacy) { throw 'Install the latest LfsPad setup before using in-game updates.' }
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    if ($Phase -eq 'Check') {
        $newest=Get-ReleaseMetadata '?per_page=30' |
            Where-Object { $_.draft -eq $false -and $_.tag_name -cmatch "^v$versionPattern$" } |
            Sort-Object { [version]$_.tag_name.Substring(1) } -Descending | Select-Object -First 1
        $asset=if ($newest) { Get-ReleaseAsset $newest ([version]$RunningVersion) }
        $outcome=if ($asset) { "offer $($asset.Version) $($asset.Hash)" } else { 'current' }
        return
    }
    if ($TargetVersion -cnotmatch "^$versionPattern$" -or $ExpectedArchiveHash -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Invalid update approval.'
    }
    $release=Get-ReleaseMetadata "/tags/v$TargetVersion"
    $asset=Get-ReleaseAsset $release ([version]$RunningVersion)
    if (!$asset -or $asset.Version -cne $TargetVersion -or $asset.Hash -cne $ExpectedArchiveHash) {
        throw 'The approved update package changed. Check again before installing.'
    }
    $stage=Join-Path $PSScriptRoot ("pending-"+[guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    $archive=Join-Path $stage 'release.zip'
    $file=[IO.File]::Open($archive,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $received=Receive-ReleaseFile $asset.Uri $file $asset.Size }
    finally { $file.Dispose() }
    if ($received -ne $asset.Size -or (Get-FileHash -LiteralPath $archive).Hash -ne $asset.Hash) {
        throw 'The download size or checksum did not match the release.'
    }
    Copy-UpdateEntries $archive $stage $TargetVersion
    $dll=Join-Path $stage 'dxgi.dll'
    Assert-Win32Dll $dll
    $dllHash=(Get-FileHash -LiteralPath $dll).Hash
    if ($game.HasExited) { throw 'LFS exited before the update was ready.' }
    Write-UpdateSignal $ready 'ready'
    if (!$game.WaitForExit(45000)) { throw 'LFS did not close within 45 seconds.' }
    $reopen=$true
    & "$PSScriptRoot\install.ps1" -GameDirectory $root -DllPath $dll -Update -WorkerSource $stage `
        -ExpectedCurrentHash $installation.CurrentHash -ExpectedNewHash $dllHash -ManualApproval:(!$Automatic) | Out-Null
    $outcome='installed'
} catch {
    $detail=$_.Exception.Message
} finally {
    if ($stage) {
        foreach ($name in 'release.zip','dxgi.dll','install-common.ps1','install.ps1','update.ps1') {
            try { [IO.File]::Delete((Join-Path $stage $name)) } catch { }
        }
        try { [IO.Directory]::Delete($stage) } catch { }
    }
    if ($lock) { $lock.Dispose() }
    $gameOpen=$game -and !$game.HasExited
    if ($game) { $game.Dispose() }
    if ($reopen) {
        try {
            Assert-SafePath $gameExe
            if (!(Get-Process LFS -ErrorAction SilentlyContinue | Where-Object Path -eq $gameExe)) {
                Start-Process -FilePath $gameExe -WorkingDirectory $root
            }
        } catch { $detail="Couldn't restart LFS. $($_.Exception.Message)" }
    }
    try {
        Assert-SafePath $status
        # Replace the log's directory entry without writing through hardlinks.
        $newLog=Join-Path $PSScriptRoot "request-$RequestId.log"
        $logOutcome=if ($outcome.StartsWith('offer ')) { "offer $($asset.Version)" } else { $outcome }
        Write-UpdateSignal $newLog "$([DateTime]::UtcNow.ToString('s'))Z $Phase $logOutcome $detail"
        if ([IO.File]::Exists($status)) {
            $oldLog=Join-Path $PSScriptRoot "request-$RequestId.old"
            [IO.File]::Replace($newLog,$status,$oldLog)
            [IO.File]::Delete($oldLog)
        } else {
            [IO.File]::Move($newLog,$status)
        }
    } catch { }
    if ($gameOpen) {
        try { Write-UpdateSignal $result $outcome } catch { }
    }
    try { [IO.File]::Delete($ready) } catch { }
}
