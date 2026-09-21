param([string] $PreviewPath, [switch] $Runtime, [string] $WireFixturePath, [string] $BarkPreviewPath)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$barkAsset = Join-Path $repo 'components\tokki_speaker\audio\dog_bark.wav'
if ($PreviewPath) { $PreviewPath = [System.IO.Path]::GetFullPath($PreviewPath) }
if ($BarkPreviewPath) { $BarkPreviewPath = [System.IO.Path]::GetFullPath($BarkPreviewPath) }
if ($WireFixturePath) {
    $WireFixturePath = [System.IO.Path]::GetFullPath($WireFixturePath)
    $Runtime = $true
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw 'Install Visual Studio C++ tools or run from a native developer PowerShell.'
    }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'Visual Studio C++ tools were not found.' }
    Import-Module "$installation/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

$build = Join-Path $repo ".host-build-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $build | Out-Null
$sources = @(
    "$PSScriptRoot/test_gestures.c"
    "$repo/components/tokki_gestures/tokki_gestures.c"
    "$repo/components/tokki_gestures/src/led/led_actions.c"
    "$repo/components/tokki_gestures/src/neopixel/neopixel_actions.c"
    "$repo/components/tokki_neopixel/neopixel_effects.c"
    "$repo/components/tokki_gestures/src/oled/oled_actions.c"
    "$repo/components/tokki_oled/pet_eyes.c"
    "$repo/components/tokki_oled/oled_art.c"
    "$repo/components/tokki_oled/oled_scroll.c"
    "$repo/components/tokki_gestures/src/speaker/speaker_actions.c"
    "$repo/components/tokki_speaker/speaker_tone.c"
)
$includes = @(
    "/I$PSScriptRoot/include"
    "/I$repo/components/tokki_gestures/include"
    "/I$repo/components/tokki_gestures/src"
    "/I$repo/components/tokki_led/include"
    "/I$repo/components/tokki_neopixel/include"
    "/I$repo/components/tokki_oled/include"
    "/I$repo/components/tokki_speaker/include"
)
Push-Location $build
try {
    & cl.exe /nologo /TC /std:c11 /W4 /WX @includes @sources /Fetokki-host.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed.' }
    if ($PreviewPath) {
        & ./tokki-host.exe $barkAsset $PreviewPath
    } else {
        & ./tokki-host.exe $barkAsset
    }
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }
    if ($BarkPreviewPath) {
        & .\tokki-host.exe $barkAsset --bark-wav $BarkPreviewPath
        if ($LASTEXITCODE -ne 0) { throw 'Bark WAV export failed.' }
    }
    $boardInclude = "/I$repo\components\tokki_board\include"
    $runtimeInclude = "/I$repo\components\tokki_runtime\include"
    & cl.exe /nologo /TC /std:c11 /W4 /WX @includes $boardInclude "$PSScriptRoot\test_led.c" "$repo\components\tokki_led\tokki_led.c" /Fetokki-led.exe
    if ($LASTEXITCODE -ne 0) { throw 'LED host test compilation failed.' }
    & .\tokki-led.exe
    if ($LASTEXITCODE -ne 0) { throw 'LED latch tests failed.' }
    & .\tokki-led.exe --early-failure
    if ($LASTEXITCODE -ne 0) { throw 'LED initialization retry tests failed.' }
    & cl.exe /nologo /TC /std:c11 /W4 /WX @includes $boardInclude $runtimeInclude "$PSScriptRoot\test_startup.c" "$repo\main\app_main.c" /Fetokki-startup.exe
    if ($LASTEXITCODE -ne 0) { throw 'Startup host test compilation failed.' }
    & .\tokki-startup.exe
    if ($LASTEXITCODE -ne 0) { throw 'Startup failure indicator tests failed.' }
    if ($Runtime) {
        $workerSources = @(
            "$PSScriptRoot\test_workers.c"
            "$repo\components\tokki_runtime\tokki_runtime.c"
            "$repo\components\tokki_runtime\tokki_idle.c"
            "$repo\components\tokki_neopixel\neopixel_effects.c"
            "$repo\components\tokki_oled\pet_eyes.c"
            "$repo\components\tokki_oled\oled_art.c"
            "$repo\components\tokki_oled\oled_scroll.c"
        )
        & cl.exe /nologo /TC /std:c11 /W4 /WX @includes $runtimeInclude @workerSources /Fetokki-workers.exe
        if ($LASTEXITCODE -ne 0) { throw 'Worker host test compilation failed.' }
        & .\tokki-workers.exe
        if ($LASTEXITCODE -ne 0) { throw 'Worker idle/preemption tests failed.' }
        $cjson = Join-Path $repo 'managed_components\espressif__cjson\cJSON'
        if (-not (Test-Path (Join-Path $cjson 'cJSON.c'))) {
            throw 'Build production firmware once with ESP-IDF to restore the managed cJSON dependency, then rerun -Runtime.'
        }
        & cl.exe /nologo /TC /std:c11 /W3 "/I$cjson" "$cjson\cJSON.c" /c /Focjson.obj
        if ($LASTEXITCODE -ne 0) { throw 'cJSON host compilation failed.' }
        $runtimeSources = @(
            "$PSScriptRoot\test_runtime.c"
            "$repo\components\tokki_runtime\tokki_protocol.c"
            "$repo\components\tokki_runtime\tokki_idle.c"
        ) + $sources[1..($sources.Length - 1)]
        & cl.exe /nologo /TC /std:c11 /W4 /WX @includes "/I$cjson" "/I$repo\components\tokki_runtime\include" @runtimeSources /Fetokki-runtime.exe /link cjson.obj
        if ($LASTEXITCODE -ne 0) { throw 'Runtime host test compilation failed.' }
        & .\tokki-runtime.exe
        if ($LASTEXITCODE -ne 0) { throw 'Runtime host tests failed.' }
        if ($WireFixturePath) {
            $frames = & .\tokki-runtime.exe --wire-fixture
            if ($LASTEXITCODE -ne 0) { throw 'Protocol fixture export failed.' }
            [System.IO.File]::WriteAllLines($WireFixturePath, [string[]] $frames, [System.Text.UTF8Encoding]::new($false))
        }
    }
} finally {
    Pop-Location
    Remove-Item $build -Recurse -Force
}

