# Physical Tokki desktop app

Native Tauri 2 + React/TypeScript prototype for **discovering and manually sending
gestures to the pet**. The Rust backend owns a real USB UART connection; there is
no mock transport, hardcoded gesture catalog, or simulated completion.

## Prerequisites and run

Windows: Node.js (a version supported by Vite 8, such as 22.12+), npm, Rust/Cargo,
Microsoft Visual Studio C++ Build Tools, and Microsoft Edge WebView2 Runtime.
Install your board's USB UART driver if Windows does not expose its COM port.

From the repository root:

```powershell
Set-Location .\pc-app
npm ci
npm run tauri dev
```

If npm 11's peer resolver fails with `Cannot read properties of null (reading
'edgesOut')`, use `npm ci --legacy-peer-deps`. Stop the development app with
`Ctrl+C`. The app does not flash firmware.

`npm run dev` starts a **browser-only UI preview**. It explicitly explains that
the native app is required, disables connection controls, and cannot send
gestures. It never substitutes sample data or reports fake success. The existing
sidebar/device/catalog layout uses the Clawpilot light/dark palette; the OS theme
is detected at startup, with `?scoutTheme=light` or `?scoutTheme=dark` overrides.

## Tokki's desk

The home screen introduces Tokki with a gently animated pet illustration and
shortcuts to gestures, bundles, and manually tested event ideas. The illustration
is decorative, not a live view of the physical OLED. Reduced-motion preferences
disable decorative and preview animation.

The **Pet control desk** button stays in the top-right header on every screen
and opens Device settings. Its pet icon and status dot are **green only when
the native connection status is `connected`** (handshake and catalog discovery
complete). They are **red otherwise**, including disconnected, connecting,
loading, failed, and browser-only states. Text and an accessible status
announcement explain the state without relying on color alone.

Local previews remain usable without hardware. A connected preview still sends
the same gesture to the physical pet; this redesign does not change serial
commands, queuing, connection readiness, or completion reporting. Event triggers
remain manual; the UI does not claim that a scheduler is running.

The OLED picker and bundle palette offer **23 OLED gestures**, including
**Sleeping eyes (Zzz)** (`oled.sleeping`, 48 frames × 90 ms = 4.32 seconds),
**Night sky** (`oled.night_sky`, 4.32 seconds), and **Sunrise**
(`oled.sunrise`, 5.76 seconds). The sleeping preview gently closes into
low, happy-style crescent lids, breathes with a subtle bob, and floats three
monochrome Z letters above the right eye before reopening. Like the other eye
previews, its eye shapes are an approximation, not an exact firmware pixel copy.
It starts open, closes by frame 8, holds through frame 39, and reopens by frames
46–47. Stopping a local preview freezes it; replay starts again at frame zero.
Sleep and sky previews respect reduced motion by showing a representative still.

The sky scenes' monochrome
pixel previews follow the firmware's twinkling stars/moon/shooting star and
eased sun/rays/horizon geometry. Animated sleep and sky previews return to open
eyes when finished. Firmware's default idle shuffle has **9 choices**, including
one paired **sleeping eyes → night sky** sequence. Night sky only follows the
sleeping eyes, with no intervening pause or reopen. Each portion lasts
**48 frames × 60 ms = 2.88 seconds**; open eyes return after the sky.
The **600–1200 ms** rest holds occur between choices, not within the pair.
Manual previews/gestures are unchanged. Sunrise remains manual.

## Manual hardware verification

1. Use a pet already running the `TOKKI/1` firmware (hello currently reports
   firmware `0.2.0`, protocol `1`, and `queueCapacity: 4`). Connect its USB cable.
2. **Close ESP-IDF Monitor, Arduino Serial Monitor, and any other serial owner.**
   This app and a serial monitor must not own the port simultaneously.
3. On **Device**, click **Rescan**, select the board's actual COM port, then
   **Connect**. Enumeration does not open ports. The app never scans by opening
   every port and never auto-connects after a failure.
4. Verify real firmware/board metadata and the discovered action count. The
   current firmware has 47 action descriptors; the UI does not assume that number.
   Connect sends hello, waits for `ready: true`, then fetches **every catalog
   page**. **Gestures → Refresh catalog** repeats discovery from cursor zero.
   `ready` covers board/LED/RGB startup only, not verified OLED or speaker health.
   OLED/speaker initialize lazily; their failures arrive as `action.failed`.
   After flashing the updated sound catalog, reconnect or refresh discovery:
   `speaker.tone_low`, `speaker.tone_mid`, `speaker.tone_high`, and
   `speaker.tone_rise` replace the removed sigh, boing, down-step, and CC0
   single-bark actions. Update saved bundles that reference the retired IDs.
5. On **Gestures**, search by name, device, or ID and click **Send**, or use a
   local preview. A preview always animates in the PC app and also dispatches
   the same action to the pet while connected. Observe
   **awaiting acceptance → queued → running → completed**, driven by the device.
   Physically confirm the expected OLED/LED/speaker/NeoPixel behavior. Bundle
   lanes start together: separate devices run concurrently, while actions for
   one device remain ordered. Send several gestures to check the four waiting
   slots plus one active action per device. Driver failures must appear as
   **failed** with the firmware error.
6. Once each device's queue drains, confirm it resumes idle: shuffled eye
   animations/sequences (9 choices, including lovey-dovey, shy, and sleeping
   eyes immediately followed by night sky; 2.88 seconds per portion of the
   pair, with 600–1200 ms rest holds between choices), and
   occasional teal breathing with 6-14-second dark pauses. Send OLED and
   NeoPixel actions during idle to verify each takes priority without stopping
   the other device's idle animation.
   There is intentionally **no Stop button**: these gestures are finite and
   noncancellable. Disconnecting or closing the app does **not** cancel work
   already accepted by firmware.
7. Unplug USB during queued/running work. Confirm a real read failure or bounded
   timeout, cleared connection/catalog, and **outcome unknown** for unfinished
   requests (never invented completion). Replug, rescan if necessary, explicitly
   reconnect, and verify a fresh catalog. Old pending work is not replayed.
8. With no device, try a wrong/occupied port. Confirm an open/handshake failure,
   not a green success indication. Sending requires completed discovery.

Do not repeatedly resend a timed-out gesture without checking the pet: the
device might already have executed it. The app does **not retry action.run**.

## Native transport and bounds

- One background Rust worker owns the serial port independently of the webview.
  Settings: **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**.
  DTR is suppressed at open; DTR and RTS are explicitly deasserted afterward.
  Failure to set either line is reported, not ignored. USB drivers/hardware can
  still produce an open-time transient; this needs verification on the board.
- Newline UTF-8 `TOKKI/1 ` frames are capped at **1024 bytes including prefix
  and LF**; CRLF is accepted. Non-prefixed boot/log lines are ignored, including
  oversized logs. Malformed prefixed frames cause an explicit protocol failure.
- Requests have unique nonempty `[A-Za-z0-9_.:-]` IDs (at most 64 characters), correlated
  responses, and no automatic action retry. Late/unknown request IDs and unknown
  event types are ignored. Known lifecycle events must match the request/action.
- Hello alone is retried, at most **five attempts, 900 ms each**, to tolerate boot
  chatter or `ready: false`. Catalog and action acceptance time out after **3 s**.
  Accepted work must start within **120 s**, then finish within **120 s**.
  This conservative prototype watchdog is an uncertainty bound, not an action
  duration estimate. A timeout closes the connection, preserves explicit timeout
  history, and requires manual reconnect.
- Command channel: **16 entries**, bounded command strings. The local in-flight
  bound is the firmware's four waiting slots plus one worker for each device in
  the discovered catalog. Firmware remains authoritative and can return
  `device_busy`. Catalog: at most **4096**
  descriptors, no more than four per page; cursor progression, consistent total,
  duplicate IDs, required descriptor fields, and noncancellability are checked.
  Action IDs are at most **96 bytes** and cannot contain NUL. Native requests are
  shallow objects without NUL escapes, compatible with firmware's nesting limit.
  A catalog is exposed only after complete validation of all pages.
- Activity: last **100** app-session entries, preserving in-flight entries.
  Pending transport state/catalog are cleared on disconnect/reconnect. Historical
  terminal outcomes remain visible within this app process but are not persisted
  across app restarts. Errors retain firmware codes/messages.
- React polls the backend's revisioned snapshot every **250 ms** without
  overlapping polls. This is a real-state read, not a gesture timer. The worker
  continues reading/processing lifecycle events while the UI is on another tab
  or inactive.
- **Events are explicitly deferred**: no scheduler, reminders, email integration,
  event rules, reconnect automation, or gesture cancellation is implemented.

The shared wire contract is in `..\protocol\tokki-serial-v1.md`.

## Validation and build

```powershell
npm test
npm run build
cargo test --manifest-path .\src-tauri\Cargo.toml --lib
cargo check --manifest-path .\src-tauri\Cargo.toml
cargo fmt --manifest-path .\src-tauri\Cargo.toml -- --check
npm run tauri build -- --debug --no-bundle
```

The debug native executable is `src-tauri\target\debug\pc-app.exe`.
For release installers use `npm run tauri build`; installers are generated under
`src-tauri\target\release\bundle`.

Rust tests cover framing/CRLF/boot logs, frame size and resynchronization, strict
envelopes, request bounds, dynamic pagination, correlation, real acceptance and
lifecycle transitions, queue rejection, driver errors, explicit timeouts,
disconnect cleanup, stale events, bounded handshake retries, and a scripted
duplex test link fetching all pages. The scripted link exists **only in tests**.
Frontend tests cover the native-only guard, command routing, dynamic IDs, error
propagation without retries, queue counts, and unchanged backend snapshots.
Focused OLED tests cover the local catalog, sleeping-eye poses and Z geometry,
accessible preview output, and shared sleep/sky timing, stop/replay cleanup,
and reduced-motion changes.
These do not replace the physical checks above; USB line transients, real driver
behavior, motor/display/audio effects, and installers require hardware/manual
validation. No automatic hardware test or flashing occurs.

`src-tauri\tests\fixtures\firmware-frames.ndjson` is an unchanged transcript from
the compiled C protocol implementation and real firmware registry. One focused
Rust conformance test decodes its hello, eight catalog pages/all 31 IDs,
acceptance/lifecycle events, and driver/not-found errors. Regenerate from the
repository root with `.\tests\host\run.ps1 -WireFixturePath <output-path>` and
review any fixture changes against the shared contract.
