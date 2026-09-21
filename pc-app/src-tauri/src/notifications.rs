use crate::transport::{Command, Transport};
use serde::Serialize;
use std::sync::{Arc, Mutex};

const PENDING_CAPACITY: usize = 5;

#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct NotificationSnapshot {
    pub supported: bool,
    pub permission: String,
    pub enabled: bool,
    pub pending: usize,
    pub queued: Vec<QueuedNotification>,
    pub last_event: Option<String>,
    pub last_error: Option<String>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct QueuedNotification {
    pub source: String,
    pub text: String,
    pub oled: String,
    pub sound: String,
    pub light: String,
}

#[derive(Debug, PartialEq)]
struct NotificationRoute {
    oled: &'static str,
    sound: &'static str,
    light: &'static str,
    title_index: usize,
    text_prefix: &'static str,
}

struct RelayState {
    snapshot: NotificationSnapshot,
    #[cfg(windows)]
    initialized: bool,
    #[cfg(windows)]
    seen: std::collections::HashSet<String>,
    #[cfg(windows)]
    pending: std::collections::VecDeque<QueuedNotification>,
}

impl Default for RelayState {
    fn default() -> Self {
        Self {
            snapshot: NotificationSnapshot {
                supported: cfg!(windows),
                permission: if cfg!(windows) {
                    "unknown"
                } else {
                    "unsupported"
                }
                .into(),
                ..NotificationSnapshot::default()
            },
            #[cfg(windows)]
            initialized: false,
            #[cfg(windows)]
            seen: std::collections::HashSet::new(),
            #[cfg(windows)]
            pending: std::collections::VecDeque::new(),
        }
    }
}

#[derive(Clone)]
pub struct NotificationRelay {
    state: Arc<Mutex<RelayState>>,
    transport: Transport,
}

impl NotificationRelay {
    pub fn new(transport: Transport) -> Self {
        let relay = Self {
            state: Arc::new(Mutex::new(RelayState::default())),
            transport,
        };
        #[cfg(windows)]
        relay.start();
        relay
    }

    pub fn snapshot(&self) -> Result<NotificationSnapshot, String> {
        self.state
            .lock()
            .map(|state| {
                let mut snapshot = state.snapshot.clone();
                #[cfg(windows)]
                {
                    snapshot.queued = state.pending.iter().cloned().collect();
                }
                snapshot
            })
            .map_err(|_| "Notification relay state unavailable".into())
    }

    pub fn set_enabled(&self, enabled: bool) -> Result<(), String> {
        let mut state = self
            .state
            .lock()
            .map_err(|_| "Notification relay state unavailable")?;
        if enabled && state.snapshot.permission != "allowed" {
            return Err("Allow Windows notification access before enabling the relay".into());
        }
        state.snapshot.enabled = enabled;
        #[cfg(windows)]
        if enabled {
            state.initialized = false;
        }
        if !enabled {
            #[cfg(windows)]
            {
                state.pending.clear();
                state.snapshot.pending = 0;
            }
        }
        Ok(())
    }

    #[cfg(windows)]
    pub fn request_access(&self, app: &tauri::AppHandle) -> Result<NotificationSnapshot, String> {
        use windows::UI::Notifications::Management::UserNotificationListener;

        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        app.run_on_main_thread(move || {
            let operation = UserNotificationListener::Current()
                .and_then(|listener| listener.RequestAccessAsync())
                .map_err(|error| error.to_string());
            let _ = sender.send(operation);
        })
        .map_err(|error| error.to_string())?;
        let operation = receiver
            .recv()
            .map_err(|_| "Notification permission request was interrupted")??;
        let status = operation.get().map_err(|error| error.to_string())?;
        let permission = permission_name(status);
        let mut state = self
            .state
            .lock()
            .map_err(|_| "Notification relay state unavailable")?;
        state.snapshot.permission = permission.into();
        state.snapshot.last_error = None;
        state.initialized = false;
        Ok(state.snapshot.clone())
    }

    #[cfg(not(windows))]
    pub fn request_access(&self, _app: &tauri::AppHandle) -> Result<NotificationSnapshot, String> {
        Err("Windows notification listening is only available on Windows".into())
    }
}

fn normalize_title(title: &str) -> String {
    let mut normalized = String::new();
    let mut previous_space = false;
    for character in title.trim().chars() {
        let output = if character.is_ascii_graphic() {
            character
        } else if character.is_whitespace() {
            ' '
        } else {
            '?'
        };
        if output == ' ' && previous_space {
            continue;
        }
        if normalized.len() == crate::protocol::MAX_MARQUEE_TEXT {
            break;
        }
        normalized.push(output);
        previous_space = output == ' ';
    }
    normalized.trim_end().to_owned()
}

