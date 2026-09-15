# Physical Tokki desktop app

This directory contains the Tauri 2, React, and TypeScript desktop app for
configuring the Physical Tokki pet. The current milestone uses a mock device
transport: the interface runs, but it does not connect to the ESP32 or COM5
yet.

## Prerequisites

Install the following before running the native desktop app:

- Node.js and npm
- Rust and Cargo
- Microsoft Edge WebView2 Runtime
- Microsoft Visual Studio C++ Build Tools

Recommended VS Code extensions:

- Tauri (`tauri-apps.tauri-vscode`)
- rust-analyzer (`rust-lang.rust-analyzer`)

## Run the native desktop app

From the repository root:

```powershell
Set-Location .\pc-app
npm install
npm run tauri dev
```

The first run can take several minutes while Cargo compiles the Rust and
Tauri dependencies. A native Physical Tokki window opens automatically when
the build completes.

Use **Connect mock** to exercise the Device, Gestures, and Events screens.
Gesture previews currently update the interface only and do not control the
physical pet.

Stop the development app with `Ctrl+C` in the terminal that started it.

## Run the browser UI only

To work on the React interface without compiling or launching Tauri:

```powershell
Set-Location .\pc-app
npm install
npm run dev
```

Open the URL printed by Vite, normally `http://localhost:1420`.

## Validate changes

Build and type-check the frontend:

```powershell
npm run build
```

Check the Rust/Tauri backend:

```powershell
cargo check --manifest-path .\src-tauri\Cargo.toml
```

Build an installable desktop bundle:

```powershell
npm run tauri build
```

Generated installers are written below `src-tauri\target\release\bundle`.

## Current limitations

- Serial-port discovery and COM-port communication are not implemented.
- The action catalog is mock data rather than data read from firmware.
- Reminder rules are not persisted or scheduled in the background.
- Manager-email events use a placeholder integration.
