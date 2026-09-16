param(
    [string] $FfmpegPath = 'ffmpeg',
    [string] $SourcePath
)

$ErrorActionPreference = 'Stop'
$downloaded = -not $SourcePath
if ($downloaded) {
    $SourcePath = Join-Path ([IO.Path]::GetTempPath()) "tokki-bark-$([guid]::NewGuid()).wav"
}
try {
    if ($downloaded) {
        Invoke-WebRequest -Uri 'https://opengameart.org/sites/default/files/dog_barking_mono.wav' -OutFile $SourcePath
    }
    $hash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash
    if ($hash -ne 'BBD0F908B3514DD3BD7D2BC04DCF64F8D360A161E7F43CAC5D6761E7ADD79451') {
        throw 'The source recording differs from the reviewed CC0 asset.'
    }
    $output = Join-Path $PSScriptRoot 'dog_bark.wav'
    & $FfmpegPath -hide_banner -nostdin -y -i $SourcePath -af 'atrim=start=0.48:end=0.98,asetpts=PTS-STARTPTS,afade=t=in:st=0:d=0.005,afade=t=out:st=0.49:d=0.01' -ar 16000 -ac 1 -c:a pcm_s16le -map_metadata -1 $output
    if ($LASTEXITCODE -ne 0) { throw 'Bark conversion failed.' }
    Write-Output 'Prepared one 0.5-second bark excerpt, mono PCM16 at 16 kHz; firmware applies the existing 20% gain.'
} finally {
    if ($downloaded -and (Test-Path -LiteralPath $SourcePath)) {
        Remove-Item -LiteralPath $SourcePath
    }
}