fn route_for(app: &str, title: &str, body: &str) -> Option<NotificationRoute> {
    let app = app.to_ascii_lowercase();
    if app.contains("teams") {
        return Some(NotificationRoute {
            oled: "oled.curious",
            sound: "speaker.trill",
            light: "neopixel.rainbow",
            title_index: 0,
            text_prefix: "",
        });
    }
    if !app.contains("outlook") {
        return None;
    }
    let content = format!("{title} {body}").to_ascii_lowercase();
    if ["meeting", "reminder", "starting", "appointment"]
        .iter()
        .any(|keyword| content.contains(keyword))
    {
        Some(NotificationRoute {
            oled: "oled.surprised",
            sound: "speaker.whistle",
            light: "neopixel.blink_yellow",
            title_index: 0,
            text_prefix: "",
        })
    } else {
        Some(NotificationRoute {
            oled: "oled.happy",
            sound: "speaker.chime",
            light: "neopixel.pulse_blue",
            // Outlook mail puts the sender before the subject.
            title_index: 1,
            text_prefix: "Email :  ",
        })
    }
}

fn routed_item(app: &str, lines: &[String]) -> Result<Option<QueuedNotification>, String> {
    let heading = lines.first().map(String::as_str).unwrap_or("");
    let body = lines.get(1..).unwrap_or_default().join(" ");
    let Some(route) = route_for(app, heading, &body) else {
        return Ok(None);
    };
    let title = lines
        .get(route.title_index)
        .map(String::as_str)
        .unwrap_or("");
    let mut text = normalize_title(title);
    if text.is_empty() {
        return Err(format!(
            "{app} notification has no readable title or email subject; notification skipped"
        ));
    }
    text.truncate(crate::protocol::MAX_MARQUEE_TEXT - route.text_prefix.len());
    let text = format!("{}{}", route.text_prefix, text.trim_end());
    Ok(Some(QueuedNotification {
        source: format!("{app}: {title}"),
        text,
        oled: route.oled.into(),
        sound: route.sound.into(),
        light: route.light.into(),
    }))
}

#[cfg(windows)]
fn permission_name(
    status: windows::UI::Notifications::Management::UserNotificationListenerAccessStatus,
) -> &'static str {
    use windows::UI::Notifications::Management::UserNotificationListenerAccessStatus;
    match status {
        UserNotificationListenerAccessStatus::Allowed => "allowed",
        UserNotificationListenerAccessStatus::Denied => "denied",
        _ => "unspecified",
    }
}

#[cfg(windows)]
impl NotificationRelay {
    fn start(&self) {
        let relay = self.clone();
        std::thread::Builder::new()
            .name("tokki-notifications".into())
            .spawn(move || loop {
                relay.poll();
                std::thread::sleep(std::time::Duration::from_millis(750));
            })
            .expect("Could not start Windows notification listener");
    }

    fn poll(&self) {
        use windows::UI::Notifications::Management::UserNotificationListener;

        let listener = match UserNotificationListener::Current() {
            Ok(listener) => listener,
            Err(error) => {
                self.record_error(format!("Packaged Windows installation required: {error}"));
                return;
            }
        };
        let access = match listener.GetAccessStatus() {
            Ok(access) => access,
            Err(error) => {
                self.record_error(error.to_string());
                return;
            }
        };
        let permission = permission_name(access);
        {
            let Ok(mut state) = self.state.lock() else {
                return;
            };
            state.snapshot.permission = permission.into();
            if permission != "allowed" {
                state.snapshot.enabled = false;
                state.initialized = false;
                state.pending.clear();
                state.snapshot.pending = 0;
                return;
            }
            if !state.snapshot.enabled {
                return;
            }
        }
        if let Err(error) = self.capture(&listener) {
            self.record_error(error);
            return;
        }
        self.dispatch();
    }

