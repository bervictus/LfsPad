param([string]$GameDirectory='C:\LFS', [string]$DllPath='', [switch]$Diagnostics, [switch]$Update)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\install-common.ps1"
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$lock=Open-Installation $gameRoot
try {
    if (!(Test-Path -LiteralPath "$gameRoot\LFS.exe" -PathType Leaf)) { throw 'LFS.exe not found.' }
    if ((Get-FileHash -LiteralPath "$gameRoot\LFS.exe").Hash -ne '77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52') {
        throw 'Only the verified 0.8C38 executable is supported.'
    }
    if (!$DllPath) {
        $DllPath="$PSScriptRoot\..\dinput8.dll"
        if (!(Test-Path -LiteralPath $DllPath)) { $DllPath="$PSScriptRoot\..\build\dinput8.dll" }
    }
    $dllFull=[IO.Path]::GetFullPath($DllPath)
    $iniSource="$PSScriptRoot\..\LfsPad.ini"
    Assert-Win32Dll $dllFull
    $newHash=(Get-FileHash -LiteralPath $dllFull).Hash
    $manifestPath="$gameRoot\addons\LfsPad\installation.json"
    $installedDll="$gameRoot\dinput8.dll"
    $previousDll="$gameRoot\LfsPad.previous.dll"

    if ($Update) {
        if ($Diagnostics) { throw 'Change Diagnostics in LfsPad.ini after updating if needed.' }
        $installation=Read-Installation $gameRoot $manifestPath
        $manifest=$installation.Record
        if ($newHash -eq $installation.CurrentHash -and $newHash -eq $manifest.InstalledHash) {
            Write-Output 'This LfsPad build is already installed.'
            return
        }
        $backup=New-BackupFolder $gameRoot 'update-'
        $stagedDll="$backup\dinput8.new"
        $stagedManifest="$backup\installation.new"
        $savedDll="$backup\dinput8.dll"
        Copy-Item -LiteralPath $dllFull -Destination $stagedDll
        if ((Get-FileHash -LiteralPath $stagedDll).Hash -ne $newHash) { throw 'Update copy verification failed.' }
        $manifest.InstalledHash=$newHash
        $manifest | ConvertTo-Json | Set-Content -LiteralPath $stagedManifest -Encoding utf8
        [IO.File]::Replace($stagedDll,$installedDll,$savedDll)
        try {
            [IO.File]::Replace($stagedManifest,$manifestPath,"$backup\installation.json")
        } catch {
            Set-InstallationFile $savedDll $installedDll
            throw
        }
        Write-Output "Updated LfsPad. Previous release backed up in $backup"
        return
    }

    if ([IO.File]::Exists($manifestPath)) { throw 'Existing LfsPad installation: use -Update.' }
    if ([IO.File]::Exists($previousDll)) { throw 'LfsPad.previous.dll already exists; preserve and resolve it before installing.' }
    if (![IO.File]::Exists($iniSource)) { throw 'Package configuration is missing.' }
    $autoexec="$gameRoot\data\script\autoexec.lfs"
    $scriptFile=Read-GameText $autoexec
    $port=Get-StartupInSimPort $scriptFile.Text
    $scriptChanged=$null -eq $port
    if ($scriptChanged) { $port=29999 }

    $iniPath="$gameRoot\LfsPad.ini"
    $hasIni=[IO.File]::Exists($iniPath)
    $iniFile=if ($hasIni) { Read-GameText $iniPath } else { Read-GameText $iniSource }
    $iniText=Set-LfsPadInSimPort $iniFile.Text $port
    if ($Diagnostics) { $iniText=$iniText -replace '(?im)^Diagnostics=\d+', 'Diagnostics=1' }
    $backup=New-BackupFolder $gameRoot
    $existing=@()
    foreach ($relative in @('dinput8.dll','cfg.txt','interface_cfg.txt','data\script\autoexec.lfs','LfsPad.ini')) {
        $original=Join-Path $gameRoot $relative
        if ([IO.File]::Exists($original)) {
            $saved=Join-Path $backup $relative
            [IO.Directory]::CreateDirectory((Split-Path -Parent $saved)) | Out-Null
            Copy-Item -LiteralPath $original -Destination $saved
            if ((Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath $saved).Hash) {
                throw "Backup verification failed: $relative"
            }
            $existing+=$relative
        }
    }

    $stagedDll="$backup\dinput8.new"
    Copy-Item -LiteralPath $dllFull -Destination $stagedDll
    if ((Get-FileHash -LiteralPath $stagedDll).Hash -ne $newHash) { throw 'Install copy verification failed.' }
    $manifest=@{
        GameDirectory=$gameRoot; BackupDirectory=$backup; ExistingFiles=$existing
        ScriptChanged=$scriptChanged; InstalledHash=$newHash; PreviousHash=''
    }
    $changes=[ordered]@{'dinput8.dll'=$stagedDll}
    if ($existing -contains 'dinput8.dll') {
        $manifest.PreviousHash=(Get-FileHash -LiteralPath "$backup\dinput8.dll").Hash
        if ($manifest.PreviousHash -eq $newHash) {
            throw 'LfsPad DLL is already in this folder without an installation record.'
        }
        $changes['LfsPad.previous.dll']="$backup\previous.new"
        Copy-Item -LiteralPath "$backup\dinput8.dll" -Destination $changes['LfsPad.previous.dll']
    }
    if (!$hasIni -or $iniText -cne $iniFile.Text) {
        $ini="$backup\ini.new"
        [IO.File]::WriteAllBytes($ini,$iniFile.Encoding.GetBytes($iniText))
        $changes['LfsPad.ini']=$ini
    }
    if ($scriptChanged) {
        $command="`r`n// LfsPad state connection`r`n/insim 29999`r`n"
        [IO.Directory]::CreateDirectory((Split-Path -Parent $autoexec)) | Out-Null
        [IO.File]::WriteAllBytes("$backup\script.new",$scriptFile.Encoding.GetBytes($scriptFile.Text+$command))
        $changes['data\script\autoexec.lfs']="$backup\script.new"
    }
    $stagedManifest="$backup\installation.new"
    $manifest | ConvertTo-Json | Set-Content -LiteralPath $stagedManifest -Encoding utf8
    $applied=@()
    try {
        foreach ($relative in $changes.Keys) {
            Set-InstallationFile $changes[$relative] (Join-Path $gameRoot $relative)
            $applied+=$relative
        }
        [IO.File]::Move($stagedManifest,$manifestPath)
    } catch {
        foreach ($relative in $applied) {
            $destination=Join-Path $gameRoot $relative
            if ($existing -contains $relative) {
                $restore=Join-Path $backup ([guid]::NewGuid().ToString('N')+'.restore')
                Copy-Item -LiteralPath (Join-Path $backup $relative) -Destination $restore
                Set-InstallationFile $restore $destination
            } else { [IO.File]::Delete($destination) }
        }
        throw
    }
    Write-Output "Installed LfsPad. Verified backup: $backup"
} finally { $lock.Dispose() }
