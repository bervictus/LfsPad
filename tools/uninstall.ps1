param([string]$GameDirectory='C:\LFS')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\install-common.ps1"
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$lock=Open-Installation $gameRoot
try {
    $manifestPath="$gameRoot\addons\LfsPad\installation.json"
    $manifest=(Read-Installation $gameRoot $manifestPath).Record
    $backup=New-BackupFolder $gameRoot 'remove-'
    $installed="$gameRoot\dinput8.dll"
    $previous="$gameRoot\LfsPad.previous.dll"
    $scriptPath="$gameRoot\data\script\autoexec.lfs"
    $restoreDll="$backup\dinput8.original"
    $cleanScript="$backup\autoexec.clean"
    if ($manifest.PreviousHash) {
        Copy-Item -LiteralPath "$($manifest.BackupDirectory)\dinput8.dll" -Destination $restoreDll
        if ((Get-FileHash -LiteralPath $restoreDll).Hash -ne $manifest.PreviousHash) {
            throw 'Original DLL copy verification failed.'
        }
    }
    if ($manifest.ScriptChanged -and [IO.File]::Exists($scriptPath)) {
        $scriptFile=Read-GameText $scriptPath
        $clean=$scriptFile.Text -creplace '(?m)^// LfsPad state connection\r?\n/insim 29999(?:\r?\n|$)', ''
        if ($clean -cne $scriptFile.Text) {
            [IO.File]::WriteAllBytes($cleanScript,$scriptFile.Encoding.GetBytes($clean))
        }
    }

    $savedDll="$backup\dinput8.dll"
    $savedPrevious="$backup\LfsPad.previous.dll"
    $savedScript="$backup\autoexec.lfs"
    try {
        if ($manifest.PreviousHash) {
            [IO.File]::Replace($restoreDll,$installed,$savedDll)
            [IO.File]::Move($previous,$savedPrevious)
        } else {
            [IO.File]::Move($installed,$savedDll)
        }
        if ([IO.File]::Exists($cleanScript)) {
            [IO.File]::Replace($cleanScript,$scriptPath,$savedScript)
        }
        [IO.File]::Move($manifestPath,"$backup\uninstalled.json")
    } catch {
        if ([IO.File]::Exists($savedScript)) { Set-InstallationFile $savedScript $scriptPath }
        if ([IO.File]::Exists($savedPrevious)) { Set-InstallationFile $savedPrevious $previous }
        if ([IO.File]::Exists($savedDll)) { Set-InstallationFile $savedDll $installed }
        throw
    }
    Write-Output 'LfsPad removed. Original DLL restored when present. User settings and backups retained.'
} finally { $lock.Dispose() }
