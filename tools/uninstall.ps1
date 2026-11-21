param([string]$GameDirectory='C:\LFS')
$ErrorActionPreference='Stop'
$gameRoot=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
if (Get-Process LFS -ErrorAction SilentlyContinue) { throw 'Close LFS before uninstalling.' }
$manifestPath="$gameRoot\addons\LfsPad\installation.json"
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ((Get-FileHash -LiteralPath "$gameRoot\dinput8.dll").Hash -ne $manifest.InstalledHash) {
    throw 'dinput8.dll changed after installation. No files were overwritten.'
}
$backupRoot=[IO.Path]::GetFullPath($manifest.BackupDirectory)
if (!$backupRoot.StartsWith("$gameRoot\addons\LfsPad\backups\",[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected backup location.' }
foreach ($relative in @('dinput8.dll','LfsPad.ini')) {
    if ($manifest.ExistingFiles -contains $relative) {
        if (!(Test-Path -LiteralPath (Join-Path $backupRoot $relative))) { throw "Missing original backup: $relative" }
    }
}
$previous="$gameRoot\LfsPad.previous.dll"
if ((Test-Path -LiteralPath $previous) -and ($manifest.ExistingFiles -contains 'dinput8.dll')) {
    $originalHash=(Get-FileHash -LiteralPath "$backupRoot\dinput8.dll").Hash
    if ((Get-FileHash -LiteralPath $previous).Hash -ne $originalHash) { throw 'The chained DLL changed. Preserve it before uninstalling.' }
}
$autoexec="$gameRoot\data\script\autoexec.lfs"
$cleanScript=$null
if ($manifest.ScriptChanged -and (Test-Path -LiteralPath $autoexec)) {
    $current=[IO.File]::ReadAllText($autoexec)
    # Leave the preceding line ending in place so later user commands cannot
    # become joined to the original final line.
    $cleanScript=$current -replace '(?m)^// LfsPad state connection\r?\n/insim 29999(?:\r?\n|$)', ''
}
if ($manifest.ExistingFiles -contains 'dinput8.dll') {
    Copy-Item -LiteralPath "$backupRoot\dinput8.dll" -Destination "$gameRoot\dinput8.dll" -Force
} else { Remove-Item -LiteralPath "$gameRoot\dinput8.dll" }
if (Test-Path -LiteralPath $previous) { Remove-Item -LiteralPath $previous }
if ($manifest.ExistingFiles -contains 'LfsPad.ini') { Copy-Item -LiteralPath "$backupRoot\LfsPad.ini" -Destination "$gameRoot\LfsPad.ini" -Force }
if ($null -ne $cleanScript) { [IO.File]::WriteAllText($autoexec,$cleanScript) }
Move-Item -LiteralPath $manifestPath -Destination "$backupRoot\uninstalled.json"
Write-Output 'LfsPad removed. Original DLL restored; backups and user settings retained.'