    fn capture(
        &self,
        listener: &windows::UI::Notifications::Management::UserNotificationListener,
    ) -> Result<(), String> {
        use std::collections::HashSet;
        use windows::UI::Notifications::NotificationKinds;

        let notifications = listener
            .GetNotificationsAsync(NotificationKinds::Toast)
            .and_then(|operation| operation.get())
            .map_err(|error| error.to_string())?;
        let mut current = HashSet::new();
        let mut additions = Vec::new();
        for index in 0..notifications.Size().map_err(|error| error.to_string())? {
            let notification = notifications
                .GetAt(index)
                .map_err(|error| error.to_string())?;
            let app = notification
                .AppInfo()
                .and_then(|info| info.DisplayInfo())
                .and_then(|info| info.DisplayName())
                .map(|name| name.to_string())
                .map_err(|error| error.to_string())?;
            let key = format!(
                "{}:{}",
                app,
                notification.Id().map_err(|error| error.to_string())?
            );
            current.insert(key.clone());
            let already_seen = {
                let state = self
                    .state
                    .lock()
                    .map_err(|_| "Notification relay state unavailable")?;
                !state.initialized || state.seen.contains(&key)
            };
            if !already_seen {
                match extract_item(&notification, &app) {
                    Ok(Some(item)) => additions.push(item),
                    Ok(None) => {}
                    Err(error) => self.record_error(error),
                }
            }
        }
        let mut state = self
            .state
            .lock()
            .map_err(|_| "Notification relay state unavailable")?;
        if !state.initialized {
            state.initialized = true;
            state.seen = current;
            return Ok(());
        }
        state.seen = current;
        for item in additions {
            if state.pending.len() < PENDING_CAPACITY {
                state.pending.push_back(item);
            } else {
                state.snapshot.last_error =
                    Some("Notification FIFO full; newest notification dropped".into());
            }
        }
        state.snapshot.pending = state.pending.len();
        Ok(())
    }

    fn dispatch(&self) {
        let snapshot = match self.transport.snapshot() {
            Ok(snapshot) => snapshot,
            Err(error) => {
                self.record_error(error);
                return;
            }
        };
        if snapshot.status != "connected"
            || snapshot
                .activity
                .iter()
                .any(|activity| matches!(activity.state.as_str(), "sending" | "queued" | "running"))
        {
            return;
        }
        let item = {
            let Ok(state) = self.state.lock() else {
                return;
            };
            state.pending.front().cloned()
        };
        let Some(item) = item else { return };
        if let Err(error) = crate::transport::validate_notification_actions(
            &snapshot,
            &item.oled,
            &item.sound,
            &item.light,
        ) {
            self.record_error(error);
            return;
        }
        {
            let Ok(mut state) = self.state.lock() else {
                return;
            };
            if !state.snapshot.enabled {
                return;
            }
            state.pending.pop_front();
            state.snapshot.pending = state.pending.len();
        }
        if let Err(error) = self.transport.submit(Command::Notification {
            text: item.text,
            oled: item.oled,
            sound: item.sound,
            light: item.light,
        }) {
            self.record_error(error);
            return;
        }
        if let Ok(mut state) = self.state.lock() {
            state.snapshot.last_event = Some(item.source);
            state.snapshot.last_error = None;
        }
    }

    fn record_error(&self, error: String) {
        if let Ok(mut state) = self.state.lock() {
            state.snapshot.last_error = Some(error);
        }
    }
}

