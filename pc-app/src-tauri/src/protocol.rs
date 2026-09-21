use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

pub const PREFIX: &[u8] = b"TOKKI/1 ";
pub const MAX_FRAME: usize = 1024;
pub const MAX_ACTIONS: usize = 4096;
pub const MAX_ACTION_ID: usize = 96;
pub const MAX_MARQUEE_TEXT: usize = 40;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Hello {
    pub protocol: u32,
    pub firmware: String,
    pub board: String,
    pub ready: bool,
    pub queue_capacity: usize,
}

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq)]
pub struct Action {
    pub id: String,
    pub name: String,
    pub device: String,
    pub cancellable: bool,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Page {
    pub actions: Vec<Action>,
    pub total: usize,
    pub next_cursor: Option<usize>,
}

#[derive(Debug, Deserialize)]
pub struct DeviceError {
    pub code: String,
    pub message: String,
}

#[derive(Debug)]
pub enum Message {
    Response {
        id: Option<String>,
        result: Result<Value, DeviceError>,
    },
    Lifecycle {
        event: String,
        request_id: String,
        action_id: String,
        error: Option<DeviceError>,
    },
    IdleError(DeviceError),
    Unknown,
}

fn field_string(value: &Value, key: &str) -> Result<String, String> {
    value
        .get(key)
        .and_then(Value::as_str)
        .filter(|s| !s.is_empty())
        .map(str::to_owned)
        .ok_or_else(|| format!("Missing or invalid {key}"))
}

pub fn valid_id(id: &str) -> bool {
    !id.is_empty()
        && id.len() <= 64
        && id
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"_.:-".contains(&byte))
}

pub fn valid_action_id(id: &str) -> bool {
    !id.is_empty() && id.len() <= MAX_ACTION_ID && !id.contains('\0')
}

pub fn valid_marquee_text(text: &str) -> bool {
    !text.is_empty()
        && text.len() <= MAX_MARQUEE_TEXT
        && text
            .bytes()
            .all(|byte| byte.is_ascii_graphic() || byte == b' ')
}

pub fn request(id: &str, method: &str, params: Value) -> Result<Vec<u8>, String> {
    if !valid_id(id) || !params.is_object() {
        return Err("Invalid request envelope".into());
    }
    let mut bytes = PREFIX.to_vec();
    bytes.extend(
        serde_json::to_vec(&json!({"id": id, "method": method, "params": params}))
            .map_err(|e| e.to_string())?,
    );
    bytes.push(b'\n');
    if bytes.len() > MAX_FRAME {
        return Err("Request exceeds 1024 bytes".into());
    }
    Ok(bytes)
}

pub fn parse(payload: &[u8]) -> Result<Message, String> {
    // Parsing via a struct first rejects duplicate top-level envelope fields.
    #[derive(Deserialize)]
    struct Envelope {
        id: Option<Value>,
        ok: Option<bool>,
        result: Option<Value>,
        error: Option<DeviceError>,
        event: Option<String>,
        data: Option<Value>,
    }
    let envelope: Envelope =
        serde_json::from_slice(payload).map_err(|e| format!("Invalid protocol JSON: {e}"))?;
    if let Some(event) = envelope.event {
        if envelope.ok.is_some() || envelope.id.is_some() {
            return Err("Ambiguous protocol envelope".into());
        }
        if !matches!(
            event.as_str(),
            "action.started" | "action.completed" | "action.failed" | "idle.error"
        ) {
            return Ok(Message::Unknown);
        }
        let data = envelope.data.ok_or("Missing event data")?;
        if event == "idle.error" {
            return Ok(Message::IdleError(
                serde_json::from_value(data).map_err(|e| e.to_string())?,
            ));
        }
        let request_id = field_string(&data, "requestId")?;
        if !valid_id(&request_id) {
            return Err("Invalid lifecycle requestId".into());
        }
        let action_id = field_string(&data, "actionId")?;
        if !valid_action_id(&action_id) {
            return Err("Invalid lifecycle actionId".into());
        }
        let error = if event == "action.failed" {
            Some(serde_json::from_value(data).map_err(|e| e.to_string())?)
        } else {
            None
        };
        return Ok(Message::Lifecycle {
            event,
            request_id,
            action_id,
            error,
        });
    }
    let id = match envelope.id {
        Some(Value::String(id)) if valid_id(&id) => Some(id),
        None | Some(Value::Null) => None,
        _ => return Err("Invalid response id".into()),
    };
    let result = match envelope.ok {
        Some(true) if id.is_some() && envelope.error.is_none() => {
            Ok(envelope.result.ok_or("Missing result")?)
        }
        Some(false) if envelope.result.is_none() => {
            Err(envelope.error.ok_or("Missing response error")?)
        }
        _ => return Err("Invalid response envelope".into()),
    };
    Ok(Message::Response { id, result })
}

