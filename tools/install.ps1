param([string]$GameDirectory='C:\LFS', [string]$DllPath='', [switch]$Diagnostics, [switch]$Update)
$ErrorActionPreference='Stop'
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
if (Get-Process LFS -ErrorAction SilentlyContinue) { throw 'Close LFS before installing.' }
if (!(Test-Path -LiteralPath "$gameRoot\LFS.exe")) { throw 'LFS.exe not found.' }
if ((Get-FileHash -LiteralPath "$gameRoot\LFS.exe").Hash -ne '77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52') {
    throw 'Only the verified 0.8C38 executable is supported.'
}
if (!$DllPath) {
    $DllPath="$PSScriptRoot\..\dinput8.dll"
    if (!(Test-Path -LiteralPath $DllPath)) { $DllPath="$PSScriptRoot\..\build\dinput8.dll" }
}
$dllFull=[IO.Path]::GetFullPath($DllPath)
$iniSource="$PSScriptRoot\..\LfsPad.ini"
if (!(Test-Path -LiteralPath $dllFull)) { throw "Build not found: $dllFull" }
if (!(Test-Path -LiteralPath $iniSource)) { throw 'Package configuration is missing.' }
$packageRoot="$gameRoot\addons\LfsPad"
$manifestPath="$packageRoot\installation.json"
if ($Update) {
    if ($Diagnostics) { throw 'Change Diagnostics in LfsPad.ini after updating if needed.' }
    if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Install LfsPad before updating.' }
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $installedDll="$gameRoot\dinput8.dll"
    $previousDll="$gameRoot\LfsPad.previous.dll"
    if ($manifest.GameDirectory -ne $gameRoot -or
        (Get-FileHash -LiteralPath $installedDll).Hash -ne $manifest.InstalledHash) {
        throw 'Installed LfsPad does not match the installation record.'
    }
    $previousHash=if (Test-Path -LiteralPath $previousDll) {
        (Get-FileHash -LiteralPath $previousDll).Hash
    } else { '' }
    if ($previousHash -ne $manifest.PreviousHash) { throw 'Previous DLL was changed.' }
    $newHash=(Get-FileHash -LiteralPath $dllFull).Hash
    if ($newHash -eq $manifest.InstalledHash) {
        Write-Output 'This LfsPad build is already installed.'
        return
    }
    $updateBackup="$packageRoot\backups\update-$(Get-Date -Format yyyyMMdd-HHmmss-fff)"
    New-Item -ItemType Directory -Path $updateBackup | Out-Null
    $stagedDll="$updateBackup\dinput8.new"
    $stagedManifest="$updateBackup\installation.new"
    $savedDll="$updateBackup\dinput8.dll"
    Copy-Item -LiteralPath $dllFull -Destination $stagedDll
    if ((Get-FileHash -LiteralPath $stagedDll).Hash -ne $newHash) { throw 'Update copy verification failed.' }
    $manifest.InstalledHash=$newHash
    $manifest | ConvertTo-Json | Set-Content -LiteralPath $stagedManifest -Encoding utf8
    [IO.File]::Replace($stagedDll,$installedDll,$savedDll)
    try {
        [IO.File]::Replace($stagedManifest,$manifestPath,"$updateBackup\installation.json")
    } catch {
        [IO.File]::Replace($savedDll,$installedDll,$null)
        throw
    }
    Write-Output "Updated LfsPad. Previous release backed up in $updateBackup"
    return
}
if (Test-Path -LiteralPath $manifestPath) { throw 'Existing LfsPad installation: uninstall before replacing it.' }
if (Test-Path -LiteralPath "$gameRoot\LfsPad.previous.dll") { throw 'LfsPad.previous.dll already exists; preserve and resolve it before installing.' }
$autoexec="$gameRoot\data\script\autoexec.lfs"
$scriptText=if (Test-Path -LiteralPath $autoexec) { [IO.File]::ReadAllText($autoexec) } else { '' }
if ($scriptText -match '(?im)^/insim\s+(\d+)' -and $Matches[1] -ne '29999') {
    throw 'Existing InSim port differs from 29999. Configure this package for that port first.'
}
$scriptChanged=!($scriptText -match '(?im)^/insim\s+29999\s*$')
$backupRoot="$packageRoot\backups\$(Get-Date -Format yyyyMMdd-HHmmss-fff)"
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$existing=@()
foreach ($relative in @('dinput8.dll','cfg.txt','interface_cfg.txt','data\script\autoexec.lfs','LfsPad.ini')) {
    $original=Join-Path $gameRoot $relative
    if (Test-Path -LiteralPath $original) {
        $saved=Join-Path $backupRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
        Copy-Item -LiteralPath $original -Destination $saved
        if ((Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath $saved).Hash) { throw "Backup verification failed: $relative" }
        $existing+=$relative
    }
}
$previousCreated=$false
try {
    if ($existing -contains 'dinput8.dll') {
        Copy-Item -LiteralPath "$gameRoot\dinput8.dll" -Destination "$gameRoot\LfsPad.previous.dll"
        $previousCreated=$true
    }
    Copy-Item -LiteralPath $dllFull -Destination "$gameRoot\dinput8.dll" -Force
    if (!($existing -contains 'LfsPad.ini')) { Copy-Item -LiteralPath $iniSource -Destination "$gameRoot\LfsPad.ini" }
    if ($Diagnostics) {
        $text=[IO.File]::ReadAllText("$gameRoot\LfsPad.ini")
        [IO.File]::WriteAllText("$gameRoot\LfsPad.ini", ($text -replace '(?im)^Diagnostics=\d+', 'Diagnostics=1'))
    }
    if ($scriptChanged) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $autoexec) -Force | Out-Null
        Add-Content -LiteralPath $autoexec -Value "`r`n// LfsPad state connection`r`n/insim 29999"
    }
    $manifest=@{
        GameDirectory=$gameRoot; BackupDirectory=$backupRoot; ExistingFiles=$existing
        ScriptChanged=$scriptChanged; InstalledHash=(Get-FileHash -LiteralPath "$gameRoot\dinput8.dll").Hash
        PreviousHash=if ($previousCreated) { (Get-FileHash -LiteralPath "$gameRoot\LfsPad.previous.dll").Hash } else { '' }
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath "$manifestPath.tmp" -Encoding utf8
    Move-Item -LiteralPath "$manifestPath.tmp" -Destination $manifestPath
} catch {
    $installError=$_
    foreach ($relative in @('dinput8.dll','LfsPad.ini','data\script\autoexec.lfs')) {
        $destination=Join-Path $gameRoot $relative
        if ($existing -contains $relative) { Copy-Item -LiteralPath (Join-Path $backupRoot $relative) -Destination $destination -Force }
        elseif (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination }
    }
    if ($previousCreated) { Remove-Item -LiteralPath "$gameRoot\LfsPad.previous.dll" }
    if (Test-Path -LiteralPath "$manifestPath.tmp") { Remove-Item -LiteralPath "$manifestPath.tmp" }
    throw $installError
}
Write-Output "Installed LfsPad. Verified backup: $backupRoot"
