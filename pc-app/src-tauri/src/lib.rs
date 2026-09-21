mod notifications;
mod protocol;
mod transport;

#[tauri::command]
fn serial_ports() -> Result<Vec<transport::Port>, String> {
    transport::ports()
}

#[tauri::command]
fn serial_snapshot(
    transport: tauri::State<'_, transport::Transport>,
) -> Result<transport::Snapshot, String> {
    transport.snapshot()
}

#[tauri::command]
fn serial_connect(
    port: String,
    transport: tauri::State<'_, transport::Transport>,
) -> Result<(), String> {
    transport.submit(transport::Command::Connect(port))
}

#[tauri::command]
fn serial_disconnect(transport: tauri::State<'_, transport::Transport>) -> Result<(), String> {
    transport.submit(transport::Command::Disconnect)
}

#[tauri::command]
fn serial_refresh(transport: tauri::State<'_, transport::Transport>) -> Result<(), String> {
    transport.submit(transport::Command::Refresh)
}

#[tauri::command]
fn serial_run(
    action_id: String,
    transport: tauri::State<'_, transport::Transport>,
) -> Result<(), String> {
    transport.submit(transport::Command::Run(action_id))
}

#[tauri::command]
fn serial_marquee(
    text: String,
    transport: tauri::State<'_, transport::Transport>,
) -> Result<(), String> {
    transport.submit(transport::Command::Marquee(text))
}

#[tauri::command]
fn notification_snapshot(
    relay: tauri::State<'_, notifications::NotificationRelay>,
) -> Result<notifications::NotificationSnapshot, String> {
    relay.snapshot()
}

#[tauri::command]
fn notification_request_access(
    app: tauri::AppHandle,
    relay: tauri::State<'_, notifications::NotificationRelay>,
) -> Result<notifications::NotificationSnapshot, String> {
    relay.request_access(&app)
}

#[tauri::command]
fn notification_set_enabled(
    enabled: bool,
    relay: tauri::State<'_, notifications::NotificationRelay>,
) -> Result<(), String> {
    relay.set_enabled(enabled)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let transport = transport::Transport::new();
    let notifications = notifications::NotificationRelay::new(transport.clone());
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(transport)
        .manage(notifications)
        .invoke_handler(tauri::generate_handler![
            serial_ports,
            serial_snapshot,
            serial_connect,
            serial_disconnect,
            serial_refresh,
            serial_run,
            serial_marquee,
            notification_snapshot,
            notification_request_access,
            notification_set_enabled
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
