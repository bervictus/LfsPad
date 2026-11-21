function Assert-SafePath([string]$Path, [switch]$Directory) {
    $current=[IO.Path]::GetFullPath($Path)
    $expectDirectory=$Directory.IsPresent
    while ($current) {
        try {
            $attributes=[IO.File]::GetAttributes($current)
            if ($attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse point in installation path: $current"
            }
            if ([bool]($attributes -band [IO.FileAttributes]::Directory) -ne $expectDirectory) {
                throw "Unexpected file type: $current"
            }
        } catch [IO.FileNotFoundException], [IO.DirectoryNotFoundException] {
            # A new file or directory may not exist yet. Check its parents.
        }
        $expectDirectory=$true
        $current=[IO.Path]::GetDirectoryName($current)
    }
}

function Open-Installation([string]$Root) {
    if (!$Root -or ![IO.Directory]::Exists($Root)) { throw 'LFS directory not found.' }
    Assert-SafePath $Root -Directory
    $lockPath=Join-Path $Root '.LfsPad-operation.lock'
    Assert-SafePath $lockPath
    $lock=[IO.FileStream]::new($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,
        [IO.FileShare]::None,1,[IO.FileOptions]::DeleteOnClose)
    try {
        foreach ($game in (Get-Process LFS -ErrorAction SilentlyContinue)) {
            if (!$game.Path -or $game.Path -eq (Join-Path $Root 'LFS.exe')) {
                throw 'Close LFS before changing the installation.'
            }
        }
        foreach ($relative in @('LFS.exe','dinput8.dll','LfsPad.previous.dll','LfsPad.ini',
                'data\script\autoexec.lfs','cfg.txt','interface_cfg.txt','addons\LfsPad\installation.json')) {
            Assert-SafePath (Join-Path $Root $relative)
        }
        return $lock
    } catch {
        $lock.Dispose()
        throw
    }
}

function Read-Installation([string]$Root, [string]$ManifestPath) {
    $record=Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    if ($record -is [array] -or $record.GameDirectory -ne $Root -or
        $record.InstalledHash -notmatch '^[A-Fa-f0-9]{64}$' -or
        $record.PreviousHash -isnot [string] -or
        ($record.PreviousHash -ne '' -and $record.PreviousHash -notmatch '^[A-Fa-f0-9]{64}$') -or
        $record.ScriptChanged -isnot [bool]) {
        throw 'Invalid LfsPad installation record.'
    }
    $backup=[IO.Path]::GetFullPath($record.BackupDirectory)
    $backupParent=Join-Path $Root 'addons\LfsPad\backups'
    if ([IO.Path]::GetDirectoryName($backup) -ne $backupParent) { throw 'Unexpected backup location.' }
    Assert-SafePath $backup -Directory
    if (![IO.Directory]::Exists($backup)) { throw 'Installation backup directory is missing.' }
    $previous="$Root\LfsPad.previous.dll"
    $previousHash=if ([IO.File]::Exists($previous)) { (Get-FileHash -LiteralPath $previous).Hash } else { '' }
    if ($previousHash -ne $record.PreviousHash) { throw 'The chained DLL changed.' }
    if ($record.PreviousHash) {
        $original=Join-Path $backup 'dinput8.dll'
        Assert-SafePath $original
        if ((Get-FileHash -LiteralPath $original).Hash -ne $record.PreviousHash) {
            throw 'The backed-up DLL changed.'
        }
    }
    $currentHash=(Get-FileHash -LiteralPath "$Root\dinput8.dll").Hash
    if ($currentHash -ne $record.InstalledHash -and $currentHash -ne $previousHash) {
        throw 'Installed LfsPad does not match the installation record.'
    }
    $record.BackupDirectory=$backup
    return @{Record=$record; CurrentHash=$currentHash}
}

function New-BackupFolder([string]$Root, [string]$Prefix='') {
    $name=$Prefix+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+[guid]::NewGuid().ToString('N')
    $backup=Join-Path $Root "addons\LfsPad\backups\$name"
    Assert-SafePath $backup -Directory
    [IO.Directory]::CreateDirectory($backup).FullName
}

