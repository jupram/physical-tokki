use crate::protocol::{self, Action, Framer, Hello, Message};
use serde::Serialize;
use serde_json::{json, Value};
use serialport::{DataBits, FlowControl, Parity, SerialPortType, StopBits};
use std::{
    collections::HashMap,
    io::{Read, Write},
    sync::{
        mpsc::{self, Receiver, SyncSender, TryRecvError},
        Arc, Mutex,
    },
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

const REQUEST_TIMEOUT: Duration = Duration::from_secs(3);
const HELLO_TIMEOUT: Duration = Duration::from_millis(900);
const EXECUTION_TIMEOUT: Duration = Duration::from_secs(120);
const HISTORY_LIMIT: usize = 100;
const COMMAND_CAPACITY: usize = 16;

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Activity {
    pub request_id: String,
    pub action_id: String,
    pub name: String,
    pub state: String,
    pub message: String,
    pub updated_at: u64,
}

#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Snapshot {
    pub revision: u64,
    pub status: String,
    pub port: Option<String>,
    pub hello: Option<Hello>,
    pub actions: Vec<Action>,
    pub activity: Vec<Activity>,
    pub last_error: Option<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Port {
    pub name: String,
    pub description: String,
}

pub fn ports() -> Result<Vec<Port>, String> {
    let mut ports: Vec<_> = serialport::available_ports()
        .map_err(|e| e.to_string())?
        .into_iter()
        .map(|p| {
            let description = match p.port_type {
                SerialPortType::UsbPort(info) => format!(
                    "{} — USB {:04X}:{:04X}",
                    info.product.unwrap_or_else(|| "USB serial".into()),
                    info.vid,
                    info.pid
                ),
                SerialPortType::BluetoothPort => "Bluetooth serial".into(),
                SerialPortType::PciPort => "PCI serial".into(),
                SerialPortType::Unknown => "Serial port".into(),
            };
            Port {
                name: p.port_name,
                description,
            }
        })
        .collect();
    ports.sort_by(|a, b| a.name.cmp(&b.name));
    Ok(ports)
}

pub enum Command {
    Connect(String),
    Disconnect,
    Refresh,
    Run(String),
}

pub struct Transport {
    sender: SyncSender<Command>,
    snapshot: Arc<Mutex<Snapshot>>,
}

impl Transport {
    pub fn new() -> Self {
        let (sender, receiver) = mpsc::sync_channel(COMMAND_CAPACITY);
        let snapshot = Arc::new(Mutex::new(Snapshot {
            status: "disconnected".into(),
            ..Snapshot::default()
        }));
        let shared = Arc::clone(&snapshot);
        thread::Builder::new()
            .name("tokki-serial".into())
            .spawn(move || Worker::new(shared).run(receiver))
            .expect("Could not start serial worker");
        Self { sender, snapshot }
    }

    pub fn submit(&self, command: Command) -> Result<(), String> {
        match &command {
            Command::Connect(name) if name.is_empty() || name.len() > 256 => {
                return Err("Select a valid serial port".into());
            }
            Command::Run(id) if !protocol::valid_action_id(id) => {
                return Err("Invalid gesture ID".into());
            }
            _ => {}
        }
        self.sender
            .try_send(command)
            .map_err(|e| format!("Serial worker unavailable or command queue full: {e}"))
    }

    pub fn snapshot(&self) -> Result<Snapshot, String> {
        self.snapshot
            .lock()
            .map(|s| s.clone())
            .map_err(|_| "Serial state unavailable".into())
    }
}

#[derive(Clone)]
enum PendingKind {
    Hello,
    Catalog,
    Run,
}
struct Pending {
    kind: PendingKind,
    deadline: Instant,
}

trait SerialIo: Read + Write + Send {}
impl<T: Read + Write + Send> SerialIo for T {}

struct Worker {
    shared: Arc<Mutex<Snapshot>>,
    state: Snapshot,
    port: Option<Box<dyn SerialIo>>,
    framer: Framer,
    pending: HashMap<String, Pending>,
    executions: HashMap<String, Instant>,
    catalog: Vec<Action>,
    catalog_total: Option<usize>,
    sequence: u64,
    hello_attempts: u8,
}

fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

fn in_flight(state: &str) -> bool {
    matches!(state, "sending" | "queued" | "running")
}

impl Worker {
    fn new(shared: Arc<Mutex<Snapshot>>) -> Self {
        Self {
            shared,
            state: Snapshot {
                status: "disconnected".into(),
                ..Snapshot::default()
            },
            port: None,
            framer: Framer::default(),
            pending: HashMap::new(),
            executions: HashMap::new(),
            catalog: Vec::new(),
            catalog_total: None,
            sequence: 0,
            hello_attempts: 0,
        }
    }

    fn publish(&mut self) {
        self.state.revision += 1;
        if let Ok(mut shared) = self.shared.lock() {
            *shared = self.state.clone();
        }
    }

    fn error(&mut self, message: String) {
        self.state.last_error = Some(message);
        self.publish();
    }

    fn disconnect(&mut self, error: Option<String>) {
        self.port = None;
        self.pending.clear();
        self.executions.clear();
        self.framer = Framer::default();
        self.catalog.clear();
        self.catalog_total = None;
        self.state.actions.clear();
        self.state.hello = None;
        self.state.port = None;
        self.state.status = if error.is_some() {
            "error"
        } else {
            "disconnected"
        }
        .into();
        let reason = error
            .clone()
            .unwrap_or_else(|| "Disconnected by user".into());
        for activity in &mut self.state.activity {
            if in_flight(&activity.state) {
                activity.state = "failed".into();
                activity.message = format!(
                    "{reason}; device outcome unknown. Disconnect does not cancel gestures."
                );
                activity.updated_at = now_ms();
            }
        }
        self.state.last_error = error;
        self.publish();
    }

    fn next_id(&mut self) -> String {
        self.sequence += 1;
        format!("pc-{}-{}", now_ms(), self.sequence)
    }

    fn send(
        &mut self,
        id: String,
        method: &str,
        params: Value,
        kind: PendingKind,
    ) -> Result<(), String> {
        let bytes = protocol::request(&id, method, params)?;
        self.port
            .as_mut()
            .ok_or("Not connected")?
            .write_all(&bytes)
            .map_err(|e| format!("Serial write failed: {e}"))?;
        let timeout = if matches!(kind, PendingKind::Hello) {
            HELLO_TIMEOUT
        } else {
            REQUEST_TIMEOUT
        };
        self.pending.insert(
            id,
            Pending {
                kind,
                deadline: Instant::now() + timeout,
            },
        );
        Ok(())
    }

    fn hello(&mut self) -> Result<(), String> {
        self.hello_attempts += 1;
        let id = self.next_id();
        self.send(id, "hello", json!({}), PendingKind::Hello)
    }

    fn page(&mut self, cursor: usize) -> Result<(), String> {
        let id = self.next_id();
        self.send(
            id,
            "actions.list",
            json!({"cursor":cursor}),
            PendingKind::Catalog,
        )
    }

    fn connect(&mut self, name: String) -> Result<(), String> {
        if name.is_empty() || name.len() > 256 {
            return Err("Select a valid serial port".into());
        }
        self.disconnect(None);
        self.state.status = "connecting".into();
        self.state.port = Some(name.clone());
        self.publish();
        // DTR is suppressed at open, then both modem lines are explicitly deasserted.
        let mut port = serialport::new(&name, 115_200)
            .data_bits(DataBits::Eight)
            .parity(Parity::None)
            .stop_bits(StopBits::One)
            .flow_control(FlowControl::None)
            .timeout(Duration::from_millis(30))
            .dtr_on_open(false)
            .open()
            .map_err(|e| format!("Could not open {name}: {e}. Close other serial monitors."))?;
        port.write_data_terminal_ready(false)
            .map_err(|e| format!("Could not deassert DTR: {e}"))?;
        port.write_request_to_send(false)
            .map_err(|e| format!("Could not deassert RTS: {e}"))?;
        self.port = Some(Box::new(port));
        self.hello_attempts = 0;
        self.hello()
    }

    fn command(&mut self, command: Command) {
        let result = match command {
            Command::Connect(name) => self.connect(name),
            Command::Disconnect => {
                self.disconnect(None);
                return;
            }
            Command::Refresh => {
                if self.state.status != "connected" {
                    self.error("Connect and finish discovery before refreshing".into());
                    return;
                }
                self.state.status = "loading".into();
                self.state.last_error = None;
                self.state.actions.clear();
                self.catalog.clear();
                self.catalog_total = None;
                self.publish();
                self.page(0)
            }
            Command::Run(action_id) => {
                if self.state.status != "connected" {
                    self.error("Connect and finish discovery before sending a gesture".into());
                    return;
                }
                let Some(action) = self
                    .state
                    .actions
                    .iter()
                    .find(|a| a.id == action_id)
                    .cloned()
                else {
                    self.error("Gesture is not in the current device catalog".into());
                    return;
                };
                if self
                    .state
                    .activity
                    .iter()
                    .filter(|a| in_flight(&a.state))
                    .count()
                    >= 5
                {
                    self.error(
                        "Local queue full: at most one active and four waiting gestures".into(),
                    );
                    return;
                }
                let id = self.next_id();
                if self.state.activity.len() >= HISTORY_LIMIT {
                    if let Some(index) = self
                        .state
                        .activity
                        .iter()
                        .position(|a| !in_flight(&a.state))
                    {
                        self.state.activity.remove(index);
                    }
                }
                self.state.activity.push(Activity {
                    request_id: id.clone(),
                    action_id: action_id.clone(),
                    name: action.name,
                    state: "sending".into(),
                    message: "Awaiting firmware acceptance".into(),
                    updated_at: now_ms(),
                });
                self.state.last_error = None;
                self.publish();
                self.send(
                    id,
                    "action.run",
                    json!({"actionId":action_id}),
                    PendingKind::Run,
                )
            }
        };
        if let Err(error) = result {
            self.disconnect(Some(error));
        }
    }

    fn update_activity(&mut self, id: &str, state: &str, message: String) {
        if let Some(activity) = self.state.activity.iter_mut().find(|a| a.request_id == id) {
            activity.state = state.into();
            activity.message = message;
            activity.updated_at = now_ms();
        }
        self.publish();
    }

    fn message(&mut self, message: Message) -> Result<(), String> {
        match message {
            Message::Unknown => {}
            Message::IdleError(error) => self.error(format!(
                "Idle driver error — {}: {}",
                error.code, error.message
            )),
            Message::Response { id, result } => {
                let id = id.ok_or_else(|| match &result {
                    Err(e) => format!("Uncorrelated firmware error — {}: {}", e.code, e.message),
                    Ok(_) => "Uncorrelated firmware response".into(),
                })?;
                let Some(pending) = self.pending.remove(&id) else {
                    return Ok(());
                };
                let value = match result {
                    Ok(value) => value,
                    Err(error) => {
                        let message = format!("{}: {}", error.code, error.message);
                        if matches!(pending.kind, PendingKind::Run) {
                            self.update_activity(&id, "failed", message.clone());
                            self.error(message);
                            return Ok(());
                        }
                        return Err(message);
                    }
                };
                match pending.kind {
                    PendingKind::Hello => {
                        let hello = protocol::validate_hello(value)?;
                        if !hello.ready {
                            // Wait a full handshake interval before the next hello, even on a fast reply.
                            self.state.hello = Some(hello);
                            self.pending.insert(
                                id,
                                Pending {
                                    kind: PendingKind::Hello,
                                    deadline: pending.deadline,
                                },
                            );
                            self.publish();
                            return Ok(());
                        }
                        self.state.hello = Some(hello);
                        self.state.status = "loading".into();
                        self.publish();
                        self.page(0)?;
                    }
                    PendingKind::Catalog => {
                        match protocol::append_page(
                            &mut self.catalog,
                            &mut self.catalog_total,
                            value,
                        )? {
                            Some(cursor) => self.page(cursor)?,
                            None => {
                                self.state.actions = std::mem::take(&mut self.catalog);
                                self.state.status = "connected".into();
                                self.publish();
                            }
                        }
                    }
                    PendingKind::Run => {
                        if value.get("accepted").and_then(Value::as_bool) != Some(true) {
                            return Err("Invalid action acceptance response".into());
                        }
                        self.executions
                            .insert(id.clone(), Instant::now() + EXECUTION_TIMEOUT);
                        self.update_activity(
                            &id,
                            "queued",
                            "Accepted by firmware; waiting for action.started".into(),
                        );
                    }
                }
            }
            Message::Lifecycle {
                event,
                request_id,
                action_id,
                error,
            } => {
                let Some(activity) = self
                    .state
                    .activity
                    .iter()
                    .find(|a| a.request_id == request_id)
                else {
                    return Ok(());
                };
                if !in_flight(&activity.state) {
                    return Ok(());
                }
                if activity.action_id != action_id {
                    return Err("Lifecycle actionId does not match request".into());
                }
                if !self.executions.contains_key(&request_id) {
                    return Err("Lifecycle arrived before acceptance".into());
                }
                if event == "action.started" {
                    if activity.state != "queued" {
                        return Err("Duplicate or out-of-order action.started".into());
                    }
                    self.executions
                        .insert(request_id.clone(), Instant::now() + EXECUTION_TIMEOUT);
                    self.update_activity(
                        &request_id,
                        "running",
                        "Firmware is executing this gesture".into(),
                    );
                } else {
                    if activity.state != "running" {
                        return Err("Completion arrived before action.started".into());
                    }
                    self.executions.remove(&request_id);
                    if let Some(error) = error {
                        let message = format!("{}: {}", error.code, error.message);
                        self.update_activity(&request_id, "failed", message.clone());
                        self.error(message);
                    } else {
                        self.update_activity(
                            &request_id,
                            "completed",
                            "Completed by firmware".into(),
                        );
                    }
                }
            }
        }
        Ok(())
    }

    fn tick(&mut self, now: Instant) -> Result<(), String> {
        let expired: Vec<_> = self
            .pending
            .iter()
            .filter(|(_, p)| p.deadline <= now)
            .map(|(id, p)| (id.clone(), p.kind.clone()))
            .collect();
        for (id, kind) in expired {
            self.pending.remove(&id);
            match kind {
                PendingKind::Hello if self.hello_attempts < 5 => self.hello()?,
                PendingKind::Hello => {
                    return Err(
                        "Hello handshake timed out or firmware is not ready after five attempts"
                            .into(),
                    )
                }
                PendingKind::Catalog => return Err("Catalog request timed out".into()),
                PendingKind::Run => {
                    self.update_activity(
                        &id,
                        "timed_out",
                        "Acceptance timed out; outcome unknown. Not retried.".into(),
                    );
                    return Err(
                        "Gesture acceptance timed out; connection closed. No automatic retry."
                            .into(),
                    );
                }
            }
        }
        if let Some(id) = self
            .executions
            .iter()
            .find(|(_, deadline)| **deadline <= now)
            .map(|(id, _)| id.clone())
        {
            self.update_activity(
                &id,
                "timed_out",
                "No lifecycle progress for 120 seconds; outcome unknown. Not retried.".into(),
            );
            return Err(
                "Gesture lifecycle timed out; connection closed. No automatic retry.".into(),
            );
        }
        Ok(())
    }

    fn run(mut self, receiver: Receiver<Command>) {
        loop {
            for _ in 0..COMMAND_CAPACITY {
                match receiver.try_recv() {
                    Ok(command) => self.command(command),
                    Err(TryRecvError::Empty) => break,
                    Err(TryRecvError::Disconnected) => return,
                }
            }
            let mut bytes = [0u8; 256];
            if let Some(port) = self.port.as_mut() {
                match port.read(&mut bytes) {
                    Ok(0) => self.disconnect(Some("Serial port closed".into())),
                    Ok(count) => {
                        for byte in &bytes[..count] {
                            if let Some(result) = self.framer.push(*byte) {
                                if let Err(error) = result.and_then(|message| self.message(message))
                                {
                                    self.disconnect(Some(format!(
                                        "Protocol/transport failure: {error}"
                                    )));
                                    break;
                                }
                            }
                        }
                    }
                    Err(e)
                        if matches!(
                            e.kind(),
                            std::io::ErrorKind::TimedOut
                                | std::io::ErrorKind::WouldBlock
                                | std::io::ErrorKind::Interrupted
                        ) => {}
                    Err(e) => self.disconnect(Some(format!("Serial read failed: {e}"))),
                }
            } else {
                thread::sleep(Duration::from_millis(30));
            }
            if let Err(error) = self.tick(Instant::now()) {
                self.disconnect(Some(error));
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct TestWire {
        incoming: std::collections::VecDeque<u8>,
        requests: Vec<Value>,
        replies: bool,
    }

    struct TestPort(Arc<Mutex<TestWire>>);

    impl Read for TestPort {
        fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
            let mut wire = self.0.lock().unwrap();
            let count = buffer.len().min(wire.incoming.len());
            for byte in &mut buffer[..count] {
                *byte = wire.incoming.pop_front().unwrap();
            }
            Ok(count)
        }
    }

    impl Write for TestPort {
        fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> {
            let request: Value = serde_json::from_slice(&bytes[protocol::PREFIX.len()..]).unwrap();
            let mut wire = self.0.lock().unwrap();
            if wire.replies {
                let result = match request["method"].as_str().unwrap() {
                    "hello" => {
                        json!({"protocol":1,"firmware":"0.2.0","board":"test-device","ready":true,"queueCapacity":4})
                    }
                    "actions.list" => {
                        let cursor = request["params"]["cursor"].as_u64().unwrap();
                        let action = |id: &str| json!({"id":id,"name":id,"device":"test-device","cancellable":false});
                        if cursor == 0 {
                            json!({"actions":[action("new.one"),action("new.two")],"total":3,"nextCursor":2})
                        } else {
                            assert_eq!(cursor, 2);
                            json!({"actions":[action("new.three")],"total":3,"nextCursor":null})
                        }
                    }
                    "action.run" => json!({"accepted":true}),
                    method => panic!("Unexpected method {method}"),
                };
                let response = format!(
                    "TOKKI/1 {}\n",
                    json!({"id":request["id"],"ok":true,"result":result})
                );
                wire.incoming.extend(response.bytes());
            }
            wire.requests.push(request);
            Ok(bytes.len())
        }
        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }

    fn test_link(replies: bool) -> (Worker, Arc<Mutex<TestWire>>) {
        let wire = Arc::new(Mutex::new(TestWire {
            replies,
            ..TestWire::default()
        }));
        let mut w = worker();
        w.port = Some(Box::new(TestPort(Arc::clone(&wire))));
        (w, wire)
    }

    fn drain(w: &mut Worker) {
        loop {
            // Deliberately split protocol prefixes and UTF-8 across short reads.
            let mut bytes = [0u8; 7];
            let count = w.port.as_mut().unwrap().read(&mut bytes).unwrap();
            if count == 0 {
                break;
            }
            for byte in &bytes[..count] {
                if let Some(message) = w.framer.push(*byte) {
                    w.message(message.unwrap()).unwrap();
                }
            }
        }
    }

    fn worker() -> Worker {
        Worker::new(Arc::new(Mutex::new(Snapshot::default())))
    }
    fn activity(state: &str) -> Activity {
        Activity {
            request_id: "run-1".into(),
            action_id: "dynamic.action".into(),
            name: "Dynamic".into(),
            state: state.into(),
            message: String::new(),
            updated_at: 0,
        }
    }
    fn lifecycle(event: &str) -> Message {
        Message::Lifecycle {
            event: event.into(),
            request_id: "run-1".into(),
            action_id: "dynamic.action".into(),
            error: None,
        }
    }

    #[test]
    fn real_acceptance_and_lifecycle_drive_state() {
        let mut w = worker();
        w.state.activity.push(activity("sending"));
        w.pending.insert(
            "run-1".into(),
            Pending {
                kind: PendingKind::Run,
                deadline: Instant::now() + REQUEST_TIMEOUT,
            },
        );
        w.message(Message::Response {
            id: Some("run-1".into()),
            result: Ok(json!({"accepted":true})),
        })
        .unwrap();
        assert_eq!(w.state.activity[0].state, "queued");
        w.message(lifecycle("action.started")).unwrap();
        assert_eq!(w.state.activity[0].state, "running");
        w.message(lifecycle("action.completed")).unwrap();
        assert_eq!(w.state.activity[0].state, "completed");
        assert!(w.executions.is_empty());
    }

    #[test]
    fn out_of_order_events_are_not_success() {
        let mut w = worker();
        w.state.activity.push(activity("sending"));
        assert!(w.message(lifecycle("action.started")).is_err());
        w.state.activity[0].state = "queued".into();
        w.executions
            .insert("run-1".into(), Instant::now() + EXECUTION_TIMEOUT);
        assert!(w.message(lifecycle("action.completed")).is_err());
    }

    #[test]
    fn timeout_is_explicit_and_never_retried() {
        let mut w = worker();
        w.state.activity.push(activity("sending"));
        w.pending.insert(
            "run-1".into(),
            Pending {
                kind: PendingKind::Run,
                deadline: Instant::now(),
            },
        );
        let error = w.tick(Instant::now()).unwrap_err();
        w.disconnect(Some(error));
        assert_eq!(w.state.activity[0].state, "timed_out");
        assert!(w.pending.is_empty());
        assert!(w.state.activity[0].message.contains("Not retried"));
        w.message(lifecycle("action.completed")).unwrap();
        assert_eq!(w.state.activity[0].state, "timed_out");
    }

    #[test]
    fn disconnect_invalidates_in_flight_catalog_and_pending() {
        let mut w = worker();
        w.state.activity.push(activity("running"));
        w.state.actions.push(Action {
            id: "a".into(),
            name: "A".into(),
            device: "x".into(),
            cancellable: false,
        });
        w.executions.insert("run-1".into(), Instant::now());
        w.disconnect(Some("USB unplugged".into()));
        assert_eq!(w.state.activity[0].state, "failed");
        assert!(w.state.activity[0].message.contains("outcome unknown"));
        assert!(w.state.actions.is_empty() && w.executions.is_empty() && w.pending.is_empty());
    }

    #[test]
    fn stale_response_does_not_change_state() {
        let mut w = worker();
        w.message(Message::Response {
            id: Some("previous-session".into()),
            result: Ok(json!({"accepted":true})),
        })
        .unwrap();
        assert!(w.state.activity.is_empty());
        assert!(w.executions.is_empty());
    }

    #[test]
    fn queue_rejection_and_driver_failure_are_real_errors() {
        let mut w = worker();
        w.state.activity.push(activity("sending"));
        w.pending.insert(
            "run-1".into(),
            Pending {
                kind: PendingKind::Run,
                deadline: Instant::now(),
            },
        );
        w.message(Message::Response {
            id: Some("run-1".into()),
            result: Err(protocol::DeviceError {
                code: "device_busy".into(),
                message: "Queue full".into(),
            }),
        })
        .unwrap();
        assert_eq!(w.state.activity[0].state, "failed");
        assert!(w.state.last_error.as_ref().unwrap().contains("device_busy"));
    }

    #[test]
    fn command_channel_is_bounded() {
        let (sender, _receiver) = mpsc::sync_channel(COMMAND_CAPACITY);
        for _ in 0..COMMAND_CAPACITY {
            sender.try_send(Command::Refresh).unwrap();
        }
        assert!(sender.try_send(Command::Refresh).is_err());
    }

    #[test]
    fn handshake_fetches_every_page_then_sends_only_discovered_actions() {
        let (mut w, wire) = test_link(true);
        wire.lock().unwrap().incoming.extend(b"boot chatter\xff\n");
        w.hello().unwrap();
        drain(&mut w);
        assert_eq!(w.state.status, "connected");
        assert_eq!(w.state.actions.len(), 3);
        assert_eq!(wire.lock().unwrap().requests.len(), 3);
        w.command(Command::Run("new.three".into()));
        drain(&mut w);
        assert_eq!(w.state.activity[0].action_id, "new.three");
        assert_eq!(w.state.activity[0].state, "queued");
        let before = wire.lock().unwrap().requests.len();
        w.command(Command::Run("not.discovered".into()));
        assert_eq!(wire.lock().unwrap().requests.len(), before);
        assert!(w.state.last_error.as_ref().unwrap().contains("catalog"));
    }

    #[test]
    fn hello_retry_is_bounded_to_five_and_does_not_retry_runs() {
        let (mut w, wire) = test_link(false);
        w.hello().unwrap();
        for _ in 0..4 {
            w.tick(Instant::now() + HELLO_TIMEOUT + Duration::from_secs(1))
                .unwrap();
        }
        assert!(w
            .tick(Instant::now() + HELLO_TIMEOUT + Duration::from_secs(1))
            .unwrap_err()
            .contains("five attempts"));
        assert_eq!(wire.lock().unwrap().requests.len(), 5);
        assert!(wire
            .lock()
            .unwrap()
            .requests
            .iter()
            .all(|r| r["method"] == "hello"));
    }

    #[test]
    fn local_queue_allows_five_and_rejects_sixth_without_writing() {
        let (mut w, wire) = test_link(true);
        w.hello().unwrap();
        drain(&mut w);
        for _ in 0..5 {
            w.command(Command::Run("new.one".into()));
        }
        let before = wire.lock().unwrap().requests.len();
        w.command(Command::Run("new.one".into()));
        assert_eq!(w.state.activity.len(), 5);
        assert_eq!(wire.lock().unwrap().requests.len(), before);
        assert!(w.state.last_error.as_ref().unwrap().contains("queue full"));
        drain(&mut w);
        assert!(w.state.activity.iter().all(|a| a.state == "queued"));
    }

    #[test]
    fn lifecycle_timeout_and_catalog_timeout_close_without_success() {
        let mut w = worker();
        w.state.activity.push(activity("queued"));
        w.executions.insert("run-1".into(), Instant::now());
        assert!(w
            .tick(Instant::now())
            .unwrap_err()
            .contains("lifecycle timed out"));
        assert_eq!(w.state.activity[0].state, "timed_out");
        w.executions.clear();
        w.pending.insert(
            "page-1".into(),
            Pending {
                kind: PendingKind::Catalog,
                deadline: Instant::now(),
            },
        );
        assert_eq!(
            w.tick(Instant::now()).unwrap_err(),
            "Catalog request timed out"
        );
    }

    #[test]
    fn failure_lifecycle_preserves_driver_error() {
        let mut w = worker();
        w.state.activity.push(activity("running"));
        w.executions
            .insert("run-1".into(), Instant::now() + EXECUTION_TIMEOUT);
        w.message(Message::Lifecycle {
            event: "action.failed".into(),
            request_id: "run-1".into(),
            action_id: "dynamic.action".into(),
            error: Some(protocol::DeviceError {
                code: "driver_error".into(),
                message: "ESP_ERR_TIMEOUT".into(),
            }),
        })
        .unwrap();
        assert_eq!(w.state.activity[0].state, "failed");
        assert!(w.state.activity[0].message.contains("ESP_ERR_TIMEOUT"));
        assert!(w.executions.is_empty());
    }

    #[test]
    fn refresh_replaces_catalog_instead_of_accumulating_duplicates() {
        let (mut w, _) = test_link(true);
        w.hello().unwrap();
        drain(&mut w);
        w.command(Command::Refresh);
        assert!(w.state.actions.is_empty());
        assert_eq!(w.state.status, "loading");
        drain(&mut w);
        assert_eq!(w.state.actions.len(), 3);
        assert_eq!(w.state.status, "connected");
    }
}
