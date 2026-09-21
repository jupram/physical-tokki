# Windows notification relay setup

This guide installs Physical Tokki on another Windows laptop and enables the
Teams and Outlook notification relay. It is written for either a person or an
automation agent to follow.

## Compatibility

- Windows 10 version 1809 (build 17763) or newer, or Windows 11.
- The MSIX architecture must match Windows: `arm64`, `x64`, or `x86`.
- Microsoft Edge WebView2 Runtime must be installed.
- Windows notification access is required. Browser, `tauri dev`, MSI, and NSIS
  builds cannot use `UserNotificationListener`; install the MSIX.
- A data-capable USB cable and the board's USB serial driver are required for
  physical output.
- The firmware and desktop app must come from the same repository revision when
  protocol features change.

The current locally built `arm64` MSIX only runs on ARM64 Windows. It is signed
with a private development certificate that is not trusted on other laptops by
default. For broad distribution, sign each architecture with a certificate from
a trusted code-signing provider or distribute through Microsoft Store. For
controlled development machines, use the local-certificate process below.

## Build machine prerequisites

Install:

1. Git.
2. Node.js 22.12 or newer and npm.
3. Rust using the MSVC toolchain for the host architecture.
4. Visual Studio 2022 Build Tools with **Desktop development with C++**.
5. Windows 10/11 SDK, including `makeappx.exe` and `signtool.exe`.
6. Microsoft Edge WebView2 Runtime.
7. ESP-IDF 5.5.1 when firmware must also be built or flashed.

Verify the desktop dependencies from the repository root:

```powershell
Set-Location .\pc-app
npm ci
$env:Path = "$HOME\.cargo\bin;$env:Path"
cargo fetch --locked --manifest-path .\src-tauri\Cargo.toml
npm test
npm run build
cargo test --locked --manifest-path .\src-tauri\Cargo.toml
```

## Create a development signing certificate

Run in PowerShell on the build machine. Reuse an existing unexpired certificate
with the same subject when possible.

```powershell
$subject = 'CN=Physical Tokki Development'
$cert = Get-ChildItem Cert:\CurrentUser\My |
  Where-Object { $_.Subject -eq $subject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
  Sort-Object NotAfter -Descending |
  Select-Object -First 1

if (-not $cert) {
  $cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $subject `
    -FriendlyName 'Physical Tokki Development' `
    -CertStoreLocation Cert:\CurrentUser\My `
    -NotAfter (Get-Date).AddYears(3)
}

Export-Certificate `
  -Cert $cert `
  -FilePath .\Physical-Tokki-Development.cer `
  -Force

$cert.Thumbprint
```

Keep the private key on the build machine. Transfer only the generated `.cer`
and signed `.msix` to installation machines.

## Build and sign the architecture-matched MSIX

The packaging script uses the architecture of the current Windows process. Run
it natively on the target architecture, or use an appropriate CI runner.

```powershell
Set-Location .\pc-app
$thumbprint = 'PASTE_CERTIFICATE_THUMBPRINT'
.\src-tauri\windows\package-msix.ps1 `
  -Publisher 'CN=Physical Tokki Development' `
  -CertificateThumbprint $thumbprint
```

The certificate subject must exactly match the manifest publisher. The package
is written under `src-tauri\target\release\bundle\msix`.

Verify before distribution:

```powershell
Get-AuthenticodeSignature `
  .\src-tauri\target\release\bundle\msix\Physical-Tokki-0.1.0.2-<architecture>.msix
```

The expected status is `Valid` on a machine that trusts the signer.

## Trust and install on another development laptop

Copy the `.cer` and architecture-matched `.msix` to the laptop. In an
**Administrator Command Prompt**, run these with the actual certificate path:

```cmd
certutil -addstore Root "C:\path\to\Physical-Tokki-Development.cer"
certutil -addstore TrustedPeople "C:\path\to\Physical-Tokki-Development.cer"
```

Trust only a certificate received through a verified channel. Then install from
PowerShell:

```powershell
Add-AppxPackage -Path 'C:\path\to\Physical-Tokki-0.1.0.2-<architecture>.msix'
Get-AppxPackage -Name PhysicalTokki.Desktop |
  Select-Object Name, Version, Architecture, Publisher, Status
```

Expected values include publisher `CN=Physical Tokki Development` and status
`Ok`. A production certificate or Microsoft Store package should not require
these manual development-certificate trust steps.

## Flash matching firmware

In an exported ESP-IDF 5.5.1 PowerShell at the repository root:

```powershell
idf.py build
idf.py -p COM_PORT flash
```

Replace `COM_PORT` with the board's USB serial port. Close ESP-IDF Monitor before
opening the desktop app because only one process can own the port.

## Enable and verify the relay

1. Launch **Physical Tokki** from the Start menu.
2. Open **Device**, rescan, select the board's USB COM port, and connect.
3. Confirm firmware discovery finishes and the device status is connected.
4. Open **Events** and select **Allow access** if shown. Approve Windows
   notification access. If a toggle is already shown, permission is allowed.
5. Enable **Windows notification relay**. The status must read `Listening`.
6. Create a new notification after enabling the relay; existing notifications
   are intentionally treated as backlog and ignored.
7. Verify routing:

| Source | OLED | Speaker | NeoPixel |
| --- | --- | --- | --- |
| Microsoft Teams | One 40-character marquee | Lively trill | Purple |
| Outlook mail | One 40-character marquee | Chime | Blue |
| Outlook meeting/reminder | One 40-character marquee | Rising whistle | Yellow |

The NeoPixel remains lit for the same calculated duration as the OLED marquee,
then turns off. OLED, speaker, and NeoPixel use separate firmware workers and
run concurrently. If no board is connected, matching notifications remain in
the app's bounded five-item FIFO. The Events screen displays each queued source,
normalized marquee text, sound, and color. Its Play buttons simulate routing or
queued entries locally; simulation does not send to hardware or consume entries.

## Troubleshooting

- `0x800B0109`: trust the signer in both Local Machine `Root` and
  `TrustedPeople`, or use a publicly trusted package.
- `0x80073CF0`: confirm the package signature, certificate trust, and matching
  architecture.
- No access prompt: a visible relay toggle and `Listening` status mean access is
  already allowed. Otherwise check Windows notification privacy settings.
- No COM port: use a data-capable USB cable and install the board's USB UART
  driver. Bluetooth serial ports are not the Tokki USB connection.
- Port busy: close ESP-IDF Monitor and all other serial tools.
- Queued notification does not dispatch: connect the board and complete gesture
  discovery. The relay does not auto-connect.
- Updated desktop app with old firmware: marquee and sound may work, but the new
  notification-light request will fail as unsupported. Flash matching firmware.