function Test-SpeakerWav {
param([string] $audioPath, [int] $expectedSamples = 0)

$reader = [System.IO.BinaryReader]::new([System.IO.File]::OpenRead($audioPath))
try {
    $ascii = [System.Text.Encoding]::ASCII
    if ($ascii.GetString($reader.ReadBytes(4)) -ne 'RIFF') { throw 'Not a RIFF WAV.' }
    $riffSize = $reader.ReadUInt32()
    if ($riffSize + 8 -ne $reader.BaseStream.Length) { throw 'Invalid RIFF size.' }
    if ($ascii.GetString($reader.ReadBytes(4)) -ne 'WAVE') { throw 'Not WAVE audio.' }
    $validFormat = $false
    $sampleCount = 0
    $peak = 0
    while ($reader.BaseStream.Position + 8 -le $reader.BaseStream.Length) {
        $chunk = $ascii.GetString($reader.ReadBytes(4))
        $size = $reader.ReadUInt32()
        $next = $reader.BaseStream.Position + $size + ($size % 2)
        if ($next -gt $reader.BaseStream.Length) { throw 'Truncated WAV chunk.' }
        if ($chunk -eq 'fmt ') {
            if ($size -lt 16) { throw 'Short PCM format.' }
            $format = $reader.ReadUInt16()
            $channels = $reader.ReadUInt16()
            $rate = $reader.ReadUInt32()
            $byteRate = $reader.ReadUInt32()
            $alignment = $reader.ReadUInt16()
            $bits = $reader.ReadUInt16()
            $validFormat = $format -eq 1 -and $channels -eq 1 -and $rate -eq 16000 -and $bits -eq 16 -and $byteRate -eq 32000 -and $alignment -eq 2
        } elseif ($chunk -eq 'data') {
            if (-not $validFormat -or $size -eq 0 -or $size % 2 -ne 0) { throw 'Invalid PCM data.' }
            $sampleCount = $size / 2
            for ($sample = 0; $sample -lt $sampleCount; $sample++) {
                $value = [int] $reader.ReadInt16()
                $peak = [Math]::Max($peak, [Math]::Abs($value))
                if ([Math]::Abs([Math]::Truncate($value * 20 / 100)) -gt 6553) { throw 'Scaled speech exceeds amplitude ceiling.' }
            }
        }
        $reader.BaseStream.Position = $next
    }
    if ($sampleCount -lt 1600 -or $sampleCount -gt 80000 -or $peak -eq 0) { throw 'Audio must be non-silent and between 0.1 and 5 seconds.' }
    if ($expectedSamples -gt 0 -and $sampleCount -ne $expectedSamples) { throw 'Unexpected sample count.' }
    if ($expectedSamples -gt 0 -and $sampleCount / 16000 + 0.5 -gt 1.5) { throw 'Sound plus padding exceeds 1.5 seconds.' }
    Write-Output "PASS: $([IO.Path]::GetFileName($audioPath)), 16 kHz mono PCM16, $($sampleCount / 16000) seconds, scaled peak $([Math]::Truncate($peak * 0.2))/32768"
} finally {
    $reader.Dispose()
}
}

Test-SpeakerWav "$repo/components/tokki_speaker/audio/drink_water.wav"
Test-SpeakerWav "$repo/components/tokki_speaker/audio/dog_bark.wav" -expectedSamples 8318