param([string]$GameDirectory='C:\LFS', [string]$DllPath='', [switch]$Diagnostics, [switch]$Update,
    [string]$ExpectedCurrentHash='', [string]$ExpectedNewHash='', [switch]$ManualApproval,
    [string]$WorkerSource=$PSScriptRoot)

$ErrorActionPreference='Stop'
. "$PSScriptRoot\install-common.ps1"
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$lock=Open-Installation $gameRoot
try {
    if (![IO.File]::Exists("$gameRoot\LFS.exe")) { throw 'LFS.exe not found.' }
    if ((Get-FileHash -LiteralPath "$gameRoot\LFS.exe").Hash -ne '77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52') {
        throw 'Only the verified 0.8C38 executable is supported.'
    }
    if (!$DllPath) {
        $DllPath="$PSScriptRoot\..\dxgi.dll"
        if (![IO.File]::Exists($DllPath)) { $DllPath="$PSScriptRoot\..\build\dxgi.dll" }
    }
    $dllFull=[IO.Path]::GetFullPath($DllPath)
    Assert-Win32Dll $dllFull
    $newHash=(Get-FileHash -LiteralPath $dllFull).Hash
    $manifestFile='addons\LfsPad\installation.json'
    $manifestPath=Join-Path $gameRoot $manifestFile
    $autoexecFile='data\script\autoexec.lfs'
    $autoexec=Join-Path $gameRoot $autoexecFile
    $automatic=[bool]($ExpectedCurrentHash -or $ExpectedNewHash)
    if ($automatic -and !$Update) { throw 'An automatic update requires an existing installation.' }

    $changes=[ordered]@{}
    $scriptChanged=$false
    if ($Update) {
        if ($Diagnostics) { throw 'Change Diagnostics in LfsPad.ini after updating if needed.' }
        $installation=Read-Installation $gameRoot $manifestPath -Migrate:(!$automatic)
        $old=$installation.Record
        $newLoader=$installation.Legacy
        if ($automatic -and ($newLoader -or $ExpectedCurrentHash -ne $installation.CurrentHash -or
                $ExpectedNewHash -ne $newHash -or (!$ManualApproval -and !(Test-AutoUpdateEnabled $gameRoot)))) {
            throw 'Update cancelled because the installation or settings changed.'
        }
        if (!$automatic) {
            $scriptFile=Read-GameText $autoexec
            if ($null -eq (Get-StartupInSimPort $scriptFile.Text)) {
                $port=Get-LfsPadGeneralInt $gameRoot 'InSimPort' 29999
                if ($port -lt 1 -or $port -gt 65535) { $port=29999 }
                $scriptChanged=$true
            }
        }
    } else {
        if ([IO.File]::Exists($manifestPath)) { throw 'Existing LfsPad installation: use -Update.' }
        if ([IO.File]::Exists("$gameRoot\LfsPad.previous.dll")) {
            throw 'LfsPad.previous.dll exists without an installation record.'
        }
        $newLoader=$true
        $iniSource="$PSScriptRoot\..\LfsPad.ini"
        if (![IO.File]::Exists($iniSource)) { throw 'Package configuration is missing.' }
        $scriptFile=Read-GameText $autoexec
        $port=Get-StartupInSimPort $scriptFile.Text
        $scriptChanged=$null -eq $port
        if ($scriptChanged) { $port=29999 }
    }
    if ($newLoader -and [IO.File]::Exists("$gameRoot\dxgi.dll")) {
        throw 'Another dxgi.dll is already installed in the game folder.'
    }
    if ($Update -and !$newLoader -and !$scriptChanged -and $newHash -eq $installation.CurrentHash) {
        if ($WorkerSource -ne "$gameRoot\addons\LfsPad\update" -and
                [IO.File]::Exists((Join-Path $WorkerSource 'update.ps1'))) {
            $backup=New-BackupFolder $gameRoot 'worker-'
            Add-UpdateWorkerChanges $gameRoot $WorkerSource $backup $changes
            Set-InstallationFiles $gameRoot $backup $changes
        }
        Write-Output 'This LfsPad build is already installed.'
        return
    }

    $backup=New-BackupFolder $gameRoot $(if ($Update) { 'update-' } else { '' })
    $stagedDll="$backup\dxgi.new"
    Copy-Item -LiteralPath $dllFull -Destination $stagedDll
    if ((Get-FileHash -LiteralPath $stagedDll).Hash -ne $newHash) { throw 'Install copy verification failed.' }
    if ($Update -and $newLoader) {
        if ($installation.CurrentHash -eq $old.InstalledHash) {
            if ($old.PreviousHash) {
                $restore="$backup\dinput8.original"
                Copy-Item -LiteralPath "$($old.BackupDirectory)\dinput8.dll" -Destination $restore
                if ((Get-FileHash -LiteralPath $restore).Hash -ne $old.PreviousHash) { throw 'Original DLL copy verification failed.' }
                $changes['dinput8.dll']=$restore
            } else {
                $changes['dinput8.dll']=$null
            }
        }
        if ($old.PreviousHash) { $changes['LfsPad.previous.dll']=$null }
    }
    $changes['dxgi.dll']=$stagedDll

    if (!$Update) {
        $iniPath="$gameRoot\LfsPad.ini"
        $hasIni=[IO.File]::Exists($iniPath)
        $iniFile=if ($hasIni) { Read-GameText $iniPath } else { Read-GameText $iniSource }
        $iniText=Set-LfsPadInSimPort $iniFile.Text $port
        if ($Diagnostics) { $iniText=$iniText -replace '(?im)^Diagnostics=\d+', 'Diagnostics=1' }
        if (!$hasIni -or $iniText -cne $iniFile.Text) {
            [IO.File]::WriteAllBytes("$backup\ini.new",$iniFile.Encoding.GetBytes($iniText))
            $changes['LfsPad.ini']="$backup\ini.new"
        }
    }
    if ($scriptChanged) {
        $command="`r`n// LfsPad state connection`r`n/insim $port`r`n"
        [IO.Directory]::CreateDirectory((Split-Path -Parent $autoexec)) | Out-Null
        [IO.File]::WriteAllBytes("$backup\script.new",$scriptFile.Encoding.GetBytes($scriptFile.Text+$command))
        $changes[$autoexecFile]="$backup\script.new"
    }
    $manifest=@{
        GameDirectory=$gameRoot
        BackupDirectory=if ($Update) { $old.BackupDirectory } else { $backup }
        ScriptChanged=$scriptChanged -or ($Update -and $old.ScriptChanged)
        InstalledHash=$newHash
        PreviousHash=''
        Loader='dxgi'
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath "$backup\installation.new" -Encoding utf8
    $changes[$manifestFile]="$backup\installation.new"
    Add-UpdateWorkerChanges $gameRoot $WorkerSource $backup $changes
    Set-InstallationFiles $gameRoot $backup $changes -NewLoader:$newLoader
    $operation=if ($Update) { 'Updated' } else { 'Installed' }
    Write-Output "$operation LfsPad. Backup: $backup"
} finally { $lock.Dispose() }
