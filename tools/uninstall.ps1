param([string]$GameDirectory='C:\LFS')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\install-common.ps1"
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$lock=Open-Installation $gameRoot
try {
    $manifestFile='addons\LfsPad\installation.json'
    $installation=Read-Installation $gameRoot (Join-Path $gameRoot $manifestFile)
    $manifest=$installation.Record
    $backup=New-BackupFolder $gameRoot 'remove-'
    $changes=[ordered]@{}
    if ($installation.Legacy) {
        if ($installation.CurrentHash -eq $manifest.InstalledHash) {
            $restore=$null
            if ($manifest.PreviousHash) {
                $restore="$backup\dinput8.original"
                Copy-Item -LiteralPath "$($manifest.BackupDirectory)\dinput8.dll" -Destination $restore
                if ((Get-FileHash -LiteralPath $restore).Hash -ne $manifest.PreviousHash) {
                    throw 'Original DLL copy verification failed.'
                }
            }
            $changes['dinput8.dll']=$restore
        }
        if ($manifest.PreviousHash) { $changes['LfsPad.previous.dll']=$null }
    } else {
        $changes['dxgi.dll']=$null
    }
    $autoexecFile='data\script\autoexec.lfs'
    $scriptPath=Join-Path $gameRoot $autoexecFile
    if ($manifest.ScriptChanged -and [IO.File]::Exists($scriptPath)) {
        $scriptFile=Read-GameText $scriptPath
        $clean=$scriptFile.Text -creplace '(?m)^// LfsPad state connection\r?\n/insim [0-9]{1,5}(?:\r?\n|$)', ''
        if ($clean -cne $scriptFile.Text) {
            $changes[$autoexecFile]="$backup\autoexec.clean"
            [IO.File]::WriteAllBytes($changes[$autoexecFile],$scriptFile.Encoding.GetBytes($clean))
        }
    }
    $changes[$manifestFile]=$null
    Set-InstallationFiles $gameRoot $backup $changes
    Write-Output 'LfsPad removed. User settings and backups retained.'
} finally { $lock.Dispose() }