function Set-InstallationFile([string]$Staged, [string]$Destination) {
    if ([IO.File]::Exists($Destination)) {
        # Replace the directory entry instead of writing through links or an
        # already-open file handle. Keep the replaced file with the backup.
        $backup=Join-Path (Split-Path -Parent $Staged) ([guid]::NewGuid().ToString('N')+'.old')
        [IO.File]::Replace($Staged,$Destination,$backup)
    } else {
        [IO.File]::Move($Staged,$Destination)
    }
}

function Read-GameText([string]$Path) {
    # LFS files may use legacy code pages. Latin-1 here is a reversible
    # byte mapping for ASCII edits and preserves all other characters exactly.
    $encoding=[Text.Encoding]::GetEncoding(28591)
    $bytes=if ([IO.File]::Exists($Path)) { [IO.File]::ReadAllBytes($Path) }
    if ($bytes.Length -ge 2 -and $bytes[0] -eq 255 -and $bytes[1] -eq 254) {
        $encoding=[Text.Encoding]::Unicode
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 254 -and $bytes[1] -eq 255) {
        $encoding=[Text.Encoding]::BigEndianUnicode
    }
    $text=if ($bytes.Length) { $encoding.GetString([byte[]]$bytes) } else { '' }
    return @{Text=$text; Encoding=$encoding}
}

function Get-StartupInSimPort([string]$Text) {
    $commands=[regex]::Matches($Text,'(?im)^(?:\uFEFF|\xEF\xBB\xBF)?[ \t]*/insim(?=[ \t\r\n]|$)([^\r\n]*)')
    $port=$null
    foreach ($command in $commands) {
        $arguments=$command.Groups[1].Value
        $candidate=0
        if ($arguments -notmatch '^[ \t]*([0-9]+)[ \t]*(?://.*)?$' -or
            ![int]::TryParse($Matches[1],[ref]$candidate) -or $candidate -lt 1 -or $candidate -gt 65535) {
            throw 'Invalid InSim port in autoexec.lfs. Use /insim followed by a port from 1 to 65535.'
        }
        $port=$candidate
    }
    return $port
}

function Set-LfsPadInSimPort([string]$Text, [int]$Port) {
    $line="InSimPort=$Port"
    $eol=if ($Text -match '\r?\n') { $Matches[0] } else { [Environment]::NewLine }
    $general=[regex]::Match($Text,'(?im)^(?:\uFEFF|\xEF\xBB\xBF)?[ \t]*\[General\][^\r\n]*(?:\r?\n|$)')
    if (!$general.Success) {
        if ($Port -eq 29999) { return $Text } # The DLL already defaults to 29999.
        if ($Text -and !$Text.EndsWith([string][char]10)) { $Text+=$eol }
        return ($Text+'[General]'+$eol+$line+$eol)
    }
    $offset=$general.Index+$general.Length
    $remaining=$Text.Substring($offset)
    $next=[regex]::Match($remaining,'(?m)^[ \t]*\[')
    $bodyLength=if ($next.Success) { $next.Index } else { $remaining.Length }
    $body=$remaining.Substring(0,$bodyLength)
    $key='(?im)^[ \t]*InSimPort[ \t]*=[^\r\n]*'
    if ($body -match $key) {
        $body=$body -replace $key,$line
    } elseif ($Port -ne 29999) {
        if (!$general.Value.EndsWith([string][char]10)) { $line=$eol+$line }
        $body=$line+$eol+$body
    }
    return ($Text.Substring(0,$offset)+$body+$remaining.Substring($bodyLength))
}

function Assert-Win32Dll([string]$Path) {
    $file=[IO.File]::OpenRead($Path)
    try {
        $reader=[IO.BinaryReader]::new($file)
        if ($file.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) { throw 'Package DLL is invalid.' }
        $file.Position=60
        $offset=$reader.ReadInt32()
        if ($offset -lt 64 -or $offset -gt $file.Length-26) { throw 'Package DLL is invalid.' }
        $file.Position=$offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x14c) {
            throw 'Package must contain a 32-bit Windows DLL.'
        }
        $file.Position=$offset+22
        if (!($reader.ReadUInt16() -band 0x2000) -or $reader.ReadUInt16() -ne 0x10b) {
            throw 'Package must contain a 32-bit Windows DLL.'
        }
    } finally {
        $file.Dispose()
    }
}

