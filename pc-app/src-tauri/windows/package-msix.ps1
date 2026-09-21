param(
    [string] $Publisher = 'CN=Physical Tokki Development',
    [string] $Version = '0.1.0.5',
    [string] $CertificateThumbprint
)

$ErrorActionPreference = 'Stop'
$pcApp = (Resolve-Path "$PSScriptRoot\..\..").Path
$tauri = Join-Path $pcApp 'src-tauri'
$architecture = switch ($env:PROCESSOR_ARCHITECTURE) {
    'ARM64' { 'arm64' }
    'AMD64' { 'x64' }
    'x86' { 'x86' }
    default { throw "Unsupported package architecture: $env:PROCESSOR_ARCHITECTURE" }
}

Push-Location $pcApp
try {
    & npm.cmd run tauri build -- --no-bundle
    if ($LASTEXITCODE -ne 0) { throw 'Tauri release build failed.' }
} finally {
    Pop-Location
}

$layout = Join-Path $tauri 'target\msix-layout'
$output = Join-Path $tauri "target\release\bundle\msix\Physical-Tokki-$Version-$architecture.msix"
Remove-Item $layout -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $layout 'Assets') -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
Copy-Item (Join-Path $tauri 'target\release\pc-app.exe') $layout
Copy-Item (Join-Path $tauri 'icons\StoreLogo.png') (Join-Path $layout 'Assets')
Copy-Item (Join-Path $tauri 'icons\Square44x44Logo.png') (Join-Path $layout 'Assets')
Copy-Item (Join-Path $tauri 'icons\Square150x150Logo.png') (Join-Path $layout 'Assets')

$template = Get-Content (Join-Path $PSScriptRoot 'AppxManifest.template.xml') -Raw
$manifest = $template.Replace('__PUBLISHER__', $Publisher).Replace('__VERSION__', $Version).Replace('__ARCH__', $architecture)
[System.IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, [System.Text.UTF8Encoding]::new($false))

$sdkBin = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*" -Directory |
    Where-Object Name -Match '^10\.\d+\.\d+\.\d+$' |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $sdkBin) { throw 'Windows 10/11 SDK tools were not found.' }
$toolArchitecture = @($architecture, 'x64', 'x86') | Select-Object -Unique | Where-Object {
    Test-Path (Join-Path $sdkBin.FullName "$_\makeappx.exe")
} | Select-Object -First 1
if (-not $toolArchitecture) { throw "MakeAppx was not found under $($sdkBin.FullName)" }
$makeAppx = Join-Path $sdkBin.FullName "$toolArchitecture\makeappx.exe"
$signTool = Join-Path $sdkBin.FullName "$toolArchitecture\signtool.exe"

& $makeAppx pack /d $layout /p $output /o
if ($LASTEXITCODE -ne 0) { throw 'MSIX packaging failed.' }
if ($CertificateThumbprint) {
    & $signTool sign /sha1 $CertificateThumbprint /fd SHA256 $output
    if ($LASTEXITCODE -ne 0) { throw 'MSIX signing failed.' }
} else {
    Write-Warning 'The MSIX is unsigned. Sign it with a certificate whose subject exactly matches the Publisher value before installing.'
}
Write-Output $output
