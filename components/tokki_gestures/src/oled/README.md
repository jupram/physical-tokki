# OLED gestures

This contribution extends the original eyes-only scope to the requested fixed
reminder message and monochrome art. Owner review of that scope is required.

Actions are finite wrappers over `tokki_oled` and `pet_eyes`, not independent
display drivers. Happy, sad, curious and surprised eyes run for 48 frames;
blink runs only the nine-frame close/reopen section. Drink water is a centered
two-line message. Water drop and fire each run for 24 frames. Every action
retains its final frame and returns display errors immediately.

The follow-up primitives are wink, checkmark, thinking dots, four directional
glances, sleepy eyes, heart, exclamation, lovey-dovey, shy and sleeping eyes.
These transient actions restore the centered happy-eye frame before returning.
Wink keeps the other eye open;
thinking dots advance once without an endless loop. No expression implies
sensor tracking, listening, or real task progress. See the parent catalog for
exact nominal durations and successful final states.

## Sky scenes

- `oled.night_sky`: 48 frames at 90 ms (4.32 seconds). Ten stars slowly
  twinkle out of phase without disappearing, around a fixed crescent moon and
  low hill silhouettes. A short shooting star crosses once at frames 20-31.
- `oled.sunrise`: 64 frames at 90 ms (5.76 seconds). A filled sun eases upward
  over the first 36 frames, clipped below the horizon. Seven rays gradually
  extend at frames 18-42; the completed scene holds through frame 62.
  Two simple horizontal reflections complete the scene without fine detail.

Both use deterministic integer-only, monochrome rendering on the 128x64
framebuffer. The final frame restores `PET_EYES_HAPPY, 0`; draw errors stop
the action immediately, including a failed final restoration. Existing art
enum values and action registry positions are preserved by appending the new
scenes. Both are manually selectable. In the nine-choice idle shuffle, night
sky only plays immediately after sleeping eyes, at 60 ms per frame
(2.88 seconds). It is not an independent idle choice; sunrise remains manual.
Neither automatically triggers LEDs or audio.

Desktop local previews use the same pixel geometry and 90 ms frame timing,
with static representative frames when reduced motion is requested. They are
available in the OLED preview picker and bundle palette. Refresh device
discovery after flashing to see both new IDs in the hardware catalog.

## Sleeping eyes

`oled.sleeping` is distinct from the half-lidded `oled.sleepy` expression.
It runs for 48 frames at 90 ms (4.32 seconds): open eyes ease shut by frame 8,
hold as relaxed crescents with a small breathing bob through frame 39, then
reopen by frame 46. Three simple Z marks drift up and right above the eyes
while closed. There are no eyebrows, sounds or light effects.

Sleeping eyes followed by night sky form a single sequence alongside the
existing eight idle choices. The pair plays once per shuffled round, with no
repeated choice at a round boundary. Both portions last 48 frames; idle holds
the last closed sleeping pose (frame 39) through frames 40-47 rather than
reopening before the sky. No rest or other expression intervenes. The sky's
final frame restores open eyes before a 0.6-1.2-second pause.
The manual sleeping action still reopens as described above. Idle renders one frame per
worker step, so an incoming OLED command takes priority after the current
hardware write instead of waiting for the sleep or sky sequence to finish.
The independent, intermittent NeoPixel breathing remains unchanged.

## Eye language and frame contract

The 128x64 renderer uses filled oval whites and pupils, clipped lids, and bold
closed crescent curves. There are no eyebrows or falling tears. All motion is
integer-only, with eased keyframes and small squash/stretch or pupil overshoots.
Existing enum values, action IDs, frame counts, and the 90 ms gesture frame
delay are unchanged. `PET_EYES_LOVEY_DOVEY`, `PET_EYES_SHY` and
`PET_EYES_SLEEPING` are appended to the enum and exposed as
`oled.lovey_dovey`, `oled.shy` and `oled.sleeping`. All three actions
run for 48 frames (4.32 seconds), then restore `PET_EYES_HAPPY, 0`.

| Expression | Shape and motion |
| --- | --- |
| `PET_EYES_HAPPY` | A -> B -> F: tall neutral ovals squash into upward-arched smiling crescents, hold at frames 7-22, then reopen by 29. |
| `PET_EYES_SAD` | Shorter ovals, low/inward pupils, and sloped upper lids with drooping outer corners; no extra symbols. |
| `PET_EYES_CURIOUS` | D + B: a taller eye paired with a smaller, partly lidded eye; gentle elastic stretch and shared pupil drift with a two-frame tracking lag. |
| `PET_EYES_SURPRISED` | A -> E: vertical opening and rapidly shrinking solid pin pupils, peak stretch at frame 4, settled by 7; no blink interrupts the surprised hold. |
| `PET_EYES_WINK` | A + F: the left eye stays open while the right follows the crescent blink curve at frames 34-42. |
| `PET_EYES_LOOK_LEFT`, `PET_EYES_LOOK_RIGHT` | A: stable tall whites; pupils ease sideways, overshoot at frame 4, settle by 7, hold through 15, and return by 22. |
| `PET_EYES_LOOK_UP`, `PET_EYES_LOOK_DOWN` | Same glance timing with smaller vertical travel to keep the pupils inside the whites. |
| `PET_EYES_SLEEPY` | Slowly descending upper lids and slightly squashed whites; narrowest at frames 13-17, reopened by 22. |
| `PET_EYES_LOVEY_DOVEY` | Heart-shaped pupils pulse at frames 6 and 26 with a little vertical stretch; a crescent smile at 38-41 hides the transition back to ordinary pupils. |
| `PET_EYES_SHY` | Lids lower unevenly over an inward/downward gaze, with a softly raised lower lid; peeks at 23-28, retreats again, then reopens by 46. No blush marks or eyebrows. |
| `PET_EYES_SLEEPING` | Soft closure into lower-set crescents at frames 8-39, a gentle vertical bob and floating Zzz, then reopens by 46. |

`PET_EYES_HAPPY, 0` remains the centered, open A-style restoration frame used
by the existing wrappers. Happy, sad, curious, surprised, and wink use a
48-frame cycle, as do lovey-dovey, shy and sleeping; glances and sleepy use 24 frames.
The happy cycle's frames 34-42 are reserved for the nine-frame standalone blink: open -> half ->
narrow -> closed crescent -> narrow -> half -> open. This also preserves
idle-loop callers that render consecutive frames beyond 47.

The OLED initializes lazily and remains owned by this driver. Do not run these
actions concurrently or alongside the separate self-test's OLED task. No
desktop event names, action bundles or endless loops belong here.