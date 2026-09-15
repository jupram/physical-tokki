$ErrorActionPreference = 'Stop'
$voice = New-Object -ComObject SAPI.SpVoice
$stream = New-Object -ComObject SAPI.SpFileStream
try {
    $stream.Format.Type = 18
    $stream.Open((Join-Path $PSScriptRoot 'drink_water.wav'), 3, $false)
    $voice.AudioOutputStream = $stream
    $voice.Rate = -1
    $voice.Volume = 100
    $voice.Speak('Drink water', 0) | Out-Null
    Write-Output "Generated offline speech with $($voice.Voice.GetDescription()); 16 kHz, mono, signed 16-bit PCM."
} finally {
    $stream.Close()
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($stream)
    [void] [Runtime.InteropServices.Marshal]::ReleaseComObject($voice)
}