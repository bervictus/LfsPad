$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if (!(Test-Path -LiteralPath "$projectRoot\build\dinput8.dll")) { throw 'Build the DLL first.' }
$versionLine=Select-String -LiteralPath "$projectRoot\build\CMakeCache.txt" -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(\d+\.\d+\.\d+)$'
if (!$versionLine) { throw 'CMake build version missing.' }
$version=$versionLine.Matches[0].Groups[1].Value
$packageRoot="$projectRoot\artifacts\LfsPad-$version"
$sourceRoot="$projectRoot\artifacts\LfsPad-$version-source"
$license="$projectRoot\.tools\imgui-1.91.9b\LICENSE.txt"
if (!(Test-Path -LiteralPath $license)) { throw 'Dear ImGui license missing.' }
$sdlLicense="$projectRoot\.tools\SDL3-3.4.20\LICENSE.txt"
$hidapiLicense="$projectRoot\.tools\SDL3-3.4.20\src\hidapi\LICENSE-bsd.txt"
New-Item -ItemType Directory -Path "$packageRoot\tools","$sourceRoot\src","$sourceRoot\tools\setup","$sourceRoot\tests" -Force | Out-Null
$documents=@('README.md','KONTROLLER.md','ARCHITECTURE.md','VALIDATION.md','LfsPad.ini')
foreach ($file in $documents) {
    Copy-Item -LiteralPath "$projectRoot\$file" -Destination $packageRoot -Force
    Copy-Item -LiteralPath "$projectRoot\$file" -Destination $sourceRoot -Force
}
Copy-Item -LiteralPath "$projectRoot\build\dinput8.dll" -Destination $packageRoot -Force
Copy-Item -LiteralPath "$projectRoot\tools\install.ps1","$projectRoot\tools\uninstall.ps1" -Destination "$packageRoot\tools" -Force
Copy-Item -LiteralPath $license -Destination "$packageRoot\Dear-ImGui-LICENSE.txt" -Force
Copy-Item -LiteralPath "$projectRoot\CMakeLists.txt","$projectRoot\.gitignore" -Destination $sourceRoot -Force
Get-ChildItem -LiteralPath "$projectRoot\src" -File | Copy-Item -Destination "$sourceRoot\src" -Force
Get-ChildItem -LiteralPath "$projectRoot\tools" -Filter '*.ps1' -File | Copy-Item -Destination "$sourceRoot\tools" -Force
Copy-Item -LiteralPath "$projectRoot\tools\setup\LfsPadSetup.cs" -Destination "$sourceRoot\tools\setup" -Force
Get-ChildItem -LiteralPath "$projectRoot\tests" -File | Copy-Item -Destination "$sourceRoot\tests" -Force
Copy-Item -LiteralPath $license -Destination "$sourceRoot\Dear-ImGui-LICENSE.txt" -Force
foreach ($root in @($packageRoot,$sourceRoot)) {
    Copy-Item -LiteralPath $sdlLicense -Destination "$root\SDL-LICENSE.txt" -Force
    Copy-Item -LiteralPath $hidapiLicense -Destination "$root\SDL-HIDAPI-LICENSE.txt" -Force
    $hashLines=Get-ChildItem -LiteralPath $root -File -Recurse |
        Where-Object Name -ne 'SHA256SUMS.txt' | Sort-Object FullName | ForEach-Object {
            $relative=$_.FullName.Substring($root.Length+1).Replace('\','/')
            (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()+'  '+$relative
        }
    $hashLines | Set-Content -LiteralPath "$root\SHA256SUMS.txt" -Encoding ascii
}
$binaryZip="$projectRoot\artifacts\LfsPad-$version-win32.zip"
$sourceZip="$projectRoot\artifacts\LfsPad-$version-source.zip"
Compress-Archive -LiteralPath $packageRoot -DestinationPath $binaryZip -Force
Compress-Archive -LiteralPath $sourceRoot -DestinationPath $sourceZip -Force
Get-FileHash -LiteralPath $binaryZip,$sourceZip | Select-Object Path,Hash