#[derive(Default)]
pub struct Framer {
    buffer: Vec<u8>,
    dropping: bool,
}

impl Framer {
    pub fn push(&mut self, byte: u8) -> Option<Result<Message, String>> {
        if self.dropping {
            if byte == b'\n' {
                self.dropping = false;
            }
            return None;
        }
        if self.buffer.len() >= MAX_FRAME - 1 && byte != b'\n' {
            let protocol = self.buffer.starts_with(PREFIX);
            self.buffer.clear();
            self.dropping = true;
            return protocol.then(|| Err("Protocol frame exceeds 1024 bytes".into()));
        }
        if byte != b'\n' {
            self.buffer.push(byte);
            return None;
        }
        let mut line = std::mem::take(&mut self.buffer);
        if !line.starts_with(PREFIX) {
            return None;
        }
        if line.last() == Some(&b'\r') {
            line.pop();
        }
        Some(parse(&line[PREFIX.len()..]))
    }
}

pub fn validate_hello(value: Value) -> Result<Hello, String> {
    let hello: Hello = serde_json::from_value(value).map_err(|e| e.to_string())?;
    if hello.protocol != 1
        || hello.queue_capacity != 4
        || hello.firmware.is_empty()
        || hello.board.is_empty()
    {
        return Err("Unsupported hello response (requires protocol 1, queueCapacity 4)".into());
    }
    Ok(hello)
}

