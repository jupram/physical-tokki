# Speaker gestures

Speaker contributions in this folder are notification sounds, tones, or
preinstalled spoken-word WAV assets. Arbitrary text-to-speech is not part of
the embedded firmware contract.

`speaker.drink_water` plays a fixed offline-generated phrase. `speaker.chirp`
is explicitly a synthesized bird-like sound, not a field recording.
`speaker.alert` is a short neutral tone. Parameterized greetings remain deferred.

`speaker.dog_bark` plays a **user-provided dog-bark recording twice**.
The embedded `audio/dog_bark.wav` already contains two identical copies of
the entire source clip, with no extra gap between copies. Existing pauses
inside the recording are preserved. Its 8,318 mono frames at 16 kHz last
0.519875 seconds; the usual 250 ms lead-in and trailing silence make the
gesture approximately 1.02 seconds. It uses the same PCM playback and 20%
scaling as speech, not the removed bark synthesizer.

To audition with the firmware's volume scaling and outer silence before
flashing, run from the repo root:

```powershell
.\tests\host\run.ps1 -BarkPreviewPath "$env:USERPROFILE\Downloads\Tokki_Dog_Bark.wav"
```

This exports the embedded 16 kHz mono PCM16 recording, already
scaled below the 20% ceiling. It does not normalize or boost the output.

## Dog-bark asset provenance

The user supplied `Tokki_Dog_Bark_v3.wav` on 2026-09-16 and requested two
repetitions for the gesture. Original format: 44.1 kHz, stereo PCM16,
11,462 frames. Original SHA-256:
`b230ad8dcd8620bf2fcb2e6e5c9bf73b88c8f63bb93f14c10c1a1026157bef09`.
The embedded metadata names **freewavesamples.com**, **Dog Bark**, and **2016**,
matching the [Dog Bark listing](https://freewavesamples.com/dog-bark).
The listing does not establish a CC0 license. This user-supplied asset is
**not asserted to be CC0**. On 2026-09-16, the repository owner explicitly
confirmed permission to include the recording in this repository and its
firmware, and authorized pushing it to GitHub. This confirmation is not an
independent verification of the source site's license or a CC0 dedication.

Reproduce the asset using the unmodified user-supplied original:

```powershell
node .\components\tokki_speaker\audio\generate-dog-bark.mjs `
  "$env:USERPROFILE\Downloads\Tokki_Dog_Bark_v3.wav" `
  .\components\tokki_speaker\audio\dog_bark.wav
```

The generator averages stereo channels, uses a windowed-sinc low-pass
resampler to 16 kHz, and concatenates two byte-identical copies. It does not
trim, normalize, pre-scale the amplitude, or overwrite the original. Firmware
applies the 20% scaling once during playback.

`speaker.chime` adds two ascending fixed notes (784/1047 Hz, 160/240 ms), with
a 60 ms gap. `speaker.ping` is a single 1320 Hz, 100 ms tone. Both also use the
existing 250 ms lead-in and trailing silence. No new audio file is needed.
Fixed sound sequences are defined in `speaker_sound_steps`; frequency sweeps
and sample generation are shared by the I2S player and host preview export.
Fade-in/out and the 20% ceiling are retained;
all actions are serialized and release their I2S channel on return.

## Speech asset provenance

`components/tokki_speaker/audio/drink_water.wav` was generated locally on
2026-09-15 using Windows SAPI and Microsoft David Desktop (English US), speaking
the original two-word text `Drink water` at rate -1. No downloaded recording,
external TTS service, voice imitation or personal data is used. Format is PCM,
16 kHz, mono, signed 16-bit; the generated phrase is 1.73 seconds long.

The reproducible source is `audio/generate-drink-water.ps1` in `tokki_speaker`.
It uses the machine's installed default SAPI voice, so output can differ by
machine. Review the voice choice and applicable voice distribution terms before
shipping beyond the hack prototype. Firmware scales PCM to the existing 20%
ceiling; the source WAV itself is not pre-scaled. Do not raise the ceiling to
compensate for a quiet voice without hardware review.