#[cfg(windows)]
fn extract_item(
    notification: &windows::UI::Notifications::UserNotification,
    app: &str,
) -> Result<Option<QueuedNotification>, String> {
    use windows::UI::Notifications::KnownNotificationBindings;

    let binding_name =
        KnownNotificationBindings::ToastGeneric().map_err(|error| error.to_string())?;
    let binding = notification
        .Notification()
        .and_then(|notification| notification.Visual())
        .and_then(|visual| visual.GetBinding(&binding_name))
        .map_err(|error| error.to_string())?;
    let elements = binding
        .GetTextElements()
        .map_err(|error| error.to_string())?;
    let mut text = Vec::new();
    for index in 0..elements.Size().map_err(|error| error.to_string())? {
        let value = elements
            .GetAt(index)
            .and_then(|element| element.Text())
            .map(|value| value.to_string())
            .map_err(|error| error.to_string())?;
        text.push(value);
    }
    routed_item(app, &text)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn toast_lines(lines: &[&str]) -> Vec<String> {
        lines.iter().map(|line| (*line).to_owned()).collect()
    }

    #[test]
    fn routes_allowlisted_sources_with_meeting_precedence() {
        assert_eq!(
            route_for("Microsoft Teams", "Build", "ready"),
            Some(NotificationRoute {
                oled: "oled.curious",
                sound: "speaker.trill",
                light: "neopixel.rainbow",
                title_index: 0,
                text_prefix: ""
            })
        );
        assert_eq!(
            route_for("Outlook", "Inbox", "hello"),
            Some(NotificationRoute {
                oled: "oled.happy",
                sound: "speaker.chime",
                light: "neopixel.pulse_blue",
                title_index: 1,
                text_prefix: "Email :  "
            })
        );
        assert_eq!(
            route_for("Microsoft Outlook", "Reminder", "Design meeting"),
            Some(NotificationRoute {
                oled: "oled.surprised",
                sound: "speaker.whistle",
                light: "neopixel.blink_yellow",
                title_index: 0,
                text_prefix: ""
            })
        );
        assert_eq!(route_for("Slack", "Build", "ready"), None);
    }

    #[test]
    fn only_the_title_is_scrolled_and_fifty_characters_are_preserved() {
        let item = routed_item(
            "Microsoft Teams",
            &toast_lines(&["  Café\n\t review  ", "Private message body"]),
        )
        .unwrap()
        .unwrap();
        assert_eq!(item.text, "Caf? review");
        assert_eq!(item.oled, "oled.curious");
        assert_eq!(item.sound, "speaker.trill");
        assert_eq!(item.light, "neopixel.rainbow");
        assert_eq!(normalize_title(&"x".repeat(50)), "x".repeat(50));
        assert_eq!(normalize_title(&"x".repeat(51)), "x".repeat(50));
        let reminder = routed_item(
            "Outlook",
            &toast_lines(&["Design review", "Meeting starting"]),
        )
        .unwrap()
        .unwrap();
        assert_eq!(reminder.text, "Design review");
        assert_eq!(reminder.oled, "oled.surprised");
        assert_eq!(reminder.light, "neopixel.blink_yellow");
        assert!(routed_item("Slack", &toast_lines(&["Build", "ready"]))
            .unwrap()
            .is_none());
    }

    #[test]
    fn outlook_mail_scrolls_the_subject_not_the_sender_or_preview() {
        for app in ["Outlook", "Microsoft Outlook", "Outlook (new)"] {
            for lines in [
                toast_lines(&["Alex Example", "Release notes"]),
                toast_lines(&["Alex Example", "Release notes", "Private message preview"]),
            ] {
                let item = routed_item(app, &lines).unwrap().unwrap();
                assert_eq!(item.text, "Email :  Release notes");
                assert_eq!(item.source, format!("{app}: Release notes"));
                assert_eq!(item.oled, "oled.happy");
                assert_eq!(item.sound, "speaker.chime");
                assert_eq!(item.light, "neopixel.pulse_blue");
            }
        }
    }

    #[test]
    fn outlook_subject_is_normalized_with_prefix_inside_fifty_character_limit() {
        let subject = format!("  Café\n\t {}  ", "x".repeat(60));
        let item = routed_item(
            "Outlook",
            &["Alex Example".into(), subject, "Private preview".into()],
        )
        .unwrap()
        .unwrap();
        assert_eq!(item.text, format!("Email :  Caf? {}", "x".repeat(36)));
        assert_eq!(item.text.len(), 50);
    }

    #[test]
    fn email_prefix_leaves_forty_one_characters_for_the_subject() {
        for length in [1, 40, 41, 42, 50, 51] {
            let item = routed_item("Outlook", &["Alex Example".into(), "x".repeat(length)])
                .unwrap()
                .unwrap();
            assert_eq!(
                item.text,
                format!("Email :  {}", "x".repeat(length.min(41)))
            );
            assert!(item.text.len() <= 50);
        }
    }

    #[test]
    fn missing_subject_is_reported_without_substituting_sender_or_body() {
        for lines in [
            toast_lines(&["Alex Example"]),
            toast_lines(&["Alex Example", ""]),
            toast_lines(&["Alex Example", " \t ", "Private message preview"]),
        ] {
            let error = routed_item("Outlook", &lines).err().unwrap();
            assert!(error.contains("no readable title or email subject"));
            assert!(error.contains("skipped"));
        }
        assert!(routed_item(
            "Microsoft Teams",
            &toast_lines(&["", "Do not substitute the body"])
        )
        .is_err());
    }

    #[cfg(windows)]
    #[test]
    fn snapshot_exposes_pending_fifo_and_disabling_clears_it() {
        let mut state = RelayState::default();
        state.pending.push_back(QueuedNotification {
            source: "Microsoft Teams: Build".into(),
            text: "Build".into(),
            oled: "oled.curious".into(),
            sound: "speaker.trill".into(),
            light: "neopixel.rainbow".into(),
        });
        state.snapshot.pending = state.pending.len();
        let relay = NotificationRelay {
            state: Arc::new(Mutex::new(state)),
            transport: Transport::new(),
        };

        let snapshot = relay.snapshot().unwrap();
        assert_eq!(snapshot.pending, 1);
        assert_eq!(snapshot.queued.len(), 1);
        assert_eq!(snapshot.queued[0].text, "Build");
        assert_eq!(snapshot.queued[0].oled, "oled.curious");
        assert_eq!(snapshot.queued[0].light, "neopixel.rainbow");

        relay.set_enabled(false).unwrap();
        let snapshot = relay.snapshot().unwrap();
        assert_eq!(snapshot.pending, 0);
        assert!(snapshot.queued.is_empty());
    }
}
