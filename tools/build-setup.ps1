$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$buildRoot=Join-Path $projectRoot 'build'
$versionLine=Select-String -LiteralPath "$buildRoot\CMakeCache.txt" -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(\d+\.\d+\.\d+)$'
if (!$versionLine) { throw 'Run tools/build.ps1 before building the installer.' }
$version=$versionLine.Matches[0].Groups[1].Value
$artifactRoot=Join-Path $projectRoot 'artifacts'
$payloadRoot=Join-Path $artifactRoot "LfsPad-$version"
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework C# compiler is missing.' }

$resources=@(
    @{File='dinput8.dll'; Name='dinput8.dll'},
    @{File='LfsPad.ini'; Name='LfsPad.ini'},
    @{File='tools\install.ps1'; Name='install.ps1'},
    @{File='tools\uninstall.ps1'; Name='uninstall.ps1'},
    @{File='Dear-ImGui-LICENSE.txt'; Name='Dear-ImGui-LICENSE.txt'}
)
$resourceFlags=foreach ($resource in $resources) {
    $path=Join-Path $payloadRoot $resource.File
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing package file: $path" }
    "/resource:$path,$($resource.Name)"
}
$dllHash=(Get-FileHash -LiteralPath "$payloadRoot\dinput8.dll").Hash
if ($dllHash -ne (Get-FileHash -LiteralPath "$buildRoot\dinput8.dll").Hash) {
    throw 'The package DLL differs from the current build. Run tools/package.ps1 first.'
}

$versionSource=Join-Path $buildRoot 'LfsPadSetupVersion.cs'
@"
using System.Reflection;
[assembly: AssemblyTitle("LfsPad Setup")]
[assembly: AssemblyProduct("LfsPad")]
[assembly: AssemblyVersion("$version.0")]
[assembly: AssemblyFileVersion("$version.0")]
[assembly: AssemblyInformationalVersion("$version")]
"@ | Set-Content -LiteralPath $versionSource -Encoding ascii
$output=Join-Path $artifactRoot "LfsPad-$version-setup.exe"
& $compiler /nologo /target:winexe /optimize+ /platform:anycpu /codepage:65001 `
    /reference:System.Drawing.dll /reference:System.Windows.Forms.dll "/out:$output" `
    @resourceFlags "$PSScriptRoot\setup\LfsPadSetup.cs" $versionSource
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
Get-FileHash -LiteralPath $output
