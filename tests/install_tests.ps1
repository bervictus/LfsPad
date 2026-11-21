param([string]$VerifiedGameExe='C:\LFS\LFS.exe')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$fixtureParent=[IO.Path]::GetFullPath("$projectRoot\artifacts")
$fixtureRoot=Join-Path $fixtureParent ("install-tests-"+[guid]::NewGuid().ToString('N'))
$script:checks=0
function Check([bool]$Pass,[string]$Label) {
    $script:checks++
    if (!$Pass) { throw "Check failed: $Label" }
}
function Reject([scriptblock]$Action,[string]$Label) {
    $rejected=$false
    try { & $Action | Out-Null } catch { $rejected=$true }
    Check $rejected $Label
}
function New-Fixture([string]$Name) {
    $root=Join-Path $fixtureRoot $Name
    New-Item -ItemType Directory -Path "$root\data\script" -Force | Out-Null
    Copy-Item -LiteralPath $VerifiedGameExe -Destination "$root\LFS.exe"
    return $root
}
function Install([string]$Root) { & "$projectRoot\tools\install.ps1" -GameDirectory $Root | Out-Null }
function Uninstall([string]$Root) { & "$projectRoot\tools\uninstall.ps1" -GameDirectory $Root | Out-Null }
try {
    $root=New-Fixture 'rfs-loader'
    $directInput="$root\dinput8.dll"
    $dxgi="$root\dxgi.dll"
    $ini="$root\LfsPad.ini"
    [IO.File]::WriteAllText($directInput,'original-rfs-loader-fixture')
    [IO.File]::WriteAllText($ini,"[Input]"+[Environment]::NewLine+"DeadzonePercent=31")
    [IO.File]::WriteAllText("$root\data\script\autoexec.lfs",'/insim 29999')
    $originalHash=(Get-FileHash -LiteralPath $directInput).Hash
    $scriptHash=(Get-FileHash -LiteralPath "$root\data\script\autoexec.lfs").Hash
    $iniHash=(Get-FileHash -LiteralPath $ini).Hash
    $builtDll="$projectRoot\build\dxgi.dll"
    Install $root
    $currentHash=(Get-FileHash -LiteralPath $builtDll).Hash
    Check ((Get-FileHash -LiteralPath $directInput).Hash -eq $originalHash) 'RFS keeps its own dinput8 on installation'
    Check ((Get-FileHash -LiteralPath $dxgi).Hash -eq $currentHash) 'LfsPad uses a separate loader'
    Check (![IO.File]::Exists("$root\LfsPad.previous.dll")) 'no extra copy of RFS loader created'
    Check ((Get-FileHash -LiteralPath "$root\data\script\autoexec.lfs").Hash -eq $scriptHash) 'existing InSim script unchanged'
    Check ((Get-FileHash -LiteralPath $ini).Hash -eq $iniHash) 'existing settings retained'
    $worker="$root\addons\LfsPad\update\update.ps1"
    Check ((Get-FileHash -LiteralPath $worker).Hash -eq
        (Get-FileHash -LiteralPath "$projectRoot\tools\update.ps1").Hash) 'updater installed'
    Reject { Install $root } 'double installation refused'
    [IO.File]::AppendAllText($dxgi,'modified')
    $modifiedHash=(Get-FileHash -LiteralPath $dxgi).Hash
    Reject { Uninstall $root } 'changed LfsPad DLL protected'
    Check ((Get-FileHash -LiteralPath $dxgi).Hash -eq $modifiedHash) 'rejected uninstall did not change DLL'
    Copy-Item -LiteralPath $builtDll -Destination $dxgi -Force

    # The RFS updater owns dinput8.dll and may replace it repeatedly.
    [IO.File]::WriteAllText($directInput,'first-rfs-update-fixture')
    $rfsUpdateHash=(Get-FileHash -LiteralPath $directInput).Hash
    $update=Join-Path $fixtureRoot 'update.dll'
    Copy-Item -LiteralPath $builtDll -Destination $update
    [IO.File]::AppendAllText($update,'updated-lfspad-fixture')
    $updateHash=(Get-FileHash -LiteralPath $update).Hash
    $workerInstaller="$root\addons\LfsPad\update\install.ps1"
    $automatic=@{GameDirectory=$root; DllPath=$update; Update=$true; ExpectedCurrentHash=$currentHash; ExpectedNewHash=$updateHash}
    $stale=$automatic.Clone()
    $stale.ExpectedCurrentHash='0'*64
    Reject { & $workerInstaller @stale } 'automatic update rejects stale installed hash'
    $invalid=$automatic.Clone()
    $invalid.ExpectedNewHash='0'*64
    Reject { & $workerInstaller @invalid } 'automatic update rejects unexpected download hash'
    $iniBefore=[IO.File]::ReadAllBytes($ini)
    [IO.File]::AppendAllLines($ini,[string[]]@('','[General]','AutoUpdate=0'))
    Reject { & $workerInstaller @automatic } 'automatic update respects new opt-out'
    Check ((Get-FileHash -LiteralPath $dxgi).Hash -eq $currentHash) 'rejected automatic updates leave DLL intact'
    [IO.File]::WriteAllBytes($ini,$iniBefore)
    & $workerInstaller @automatic | Out-Null
    Check ((Get-FileHash -LiteralPath $dxgi).Hash -eq $updateHash) 'automatic update installed after RFS replaced dinput8'
    Check ((Get-FileHash -LiteralPath $ini).Hash -eq $iniHash) 'update preserves settings'
    Check ((Get-FileHash -LiteralPath $directInput).Hash -eq $rfsUpdateHash) 'LfsPad update preserves new RFS loader'
    [IO.File]::WriteAllText($directInput,'second-rfs-update-fixture')
    $rfsUpdateHash=(Get-FileHash -LiteralPath $directInput).Hash
    Uninstall $root
    Check (![IO.File]::Exists($dxgi)) 'uninstall removes only LfsPad loader'
    Check ((Get-FileHash -LiteralPath $directInput).Hash -eq $rfsUpdateHash) 'uninstall preserves latest RFS loader'
    Check (![IO.File]::Exists("$root\addons\LfsPad\installation.json")) 'active manifest removed'
    Check ((Get-FileHash -LiteralPath $ini).Hash -eq $iniHash) 'uninstall preserves settings'

    foreach ($restored in @($false,$true)) {
        $root=New-Fixture "legacy-$restored"
        $backup="$root\addons\LfsPad\backups\legacy"
        [IO.Directory]::CreateDirectory($backup) | Out-Null
        [IO.File]::WriteAllText("$root\LfsPad.previous.dll",'original-rfs-loader')
        Copy-Item -LiteralPath "$root\LfsPad.previous.dll" -Destination "$backup\dinput8.dll"
        [IO.File]::WriteAllText("$root\dinput8.dll",'old-lfspad-loader')
        $oldHash=(Get-FileHash -LiteralPath "$root\dinput8.dll").Hash
        $previousHash=(Get-FileHash -LiteralPath "$root\LfsPad.previous.dll").Hash
        @{GameDirectory=$root; BackupDirectory=$backup; InstalledHash=$oldHash; PreviousHash=$previousHash; ScriptChanged=$false} |
            ConvertTo-Json | Set-Content -LiteralPath "$root\addons\LfsPad\installation.json"
        [IO.File]::WriteAllText("$root\LfsPad.ini",'[General]')
        $iniHash=(Get-FileHash -LiteralPath "$root\LfsPad.ini").Hash
        if ($restored) { Copy-Item -LiteralPath "$root\LfsPad.previous.dll" -Destination "$root\dinput8.dll" -Force }
        [IO.File]::AppendAllText("$backup\dinput8.dll",'modified')
        Reject { & "$projectRoot\tools\install.ps1" -GameDirectory $root -Update } 'migration refuses modified backup'
        Copy-Item -LiteralPath "$root\LfsPad.previous.dll" -Destination "$backup\dinput8.dll" -Force
        & "$projectRoot\tools\install.ps1" -GameDirectory $root -Update | Out-Null
        Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq $previousHash) "migration restores or retains RFS loader: $restored"
        Check ((Get-FileHash -LiteralPath "$root\dxgi.dll").Hash -eq $currentHash) "migration installs DXGI loader: $restored"
        Check ((Get-FileHash -LiteralPath "$root\LfsPad.ini").Hash -eq $iniHash) "migration preserves INI: $restored"
        Check (![IO.File]::Exists("$root\LfsPad.previous.dll")) "migration backs up old chain file: $restored"
        Uninstall $root
        Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq $previousHash) "post-migration uninstall leaves RFS intact: $restored"
    }

    $root=New-Fixture 'new-install'
    [IO.File]::WriteAllText("$root\data\script\autoexec.lfs","// existing`r`n/echo keep")
    Install $root
    Check ([IO.File]::ReadAllText("$root\data\script\autoexec.lfs").Contains('/insim 29999')) 'missing InSim port added'
    [IO.File]::AppendAllText("$root\data\script\autoexec.lfs","/echo later`r`n")
    [IO.File]::AppendAllText("$root\LfsPad.ini","`r`n; user preference")
    Uninstall $root
    $scriptText=[IO.File]::ReadAllText("$root\data\script\autoexec.lfs")
    Check (!$scriptText.Contains('/insim 29999')) 'owned InSim command removed'
    Check ($scriptText.Contains("/echo keep`r`n/echo later")) 'later user commands kept on separate lines'
    Check ([IO.File]::ReadAllText("$root\LfsPad.ini").Contains('user preference')) 'new user configuration kept'
    Check (!(Test-Path -LiteralPath "$root\dxgi.dll")) 'uninstall removes loader in a stock install'
    Check (!(Test-Path -LiteralPath "$root\dinput8.dll")) 'stock install does not create a DirectInput proxy'

    $root=New-Fixture 'different-port'
    $scriptPath="$root\data\script\autoexec.lfs"
    $customScript="// /insim 55555"+[Environment]::NewLine+"/insim 30000"
    [IO.File]::WriteAllText($scriptPath,$customScript)
    $scriptHash=(Get-FileHash -LiteralPath $scriptPath).Hash
    Install $root
    Check ([IO.File]::ReadAllText("$root\LfsPad.ini") -match '(?m)^InSimPort=30000\r?$') 'existing custom port copied into new config'
    Check ((Get-FileHash -LiteralPath $scriptPath).Hash -eq $scriptHash) 'custom port script left byte-for-byte intact'
    Uninstall $root
    Check ((Get-FileHash -LiteralPath $scriptPath).Hash -eq $scriptHash) 'uninstall leaves original custom port intact'

    $root=New-Fixture 'custom-port-existing-settings'
    $iniPath="$root\LfsPad.ini"
    $scriptPath="$root\data\script\autoexec.lfs"
    $customScript=@('/insim 29999','  /insim 065535') -join [Environment]::NewLine
    [IO.File]::WriteAllText($scriptPath,$customScript,[Text.Encoding]::UTF8)
    $ini=@('[General]','Language=en','; Keep this comment','InSimPort=29999','AllowRemoteInSim=1','','[Input]','DeadzonePercent=31')
    [IO.File]::WriteAllLines($iniPath,$ini,[Text.Encoding]::Unicode)
    $expected=[Text.Encoding]::Unicode.GetBytes(([IO.File]::ReadAllText($iniPath).Replace('InSimPort=29999','InSimPort=65535')))
    $expected=[Text.Encoding]::Unicode.GetPreamble()+$expected
    Install $root
    Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($iniPath)) -eq
        [Convert]::ToBase64String([byte[]]$expected)) 'effective startup port changes only the existing INI key'
    $iniHash=(Get-FileHash -LiteralPath $iniPath).Hash
    & "$projectRoot\tools\install.ps1" -GameDirectory $root -DllPath $update -Update | Out-Null
    Check ((Get-FileHash -LiteralPath $iniPath).Hash -eq $iniHash) 'update retains synchronized custom port and settings'
    [IO.File]::WriteAllText($scriptPath,'// Startup command removed by another mod')
    & "$projectRoot\tools\install.ps1" -GameDirectory $root -DllPath $update -Update | Out-Null
    $repaired=[IO.File]::ReadAllText($scriptPath)
    Check ($repaired.Contains('/insim 65535') -and $repaired.Contains('// Startup command removed by another mod')) 'manual update repairs missing InSim with existing Unicode INI port'
    Check ((Get-FileHash -LiteralPath $iniPath).Hash -eq $iniHash) 'InSim repair does not rewrite the INI'
    Uninstall $root
    Check (![IO.File]::ReadAllText($scriptPath).Contains('/insim 65535')) 'uninstall removes the repaired startup command'

    $root=New-Fixture 'custom-port-no-general-section'
    [IO.File]::WriteAllText("$root\data\script\autoexec.lfs",'/insim 12345')
    [IO.File]::WriteAllLines("$root\LfsPad.ini",@('[Input]','InSimPort=45678','DeadzonePercent=31'))
    Install $root
    $result=[IO.File]::ReadAllText("$root\LfsPad.ini")
    Check ($result -match '(?m)^\[General\]\r?\nInSimPort=12345\r?$') 'missing General section added for custom port'
    Check ($result -match '(?m)^\[Input\]\r?\nInSimPort=45678\r?\nDeadzonePercent=31\r?$') 'keys in other sections untouched'

    $root=New-Fixture 'invalid-port'
    foreach ($command in @('/insim 0','/insim 65536','/insim 30000x','/insim 999999999999')) {
        [IO.File]::WriteAllText("$root\data\script\autoexec.lfs",$command)
        Reject { Install $root } "invalid startup command rejected: $command"
    }
    Check (!(Test-Path -LiteralPath "$root\addons")) 'invalid port preflight made no installation files'

    $root=New-Fixture 'unknown-build'
    [IO.File]::AppendAllText("$root\LFS.exe",'unknown-version-fixture')
    Reject { Install $root } 'unknown executable refused'
    Check (!(Test-Path -LiteralPath "$root\dxgi.dll")) 'unknown build made no DLL'
    $root=New-Fixture 'existing-dxgi'
    [IO.File]::WriteAllText("$root\dxgi.dll",'other-graphics-mod')
    Reject { Install $root } 'installer refuses to replace another DXGI mod'
    Check ([IO.File]::ReadAllText("$root\dxgi.dll") -eq 'other-graphics-mod') 'other DXGI mod preserved'

    $root=New-Fixture 'linked-directory'
    $outside=Join-Path $fixtureRoot 'outside'
    New-Item -ItemType Directory -Path $outside | Out-Null
    New-Item -ItemType Junction -Path "$root\addons" -Target $outside | Out-Null
    try {
        Reject { Install $root } 'installer rejects junction through add-on directory'
        Check (!(Test-Path -LiteralPath "$outside\LfsPad")) 'nothing written through junction'
    } finally { [IO.Directory]::Delete("$root\addons") }

    $root=New-Fixture 'busy'
    $lock=[IO.File]::Open("$root\.LfsPad-operation.lock",[IO.FileMode]::CreateNew,
        [IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try { Reject { Install $root } 'parallel installer cannot enter the same folder' }
    finally { $lock.Dispose() }

    $root=New-Fixture 'invalid-dll'
    Reject { & "$projectRoot\tools\install.ps1" -GameDirectory $root -DllPath $VerifiedGameExe } 'EXE cannot be installed as a DLL'
    Check (!(Test-Path -LiteralPath "$root\addons")) 'invalid DLL preflight made no backup'

    $root=New-Fixture 'legacy-script-encoding'
    $legacy="$root\data\script\autoexec.lfs"
    $legacyPrefix=[Text.Encoding]::GetEncoding(1254).GetBytes("// "+[char]0x131+[char]0x15f+"`r`n")
    [IO.File]::WriteAllBytes($legacy,$legacyPrefix)
    Install $root
    Uninstall $root
    $after=[IO.File]::ReadAllBytes($legacy)
    Check ([Convert]::ToBase64String($after,0,$legacyPrefix.Length) -eq
        [Convert]::ToBase64String($legacyPrefix)) 'autoexec retains original legacy encoded bytes'
    Write-Output "$script:checks installation checks passed"
} finally {
    # Resolve and validate the entire recursive deletion target before use.
    $resolvedFixture=[IO.Path]::GetFullPath($fixtureRoot)
    if (!$resolvedFixture.StartsWith($fixtureParent+'\install-tests-',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Unexpected test fixture location; cleanup refused.'
    }
    if (Test-Path -LiteralPath $resolvedFixture) { Remove-Item -LiteralPath $resolvedFixture -Recurse -Force }
}
