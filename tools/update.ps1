param([Parameter(Mandatory=$true)][int]$GamePid,
      [Parameter(Mandatory=$true)][string]$RunningVersion)

$ErrorActionPreference='Stop'
# A game launched from PowerShell 7 can inherit its module path. Use the
# Windows PowerShell modules beside this worker's executable.
$env:PSModulePath=Join-Path $PSHOME 'Modules'
$repo='bervictus/LfsPad'
$versionPattern='(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})'
$maxDllSize=16MB

function Get-ReleaseAsset($Release, [version]$Current) {
    if ($Release.draft -ne $false -or $Release.prerelease -ne $false -or
        $Release.tag_name -cnotmatch "^v($versionPattern)$") { return }
    if ([version]$Matches[1] -le $Current) { return }
    $name="LfsPad-$($Matches[1])-update-win32.dll"
    $url="https://github.com/$repo/releases/download/$($Release.tag_name)/$name"
    $assets=@($Release.assets | Where-Object { $_.name -ceq $name })
    if ($assets.Count -ne 1) { throw 'The release has no unique update DLL.' }
    $asset=$assets[0]
    if ($asset.state -ne 'uploaded' -or $asset.browser_download_url -cne $url -or
        $asset.size -isnot [ValueType] -or $asset.size -le 0 -or $asset.size -gt $maxDllSize -or
        $asset.digest -cnotmatch '^sha256:([0-9a-fA-F]{64})$') {
        throw 'GitHub did not provide valid checksums and size for the update DLL.'
    }
    @{Uri=[uri]$url; Hash=$Matches[1]; Size=[long]$asset.size}
}

function Receive-ReleaseFile([uri]$Uri, [IO.Stream]$Output, [long]$Limit) {
    $allowedHosts=@('api.github.com','github.com','release-assets.githubusercontent.com','objects.githubusercontent.com')
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
                $Uri=[uri]::new($Uri,$response.Headers['Location'])
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

$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$lock=$game=$null
$stage=$status=$null
try {
    if ($RunningVersion -cnotmatch "^$versionPattern$") { return }
    $game=Get-Process -Id $GamePid -ErrorAction SilentlyContinue
    if (!$game -or $game.Path -ne (Join-Path $root 'LFS.exe')) { return }
    # Get a process handle now so the same process is waited on even if its ID is reused.
    $null=$game.Handle
    . "$PSScriptRoot\install-common.ps1"
    if (!(Test-AutoUpdateEnabled $root)) { return }
    $lockPath=Join-Path $PSScriptRoot 'check.lock'
    Assert-SafePath $lockPath
    try {
        $lock=[IO.FileStream]::new($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,
            [IO.FileShare]::None,1,[IO.FileOptions]::DeleteOnClose)
    } catch [IO.IOException] { return }
    $status=Join-Path $PSScriptRoot 'status.log'
    Assert-SafePath $status
    $manifest=Join-Path $root 'addons\LfsPad\installation.json'
    Assert-SafePath $manifest
    Assert-SafePath (Join-Path $root 'dinput8.dll')
    Assert-SafePath (Join-Path $root 'LfsPad.previous.dll')
    $installation=Read-Installation $root $manifest
    if ($installation.CurrentHash -ne $installation.Record.InstalledHash) { return }

    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    $metadata=[IO.MemoryStream]::new()
    try {
        Receive-ReleaseFile "https://api.github.com/repos/$repo/releases/latest" $metadata 512KB | Out-Null
        $release=[Text.Encoding]::UTF8.GetString($metadata.ToArray()) | ConvertFrom-Json
    } finally { $metadata.Dispose() }
    $asset=Get-ReleaseAsset $release ([version]$RunningVersion)
    if (!$asset) {
        [IO.File]::WriteAllText($status,"$([DateTime]::UtcNow.ToString('s'))Z No newer stable release.")
        return
    }
    $stage=Join-Path $PSScriptRoot ("pending-"+[guid]::NewGuid().ToString('N')+'.dll')
    $file=[IO.File]::Open($stage,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $received=Receive-ReleaseFile $asset.Uri $file $asset.Size }
    finally { $file.Dispose() }
    if ($received -ne $asset.Size -or (Get-FileHash -LiteralPath $stage).Hash -ne $asset.Hash) {
        throw 'The update DLL checksum or size did not match the release.'
    }
    Assert-Win32Dll $stage
    [IO.File]::WriteAllText($status,"$([DateTime]::UtcNow.ToString('s'))Z $($release.tag_name) ready. Waiting for LFS to close.")
    $game.WaitForExit()
    & "$PSScriptRoot\install.ps1" -GameDirectory $root -DllPath $stage -Update `
        -ExpectedCurrentHash $installation.CurrentHash -ExpectedNewHash $asset.Hash | Out-Null
    [IO.File]::WriteAllText($status,"$([DateTime]::UtcNow.ToString('s'))Z Installed $($release.tag_name).")
} catch {
    if ($status) {
        try {
            Assert-SafePath $status
            [IO.File]::WriteAllText($status,"$([DateTime]::UtcNow.ToString('s'))Z Update skipped. $($_.Exception.Message)")
        } catch { }
    }
} finally {
    if ($stage) { [IO.File]::Delete($stage) }
    if ($lock) { $lock.Dispose() }
    if ($game) { $game.Dispose() }
}
