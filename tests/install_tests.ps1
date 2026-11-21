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
    $root=New-Fixture 'existing-chain'
    [IO.File]::WriteAllText("$root\dinput8.dll",'original-loader-fixture')
    [IO.File]::WriteAllText("$root\LfsPad.ini","[Input]`r`nDeadzonePercent=31")
    [IO.File]::WriteAllText("$root\data\script\autoexec.lfs","// existing`r`n/insim 29999`r`n")
    $originalHash=(Get-FileHash -LiteralPath "$root\dinput8.dll").Hash
    $scriptHash=(Get-FileHash -LiteralPath "$root\data\script\autoexec.lfs").Hash
    Install $root
    Check ((Get-FileHash -LiteralPath "$root\LfsPad.previous.dll").Hash -eq $originalHash) 'loader chain preserved'
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq (Get-FileHash -LiteralPath "$projectRoot\build\dinput8.dll").Hash) 'installed build matches'
    Check ((Get-FileHash -LiteralPath "$root\data\script\autoexec.lfs").Hash -eq $scriptHash) 'existing InSim script unchanged'
    Check ([IO.File]::ReadAllText("$root\LfsPad.ini").Contains('31')) 'existing settings retained'
    $worker="$root\addons\LfsPad\update\update.ps1"
    Check ((Get-FileHash -LiteralPath $worker).Hash -eq
        (Get-FileHash -LiteralPath "$projectRoot\tools\update.ps1").Hash) 'updater installed'
    Reject { Install $root } 'double installation refused'
    [IO.File]::AppendAllText("$root\dinput8.dll",'modified')
    $modifiedHash=(Get-FileHash -LiteralPath "$root\dinput8.dll").Hash
    Reject { Uninstall $root } 'changed installed DLL protected'
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq $modifiedHash) 'rejected uninstall did not change DLL'
    Copy-Item -LiteralPath "$projectRoot\build\dinput8.dll" -Destination "$root\dinput8.dll" -Force
    $update=Join-Path $fixtureRoot 'update.dll'
    Copy-Item -LiteralPath "$projectRoot\build\dinput8.dll" -Destination $update
    [IO.File]::AppendAllText($update,'updated-proxy-fixture')
    $currentHash=(Get-FileHash -LiteralPath "$root\dinput8.dll").Hash
    $updateHash=(Get-FileHash -LiteralPath $update).Hash
    Reject {
        & "$root\addons\LfsPad\update\install.ps1" -GameDirectory $root -DllPath $update -Update `
            -ExpectedCurrentHash ('0'*64) -ExpectedNewHash $updateHash
    } 'automatic update rejects stale installed hash'
    Reject {
        & "$root\addons\LfsPad\update\install.ps1" -GameDirectory $root -DllPath $update -Update `
            -ExpectedCurrentHash $currentHash -ExpectedNewHash ('0'*64)
    } 'automatic update rejects unexpected download hash'
    $iniBefore=[IO.File]::ReadAllBytes("$root\LfsPad.ini")
    [IO.File]::AppendAllText("$root\LfsPad.ini","`r`n[General]`r`nAutoUpdate=0")
    Reject {
        & "$root\addons\LfsPad\update\install.ps1" -GameDirectory $root -DllPath $update -Update `
            -ExpectedCurrentHash $currentHash -ExpectedNewHash $updateHash
    } 'automatic update respects new opt-out'
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq $currentHash) 'rejected automatic updates leave DLL intact'
    [IO.File]::WriteAllBytes("$root\LfsPad.ini",$iniBefore)
    $iniHash=(Get-FileHash -LiteralPath "$root\LfsPad.ini").Hash
    & "$root\addons\LfsPad\update\install.ps1" -GameDirectory $root -DllPath $update -Update `
        -ExpectedCurrentHash $currentHash -ExpectedNewHash $updateHash | Out-Null
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq (Get-FileHash -LiteralPath $update).Hash) 'update installed'
    Check ((Get-FileHash -LiteralPath "$root\LfsPad.ini").Hash -eq $iniHash) 'update preserves settings'
    Check ((Get-FileHash -LiteralPath "$root\LfsPad.previous.dll").Hash -eq $originalHash) 'update preserves original chain'
    # A co-installed launcher can restore the original proxy. The update
    # accepts it only when the manifest, chain and backup still agree.
    Copy-Item -LiteralPath "$root\LfsPad.previous.dll" -Destination "$root\dinput8.dll" -Force
    & "$projectRoot\tools\install.ps1" -GameDirectory $root -DllPath $update -Update | Out-Null
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq (Get-FileHash -LiteralPath $update).Hash) 'update recovers an exactly restored original loader'
    $manifestPath="$root\addons\LfsPad\installation.json"
    $record=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $original="$($record.BackupDirectory)\dinput8.dll"
    [IO.File]::Move("$root\LfsPad.previous.dll","$fixtureRoot\saved-chain.dll")
    [IO.File]::WriteAllText($original,'modified-backup')
    Reject { Uninstall $root } 'missing chain and tampered backup refused'
    [IO.File]::Move("$fixtureRoot\saved-chain.dll","$root\LfsPad.previous.dll")
    Reject { Uninstall $root } 'changed backup refused even when chain matches'
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq (Get-FileHash -LiteralPath $update).Hash) 'backup rejection preserved installed DLL'
    Copy-Item -LiteralPath "$root\LfsPad.previous.dll" -Destination $original -Force
    [IO.File]::AppendAllText("$root\LfsPad.ini","`r`n; edited after upgrade")
    Uninstall $root
    Check ((Get-FileHash -LiteralPath "$root\dinput8.dll").Hash -eq $originalHash) 'original loader restored exactly'
    Check (!(Test-Path -LiteralPath "$root\LfsPad.previous.dll")) 'owned chain file removed'
    Check (!(Test-Path -LiteralPath "$root\addons\LfsPad\installation.json")) 'active manifest removed'
    Check ([IO.File]::ReadAllText("$root\LfsPad.ini").Contains('edited after upgrade')) 'pre-existing config keeps later edits'

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
    Check (!(Test-Path -LiteralPath "$root\dinput8.dll")) 'no original proxy leaves no DLL'

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
    Check (!(Test-Path -LiteralPath "$root\dinput8.dll")) 'unknown build made no DLL'
    $root=New-Fixture 'unexpected-chain'
    Install $root
    [IO.File]::WriteAllText("$root\LfsPad.previous.dll",'other-plugin')
    Reject { Uninstall $root } 'unexpected chained DLL refused'
    Check ([IO.File]::Exists("$root\LfsPad.previous.dll")) 'unexpected chained DLL preserved'

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
