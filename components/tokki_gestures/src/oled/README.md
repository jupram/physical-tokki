# OLED gestures

This contribution extends the original eyes-only scope to the requested fixed
reminder message and monochrome art. Owner review of that scope is required.

Actions are finite wrappers over `tokki_oled` and `pet_eyes`, not independent
display drivers. Happy, sad, curious and surprised eyes run for 48 frames;
blink runs only the nine-frame close/reopen section. Drink water is a centered
two-line message. Water drop and fire each run for 24 frames. Every action
retains its final frame and returns display errors immediately.

The follow-up primitives are wink, checkmark, thinking dots, four directional
glances, sleepy eyes, heart, exclamation, lovey-dovey eyes and shy eyes.
These transient actions restore the centered happy-eye frame before returning.
Wink keeps the other eye open;
thinking dots advance once without an endless loop. No expression implies
sensor tracking, listening, or real task progress. See the parent catalog for
exact nominal durations and successful final states.

## Eye language and frame contract

The 128x64 renderer uses filled oval whites and pupils, clipped lids, and bold
closed crescent curves. There are no eyebrows or falling tears. All motion is
integer-only, with eased keyframes and small squash/stretch or pupil overshoots.
Existing enum values, action IDs, frame counts, and the 90 ms gesture frame
delay are unchanged. `PET_EYES_LOVEY_DOVEY` and `PET_EYES_SHY` are appended to
the enum and exposed as `oled.lovey_dovey` and `oled.shy`. Both new actions
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

`PET_EYES_HAPPY, 0` remains the centered, open A-style restoration frame used
by the existing wrappers. Happy, sad, curious, surprised, and wink use a
48-frame cycle, as do lovey-dovey and shy; glances and sleepy use 24 frames.
The happy cycle's frames 34-42 are reserved for the nine-frame standalone blink: open -> half ->
narrow -> closed crescent -> narrow -> half -> open. This also preserves
idle-loop callers that render consecutive frames beyond 47.

The OLED initializes lazily and remains owned by this driver. Do not run these
actions concurrently or alongside the separate self-test's OLED task. No
desktop event names, action bundles or endless loops belong here.