pub fn append_page(
    catalog: &mut Vec<Action>,
    total: &mut Option<usize>,
    value: Value,
) -> Result<Option<usize>, String> {
    if value.get("nextCursor").is_none() {
        return Err("Catalog page is missing nextCursor".into());
    }
    let page: Page = serde_json::from_value(value).map_err(|e| e.to_string())?;
    let end = catalog
        .len()
        .checked_add(page.actions.len())
        .ok_or("Catalog size overflow")?;
    if page.total > MAX_ACTIONS
        || page.actions.len() > 4
        || end > page.total
        || total.is_some_and(|t| t != page.total)
        || page.next_cursor != (end < page.total).then_some(end)
        || (end < page.total && page.actions.is_empty())
    {
        return Err("Invalid catalog pagination or catalog exceeds 4096 actions".into());
    }
    for (index, action) in page.actions.iter().enumerate() {
        if !valid_action_id(&action.id)
            || action.name.is_empty()
            || action.device.is_empty()
            || action.cancellable
            || catalog.iter().any(|a| a.id == action.id)
            || page.actions[..index].iter().any(|a| a.id == action.id)
        {
            return Err("Invalid or duplicate action descriptor".into());
        }
    }
    *total = Some(page.total);
    catalog.extend(page.actions);
    Ok(page.next_cursor)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn feed(bytes: &[u8]) -> Vec<Result<Message, String>> {
        let mut framer = Framer::default();
        bytes.iter().filter_map(|b| framer.push(*b)).collect()
    }

    #[test]
    fn fragmented_crlf_and_boot_logs() {
        let messages = feed(b"boot\xff log\nTOKKI/1 {\"id\":\"1\",\"ok\":true,\"result\":{}}\r\nother\nTOKKI/1 {\"event\":\"future.event\"}\n");
        assert_eq!(messages.len(), 2);
        assert!(matches!(&messages[0], Ok(Message::Response { id: Some(id), .. }) if id == "1"));
        assert!(matches!(&messages[1], Ok(Message::Unknown)));
    }

    #[test]
    fn frame_limit_includes_prefix_and_newline_and_recovers() {
        let mut exact = b"TOKKI/1 {\"event\":\"future\",\"padding\":\"".to_vec();
        exact.resize(MAX_FRAME - 3, b'x');
        exact.extend(b"\"}\n");
        assert_eq!(exact.len(), MAX_FRAME);
        assert!(feed(&exact)[0].is_ok());
        exact.insert(MAX_FRAME - 3, b'x');
        exact.extend(b"TOKKI/1 {\"event\":\"future\"}\n");
        let messages = feed(&exact);
        assert!(messages[0].is_err());
        assert!(messages[1].is_ok());
        assert!(feed(&vec![b'x'; MAX_FRAME * 10]).is_empty());
    }

    #[test]
    fn malformed_envelopes_rejected() {
        for value in [
            r#"{"id":"","ok":true,"result":{}}"#,
            r#"{"id":"a","ok":"true","result":{}}"#,
            r#"{"id":"a","ok":true}"#,
            r#"{"id":"a","ok":false,"error":{}}"#,
            r#"{"id":"a","id":"b","ok":true,"result":{}}"#,
            r#"{"id":"a","ok":true,"result":{},"event":"future"}"#,
            r#"{"event":"action.failed","data":{"requestId":"a","actionId":"x"}}"#,
            r#"{"event":"action.started","data":{"requestId":3,"actionId":"x"}}"#,
            "[]",
            "{}",
        ] {
            assert!(parse(value.as_bytes()).is_err(), "{value}");
        }
        assert!(parse(b"\xff").is_err());
    }

    #[test]
    fn error_and_lifecycle_and_unknown_fields() {
        assert!(matches!(
            parse(br#"{"id":null,"ok":false,"error":{"code":"invalid_params","message":"bad"}}"#)
                .unwrap(),
            Message::Response {
                id: None,
                result: Err(_)
            }
        ));
        assert!(matches!(parse(br#"{"event":"action.failed","data":{"requestId":"a","actionId":"x","code":"driver_error","message":"ESP_FAIL","extra":1}}"#).unwrap(), Message::Lifecycle { error: Some(_), .. }));
    }

    #[test]
    fn requests_are_bounded_and_correlated() {
        let bytes = request("a", "hello", json!({})).unwrap();
        assert!(bytes.starts_with(PREFIX) && bytes.ends_with(b"\n"));
        assert!(request("", "hello", json!({})).is_err());
        assert!(request("é", "hello", json!({})).is_err());
        assert!(request(&"a".repeat(65), "hello", json!({})).is_err());
        assert!(request("a", "hello", json!({"huge": "x".repeat(1024)})).is_err());
        assert!(request("a", "hello", json!([])).is_err());
        assert!(request("Az09_.:-", "hello", json!({})).is_ok());
        for invalid in ["has space", "a\nb", "a\0b", "a/b", "a\"b"] {
            assert!(request(invalid, "hello", json!({})).is_err());
        }
        assert!(valid_marquee_text("Teams: Build 42!"));
        assert!(valid_marquee_text(&"x".repeat(40)));
        for invalid in ["", "line\nbreak", "café", &"x".repeat(41)] {
            assert!(!valid_marquee_text(invalid));
        }
    }

    fn descriptor(id: &str) -> Value {
        json!({"id":id,"name":id,"device":"future-device","cancellable":false})
    }

    #[test]
    fn catalog_is_dynamic_and_paginated_atomically() {
        let mut catalog = Vec::new();
        let mut total = None;
        assert_eq!(
            append_page(
                &mut catalog,
                &mut total,
                json!({"actions":[descriptor("a")],"total":2,"nextCursor":1})
            )
            .unwrap(),
            Some(1)
        );
        assert_eq!(
            append_page(
                &mut catalog,
                &mut total,
                json!({"actions":[descriptor("b")],"total":2,"nextCursor":null})
            )
            .unwrap(),
            None
        );
        assert_eq!(catalog.len(), 2);
    }

    #[test]
    fn bad_catalog_pages_rejected_without_partial_changes() {
        for page in [
            json!({"actions":[],"total":2,"nextCursor":0}),
            json!({"actions":[descriptor("a")],"total":2,"nextCursor":null}),
            json!({"actions":[descriptor("a")],"total":2,"nextCursor":2}),
            json!({"actions":[descriptor("a"),descriptor("a")],"total":2,"nextCursor":null}),
            json!({"actions":[],"total":4097,"nextCursor":null}),
            json!({"actions":vec![descriptor("a"); 5],"total":5,"nextCursor":null}),
            json!({"actions":[descriptor(&"a".repeat(97))],"total":1,"nextCursor":null}),
            json!({"actions":[descriptor("nul\0id")],"total":1,"nextCursor":null}),
        ] {
            let mut catalog = Vec::new();
            assert!(append_page(&mut catalog, &mut None, page).is_err());
            assert!(catalog.is_empty());
        }
    }

    #[test]
    fn hello_requires_supported_protocol() {
        let valid =
            json!({"protocol":1,"firmware":"0.2.0","board":"board","ready":true,"queueCapacity":4});
        assert!(validate_hello(valid.clone()).is_ok());
        let mut invalid = valid;
        invalid["protocol"] = json!(2);
        assert!(validate_hello(invalid).is_err());
    }

    #[test]
    fn c_firmware_transcript_conforms_to_rust_decoder() {
        let fixture = include_bytes!("../tests/fixtures/firmware-frames.ndjson");
        assert!(fixture.ends_with(b"\n"));
        for frame in fixture.split_inclusive(|byte| *byte == b'\n') {
            assert!(frame.starts_with(PREFIX));
            assert!(frame.len() <= MAX_FRAME);
        }
        let mut messages = feed(fixture).into_iter().map(Result::unwrap);
        let Message::Response {
            id,
            result: Ok(value),
        } = messages.next().unwrap()
        else {
            panic!("Expected firmware hello");
        };
        assert_eq!(id.as_deref(), Some("fixture-hello"));
        let hello = validate_hello(value).unwrap();
        assert_eq!(hello.protocol, 1);
        assert_eq!(hello.firmware, "0.2.0");
        assert_eq!(hello.board, "adafruit_feather_esp32_v2");
        assert!(hello.ready);
        assert_eq!(hello.queue_capacity, 4);

        let mut catalog = Vec::new();
        let mut total = None;
        for cursor in (0..31).step_by(4) {
            let Message::Response {
                id,
                result: Ok(value),
            } = messages.next().unwrap()
            else {
                panic!("Expected firmware catalog page");
            };
            assert_eq!(id, Some(format!("fixture-page-{cursor}")));
            let next = append_page(&mut catalog, &mut total, value).unwrap();
            assert_eq!(next, (cursor + 4 < 31).then_some(cursor + 4));
        }
        assert_eq!(total, Some(31));
        let expected_ids = [
            "led.blink",
            "neopixel.rainbow",
            "neopixel.blink_red",
            "neopixel.blink_yellow",
            "neopixel.blink_green",
            "neopixel.breathe_teal",
            "neopixel.pulse_blue",
            "oled.happy",
            "oled.sad",
            "oled.surprised",
            "oled.blink",
            "oled.curious",
            "oled.drink_water",
            "oled.water_drop",
            "oled.fire",
            "oled.wink",
            "oled.checkmark",
            "oled.thinking",
            "oled.look_left",
            "oled.look_right",
            "oled.look_up",
            "oled.look_down",
            "oled.sleepy",
            "oled.heart",
            "oled.exclamation",
            "speaker.drink_water",
            "speaker.chirp",
            "speaker.alert",
            "speaker.chime",
            "speaker.ping",
            "speaker.dog_bark",
        ];
        assert_eq!(
            catalog
                .iter()
                .map(|action| action.id.as_str())
                .collect::<Vec<_>>(),
            expected_ids
        );
        assert!(catalog.iter().all(|action| !action.cancellable
            && !action.name.is_empty()
            && !action.device.is_empty()));

        for (expected_request, terminal) in [
            ("fixture-run", "action.completed"),
            ("fixture-fail", "action.failed"),
        ] {
            let Message::Response {
                id,
                result: Ok(value),
            } = messages.next().unwrap()
            else {
                panic!("Expected action acceptance");
            };
            assert_eq!(id.as_deref(), Some(expected_request));
            assert_eq!(value, json!({"accepted":true}));
            for expected_event in ["action.started", terminal] {
                let Message::Lifecycle {
                    event,
                    request_id,
                    action_id,
                    error,
                } = messages.next().unwrap()
                else {
                    panic!("Expected lifecycle event");
                };
                assert_eq!(event, expected_event);
                assert_eq!(request_id, expected_request);
                assert_eq!(action_id, "oled.blink");
                if expected_event == "action.failed" {
                    let error = error.unwrap();
                    assert_eq!(error.code, "driver_error");
                    assert_eq!(error.message, "ESP_FAIL");
                } else {
                    assert!(error.is_none());
                }
            }
        }
        let Message::IdleError(error) = messages.next().unwrap() else {
            panic!("Expected idle driver error");
        };
        assert_eq!(error.code, "driver_error");
        assert_eq!(error.message, "ESP_FAIL");
        let Message::Response {
            id,
            result: Err(error),
        } = messages.next().unwrap()
        else {
            panic!("Expected action_not_found response");
        };
        assert_eq!(id.as_deref(), Some("fixture-missing"));
        assert_eq!(error.code, "action_not_found");
        assert_eq!(error.message, "Unknown action");
        assert!(messages.next().is_none());
    }

    #[test]
    fn firmware_hello_frame_does_not_claim_peripheral_health() {
        let bytes = b"I (42) tokki: board initialized\r\nTOKKI/1 {\"id\":\"pc-1\",\"ok\":true,\"result\":{\"protocol\":1,\"firmware\":\"0.2.0\",\"board\":\"adafruit_feather_esp32_v2\",\"ready\":true,\"queueCapacity\":4}}\n";
        let mut messages = feed(bytes);
        assert_eq!(messages.len(), 1);
        let Message::Response {
            result: Ok(result), ..
        } = messages.remove(0).unwrap()
        else {
            panic!("Expected hello response");
        };
        let hello = validate_hello(result).unwrap();
        assert!(hello.ready);
        assert_eq!(hello.board, "adafruit_feather_esp32_v2");
        assert_eq!(hello.firmware, "0.2.0");
        let failure = feed(b"TOKKI/1 {\"event\":\"action.failed\",\"data\":{\"requestId\":\"pc-2\",\"actionId\":\"oled.blink\",\"code\":\"driver_error\",\"message\":\"ESP_ERR_TIMEOUT\"}}\n");
        assert!(
            matches!(&failure[0], Ok(Message::Lifecycle { error: Some(error), .. }) if error.message == "ESP_ERR_TIMEOUT")
        );
    }

    #[test]
    fn thirty_descriptors_eight_wire_pages_and_utf8_survive_fragmentation() {
        let mut catalog = Vec::new();
        let mut total = None;
        let mut framer = Framer::default();
        for page in 0..8 {
            let cursor = page * 4;
            let end = (cursor + 4).min(30);
            let actions: Vec<_> = (cursor..end)
                .map(|index| {
                    json!({
                        "id":format!("fixture.action.{index}"),"name":format!("Gesture 눈 {index}"),
                        "device":"fixture","cancellable":false
                    })
                })
                .collect();
            let response = json!({"id":format!("page-{page}"),"ok":true,"result":{
                "actions":actions,"total":30,"nextCursor":(end < 30).then_some(end)
            }});
            let line = format!("TOKKI/1 {response}\r\n");
            assert!(line.len() <= MAX_FRAME);
            let mut messages = Vec::new();
            for chunk in line.as_bytes().chunks(3) {
                messages.extend(chunk.iter().filter_map(|byte| framer.push(*byte)));
            }
            assert_eq!(messages.len(), 1);
            let Message::Response {
                id,
                result: Ok(result),
            } = messages.remove(0).unwrap()
            else {
                panic!("Expected catalog response");
            };
            assert_eq!(id, Some(format!("page-{page}")));
            assert_eq!(
                append_page(&mut catalog, &mut total, result).unwrap(),
                (end < 30).then_some(end)
            );
        }
        assert_eq!(catalog.len(), 30);
        assert_eq!(total, Some(30));
        assert!(catalog[29].name.contains('눈'));
    }
}
