# Speaker gestures

Speaker contributions in this folder are notification sounds, tones, or
preinstalled spoken-word WAV assets. Arbitrary text-to-speech is not part of
the embedded firmware contract.

`speaker.drink_water` plays a fixed offline-generated phrase. `speaker.chirp`
is explicitly a synthesized bird-like sound, not a field recording.
`speaker.alert` is a short neutral tone. A bark and parameterized greetings are
deferred rather than approximated under misleading action IDs.

`speaker.chime` adds two ascending fixed notes (784/1047 Hz, 160/240 ms), with
a 60 ms gap. `speaker.ping` is a single 1320 Hz, 100 ms tone. Both also use the
existing 250 ms lead-in and trailing silence. No new audio file is needed.
Fixed tone sequences are defined in `speaker_sound_steps` and reused by the
I2S player and host preview export. Fade-in/out and the 20% ceiling are retained;
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