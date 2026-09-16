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

The upstream `speaker.dog_bark` ID and `dog_bark.wav` file remain unchanged.
The additional `speaker.bark` ID uses `dog_bark_cc0.wav`, a different single-bark
CC0 recording documented below. Do not regenerate one asset with the other's
script or apply the CC0 statement to the upstream recording.

`speaker.chime` adds two ascending fixed notes (784/1047 Hz, 160/240 ms), with
a 60 ms gap. `speaker.ping` is a single 1320 Hz, 100 ms tone. Both also use the
existing 250 ms lead-in and trailing silence. No new audio file is needed.
Fixed sound sequences are defined in `speaker_sound_steps`; frequency sweeps
and sample generation are shared by the I2S player and host preview export.
Fade-in/out and the 20% ceiling are retained;
all actions are serialized and release their I2S channel on return.

## Short synthetic additions

| Action | Audible duration | Total with silence | Recipe / intended use |
| --- | --- | --- | --- |
| `speaker.bubble` | 90 ms | 590 ms | 1200 to 480 Hz falling sweep; tiny acknowledgement |
| `speaker.whistle` | 280 ms | 780 ms | 900 to 2100 Hz rising sweep; cheerful attention |
| `speaker.sigh` | 360 ms | 860 ms | 850 to 350 Hz falling sweep; relaxed or sleepy cue |
| `speaker.boing` | 240 ms | 740 ms | 350 to 900 Hz up, then 900 to 500 Hz down; playful bounce |
| `speaker.question` | 270 ms | 820 ms | 700 Hz note, 50 ms gap, 950 to 1250 Hz rise; questioning inflection |
| `speaker.downstep` | 320 ms | 860 ms | 740 then 494 Hz, 40 ms gap; gentle negative feedback |
| `speaker.sparkle` | 270 ms | 830 ms | 1047/1319/1568 Hz, 30 ms gaps; small celebration |
| `speaker.trill` | 210 ms | 760 ms | Three 1400 to 1700 Hz chirrups, 25 ms gaps; lively attention |
| `speaker.knock` | 130 ms | 730 ms | Two 500 to 200 Hz taps, 100 ms gap; message cue |
| `speaker.sonar` | 240 ms | 850 ms | Two 880 Hz pings, 110 ms gap, second at 40% gain; reminder cue |

These names describe stylized effects, not realistic recordings. Values are
original parameter choices using the existing sine table and 25 ms edge fades.
No new WAV, external sample, TTS, random playback or dependency is needed for
these ten synthetic effects. All stay under a 1.5-second total-duration limit, including the existing
250 ms silence before and after the sound. Short pops are intentionally not
stretched to fill a second. The existing Drink water speech is unchanged.

Descending sweeps use main's shared signed interpolation in `speaker_step_frequency`
to avoid unsigned subtraction wrapping to a high pitch. Firmware and exported
PCM use the same helper. Tests cover frequency bounds, monotonic direction,
the unchanged ascending formula, action routing, total duration and sample
ceiling/fade endpoints. A zero-length sweep or an index at or beyond the end
returns the ending frequency, preserving the upstream helper contract.

### Research and design rationale (2026-09-16)

- [jsfxr](https://sfxr.me/) demonstrates small blip, jump and power-up effects
	built from oscillators, envelopes and pitch slides.
- [Bfxr](https://www.bfxr.net/) likewise exposes frequency slides and short
	attack/sustain/decay controls. These are design references, not copied code
	or downloaded presets. The existing synthesizer is sufficient here.
- [Apple audio guidance](https://developer.apple.com/design/human-interface-guidelines/playing-audio)
	emphasizes respecting silence/volume and not conveying important information
	through sound alone. For Tokki, the future PC rule should choose whether to
	play an optional sound and accompany important events with a visual cue.
	This change does not implement mute settings, scheduling or event bundles.

Ram reported testing the earlier chirp on hardware; these new effects have only
host-test evidence so far. Confirm recognizability and acceptable loudness on
the actual 0.5 W speaker. A 20% sample limit is not an electrical wattage limit.
Per-note gain is relative to the already limited waveform and cannot amplify
it: values above 100 are clamped. Existing sounds retain 100% relative gain;
the second sonar ping uses 40%. Host tests cover attenuation and clamp behavior.

## Dog bark recording and provenance

`speaker.bark` plays `components/tokki_speaker/audio/dog_bark_cc0.wav`: a 0.5-second
excerpt containing one bark, through the same scaled WAV player as speech.
The existing 250 ms lead-in/trailing silence makes the action about 1 second.
This is a real field recording with some outdoor background, not a synthetic
animal voice. Firmware applies the existing 20% gain once; the WAV is not
pre-scaled. This branch includes the bark following the clarified team request.

- Work: [Dog barking mono](https://opengameart.org/content/dog-barking-mono).
- Author: Brandon Morris; uploaded by HaelDB on 2011-03-27.
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/), verified
	from the source page's explicit license link on 2026-09-16. Attribution is
	retained for traceability even though CC0 does not require it.
- Original: https://opengameart.org/sites/default/files/dog_barking_mono.wav
- Original SHA-256: `BBD0F908B3514DD3BD7D2BC04DCF64F8D360A161E7F43CAC5D6761E7ADD79451`.
- Changes: extract 0.48-0.98 seconds (second bark), 5 ms fade-in, 10 ms fade-out,
	resample from 44.1 kHz mono to 16 kHz mono signed 16-bit PCM.
- Reproduce using `audio/generate-dog-bark.ps1` in `tokki_speaker` with FFmpeg.
	The script checks the source hash and writes only `dog_bark_cc0.wav`.
	The checked-in WAV means normal firmware
	builds require neither FFmpeg nor a network download.
- Initial conversion used FFmpeg from `@ffmpeg-installer/win32-x64@4.1.0` in a
	temporary directory, not a project dependency.

Host tests verify RIFF/PCM format, exactly 8000 samples (0.5 seconds),
non-silence and the scaled amplitude ceiling. Initial scaled peak: 2911/32768.
Hardware gain, perceived loudness and recognizability still need a lab check.

## Lab handoff

Build production and the standalone self-test in the team's ESP-IDF environment:

```powershell
idf.py build
idf.py -C self_test -B self_test/build build
```

Invoke the new `speaker.*` IDs serially through the team's integration or
`tokki_action_run(id)`. Start with `speaker.bark`; compare chirp/chime/ping with
the previous firmware, then check each new effect and the softer sonar echo.
Repeat playback to verify I2S channel cleanup. Stop if the speaker distorts or
gets warm. Host tests do not compile or validate the ESP-IDF I2S driver; no
physical test of this follow-up has been performed here.

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