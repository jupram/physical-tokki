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

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(transport::Transport::new())
        .invoke_handler(tauri::generate_handler![
            serial_ports,
            serial_snapshot,
            serial_connect,
            serial_disconnect,
            serial_refresh,
            serial_run
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
