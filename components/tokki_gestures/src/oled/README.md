# OLED gestures

This contribution extends the original eyes-only scope to the requested fixed
reminder message and monochrome art. Owner review of that scope is required.

Actions are finite wrappers over `tokki_oled` and `pet_eyes`, not independent
display drivers. Happy, sad, curious and surprised eyes run for 48 frames;
blink runs only the nine-frame close/reopen section. Drink water is a centered
two-line message. Water drop and fire each run for 24 frames. Every action
retains its final frame and returns display errors immediately.

The OLED initializes lazily and remains owned by this driver. Do not run these
actions concurrently or alongside the separate self-test's OLED task. No
desktop event names, action bundles or endless loops belong here.