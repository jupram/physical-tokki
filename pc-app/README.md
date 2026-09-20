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

## Manual hardware verification

1. Use a pet already running the `TOKKI/1` firmware (hello currently reports
   firmware `0.2.0`, protocol `1`, and `queueCapacity: 4`). Connect its USB cable.
2. **Close ESP-IDF Monitor, Arduino Serial Monitor, and any other serial owner.**
   This app and a serial monitor must not own the port simultaneously.
3. On **Device**, click **Rescan**, select the board's actual COM port, then
   **Connect**. Enumeration does not open ports. The app never scans by opening
   every port and never auto-connects after a failure.
4. Verify real firmware/board metadata and the discovered action count. The
   current firmware has 44 descriptors; the UI does not assume that number.
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
   animations (including lovey-dovey and shy) separated by calm holds, and
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
These do not replace the physical checks above; USB line transients, real driver
behavior, motor/display/audio effects, and installers require hardware/manual
validation. No automatic hardware test or flashing occurs.

`src-tauri\tests\fixtures\firmware-frames.ndjson` is an unchanged transcript from
the compiled C protocol implementation and real firmware registry. One focused
Rust conformance test decodes its hello, eight catalog pages/all 31 IDs,
acceptance/lifecycle events, and driver/not-found errors. Regenerate from the
repository root with `.\tests\host\run.ps1 -WireFixturePath <output-path>` and
review any fixture changes against the shared contract.
