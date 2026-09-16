param([string] $PreviewPath)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
if ($PreviewPath) { $PreviewPath = [System.IO.Path]::GetFullPath($PreviewPath) }

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

$build = Join-Path ([System.IO.Path]::GetTempPath()) "tokki-host-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $build | Out-Null
$sources = @(
    "$PSScriptRoot/test_gestures.c"
    "$repo/components/tokki_gestures/tokki_gestures.c"
    "$repo/components/tokki_gestures/src/led/led_actions.c"
    "$repo/components/tokki_gestures/src/neopixel/neopixel_actions.c"
    "$repo/components/tokki_gestures/src/oled/oled_actions.c"
    "$repo/components/tokki_oled/pet_eyes.c"
    "$repo/components/tokki_oled/oled_art.c"
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
        & ./tokki-host.exe $PreviewPath
    } else {
        & ./tokki-host.exe
    }
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }
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
Test-SpeakerWav "$repo/components/tokki_speaker/audio/dog_bark.wav" -expectedSamples 8000