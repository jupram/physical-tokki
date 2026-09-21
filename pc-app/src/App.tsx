import {
  Bell,
  CalendarClock,
  Cable,
  CircleStop,
  Gauge,
  Layers,
  Mail,
  MessageSquare,
  Pencil,
  Play,
  Plus,
  Radio,
  RefreshCw,
  Save,
  Sparkles,
  Trash2,
  X,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { actionCapacity, client, emptySnapshot, isInFlight, queueSummary, type NotificationSnapshot, type Port, type QueuedNotification, type Snapshot } from "./native";
import "./App.css";
import { PetConnectionBadge, PetWelcome } from "./PetDesk";
import {
  CATALOG,
  CATALOG_BY_ID,
} from "./gestures/catalog";
import type { Bundle, GestureDef } from "./gestures/catalog";
import { BundleComposer } from "./gestures/BundleComposer";
import { MediumStudio } from "./gestures/MediumStudio";
import { GesturePreview, IDLE_PREVIEW } from "./gestures/GesturePreview";
import type { PreviewState } from "./gestures/GesturePreview";
import { playBundle } from "./gestures/player";
import type { LaneGestures } from "./gestures/player";
import { playSound } from "./gestures/audio";
import type { SpeakerSound } from "./gestures/catalog";

type View = "device" | "gestures" | "bundles" | "events";

const BUNDLE_STORAGE_KEY = "tokki.bundles";
const EMPTY_NOTIFICATION_SNAPSHOT: NotificationSnapshot = {
  supported: false,
  permission: "unknown",
  enabled: false,
  pending: 0,
  queued: [],
  lastEvent: null,
  lastError: null,
};

const NOTIFICATION_PRESETS: QueuedNotification[] = [
  { source: "Microsoft Teams", text: "Teams: Alex sent a new message", sound: "trill", color: "purple" },
  { source: "Outlook mail", text: "Mail: Project update received", sound: "chime", color: "blue" },
  { source: "Outlook meeting reminder", text: "Meeting: Design review starts soon", sound: "whistle", color: "yellow" },
];

const NOTIFICATION_COLORS = { blue: "#1858ff", purple: "#a72cff", yellow: "#ffd21a" };
const SOUND_DURATIONS = { trill: 260, chime: 640, whistle: 280 };

function notificationDuration(text: string) {
  const width = text.length * 12 - 2;
  return Math.ceil((128 + width) / 2) * 45;
}

function loadBundles(): Bundle[] {
  if (typeof window === "undefined") return [];
  try {
    const raw = window.localStorage.getItem(BUNDLE_STORAGE_KEY);
    return raw ? (JSON.parse(raw) as Bundle[]) : [];
  } catch {
    return [];
  }
}

function persist<T>(key: string, value: T) {
  try {
    window.localStorage.setItem(key, JSON.stringify(value));
  } catch {
    // storage unavailable
  }
}

type EventRule = {
  id: string;
  name: string;
  trigger: string;
  bundleId: string | null;
  enabled: boolean;
};

const EVENTS_STORAGE_KEY = "tokki.events";

function loadEvents(): EventRule[] {
  if (typeof window === "undefined") return [];
  try {
    const raw = window.localStorage.getItem(EVENTS_STORAGE_KEY);
    if (raw) {
      return (JSON.parse(raw) as EventRule[]).map((event) =>
        event.id === "event.hydration" && event.trigger === "Every 1 hour"
          ? { ...event, trigger: "Every 5 mins" }
          : event);
    }
  } catch {
    // fall through to seed
  }
  return [
    { id: "event.hydration", name: "Hydration reminder", trigger: "Every 5 mins", bundleId: null, enabled: true },
  ];
}

function App() {
  const [view, setView] = useState<View>("device");
  const [snapshot, setSnapshot] = useState<Snapshot>(emptySnapshot);
  const [ports, setPorts] = useState<Port[]>([]);
  const [selectedPort, setSelectedPort] = useState("");
  const [localError, setLocalError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [filter, setFilter] = useState("");
  const [notificationRelay, setNotificationRelay] = useState<NotificationSnapshot>(EMPTY_NOTIFICATION_SNAPSHOT);

  async function refreshPorts() {
    setScanning(true);
    try {
      const found = await client.ports();
      setPorts(found);
      setSelectedPort((current) => found.some((port) => port.name === current) ? current : (found[0]?.name ?? ""));
      setLocalError(null);
    } catch (error) { setLocalError(String(error)); }
    finally { setScanning(false); }
  }

  useEffect(() => {
    if (!client.native) return;
    void refreshPorts();
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    async function poll() {
      try {
        const next = await client.snapshot();
        if (!stopped) setSnapshot((previous) => next.revision >= previous.revision ? next : previous);
      } catch (error) {
        if (!stopped) setLocalError(`Native status unavailable: ${String(error)}`);
      }
      // Polling only reads durable backend state; it never manufactures lifecycle transitions.
      if (!stopped) timer = setTimeout(poll, 250);
    }
    void poll();
    return () => { stopped = true; clearTimeout(timer); };
  }, []);

  useEffect(() => {
    if (!client.native) return;
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    async function pollNotifications() {
      try {
        const next = await client.notificationSnapshot();
        if (!stopped) setNotificationRelay(next);
      } catch (error) {
        if (!stopped) setLocalError(`Notification status unavailable: ${String(error)}`);
      }
      if (!stopped) timer = setTimeout(pollNotifications, 750);
    }
    void pollNotifications();
    return () => { stopped = true; clearTimeout(timer); };
  }, []);

  async function command(operation: () => Promise<void>) {
    setBusy(true);
    setLocalError(null);
    try { await operation(); }
    catch (error) { setLocalError(String(error)); }
    finally { setBusy(false); }
  }

  async function requestNotificationAccess() {
    setBusy(true);
    setLocalError(null);
    try { setNotificationRelay(await client.requestNotificationAccess()); }
    catch (error) { setLocalError(String(error)); }
    finally { setBusy(false); }
  }

  async function toggleNotificationRelay() {
    const enabled = !notificationRelay.enabled;
    await command(async () => {
      await client.setNotificationRelay(enabled);
      setNotificationRelay((current) => ({ ...current, enabled }));
    });
  }

  const connected = snapshot.status === "connected";
  const connecting = snapshot.status === "connecting" || snapshot.status === "loading";
  const ownsPort = connected || connecting;
  const queue = queueSummary(snapshot.activity);
  const queueCapacity = actionCapacity(snapshot);
  const visibleActions = snapshot.actions.filter((action) =>
    `${action.id} ${action.name} ${action.device}`.toLowerCase().includes(filter.toLowerCase()));
  const error = localError ?? snapshot.lastError;
  const statusText = !client.native ? "Native app required" : ({
    disconnected: "Disconnected", connecting: "Identifying device…", loading: "Loading catalog…",
    connected: "Connected", error: "Connection failed",
  }[snapshot.status] ?? snapshot.status);
  const [preview, setPreview] = useState<PreviewState>(IDLE_PREVIEW);
  const [playingBundle, setPlayingBundle] = useState<string | null>(null);
  const [bundles, setBundles] = useState<Bundle[]>(loadBundles);
  const [selectedBundleId, setSelectedBundleId] = useState<string>("");
  const [editingBundleId, setEditingBundleId] = useState<string>("");
  const [events, setEvents] = useState<EventRule[]>(loadEvents);
  const [eventDraft, setEventDraft] = useState<EventRule | null>(null);
  const [simulationStatus, setSimulationStatus] = useState("Local preview ready");
  const [previewCycle, setPreviewCycle] = useState(0);
  const stopRef = useRef<(() => void) | null>(null);

  function previewOnDevice(gesture: GestureDef) {
    if (!connected) return;
    void client.run(gesture.id).catch((error) => {
      setLocalError(`Could not preview ${gesture.name} on the device: ${String(error)}`);
    });
  }

  function stopPreview() {
    stopRef.current?.();
    stopRef.current = null;
    setPlayingBundle(null);
    setPreview(IDLE_PREVIEW);
  }

  function playSavedBundle(bundle: Bundle) {
    stopPreview();
    setPlayingBundle(bundle.id);
    setSimulationStatus(`Playing ${bundle.name}`);
    const lanes: LaneGestures = {
      oled: bundle.lanes.oled.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
      led: bundle.lanes.led.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
      speaker: bundle.lanes.speaker.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
    };
    stopRef.current = playBundle(
      lanes,
      (updater) => setPreview((prev) => updater(prev)),
      () => {
        setPlayingBundle(null);
        setPreview(IDLE_PREVIEW);
        setSimulationStatus("Local preview ready");
        stopRef.current = null;
      },
      previewOnDevice,
    );
  }

  function playNotificationPreview(item: QueuedNotification, id: string) {
    stopPreview();
    const sound = item.sound as SpeakerSound;
    const duration = notificationDuration(item.text);
    const speakerDuration = SOUND_DURATIONS[item.sound as keyof typeof SOUND_DURATIONS] ?? 500;
    const playingId = `notification:${id}`;
    setPlayingBundle(playingId);
    setPreviewCycle((cycle) => cycle + 1);
    setSimulationStatus(`Previewing ${item.source}`);
    setPreview({
      oled: { sim: { kind: "marquee", text: item.text }, label: item.text, active: true },
      led: { sim: { effect: "solid", color: NOTIFICATION_COLORS[item.color] }, label: `${item.color} notification`, active: true },
      speaker: { sim: { sound }, label: item.sound, active: true },
    });
    const stopSound = playSound(sound);
    const speakerTimer = window.setTimeout(() => {
      setPreview((current) => ({ ...current, speaker: { ...current.speaker, active: false } }));
    }, speakerDuration);
    const finishTimer = window.setTimeout(() => {
      setPlayingBundle(null);
      setSimulationStatus("Local preview ready");
      setPreview(IDLE_PREVIEW);
      stopRef.current = null;
    }, duration);
    stopRef.current = () => {
      window.clearTimeout(speakerTimer);
      window.clearTimeout(finishTimer);
      stopSound();
    };
  }

  function saveBundle(bundle: Bundle) {
    setBundles((current) => {
      const exists = current.some((entry) => entry.id === bundle.id);
      const next = exists
        ? current.map((entry) => (entry.id === bundle.id ? bundle : entry))
        : [...current, bundle];
      persist(BUNDLE_STORAGE_KEY, next);
      return next;
    });
    setSelectedBundleId(bundle.id);
    setEditingBundleId("");
    setSimulationStatus(`Saved ${bundle.name}`);
  }

  function deleteBundle(id: string) {
    if (playingBundle === id) stopPreview();
    setBundles((current) => {
      const next = current.filter((bundle) => bundle.id !== id);
      persist(BUNDLE_STORAGE_KEY, next);
      setSelectedBundleId((selected) => (selected === id ? next[0]?.id ?? "" : selected));
      return next;
    });
    if (editingBundleId === id) setEditingBundleId("");
  }

  function persistEvents(next: EventRule[]) {
    persist(EVENTS_STORAGE_KEY, next);
  }

  function newEvent() {
    setEventDraft({
      id: `event.${Date.now().toString(36)}`,
      name: "",
      trigger: "",
      bundleId: null,
      enabled: true,
    });
  }

  function saveEvent() {
    if (!eventDraft || !eventDraft.name.trim()) return;
    const draft = { ...eventDraft, name: eventDraft.name.trim(), trigger: eventDraft.trigger.trim() };
    setEvents((current) => {
      const exists = current.some((entry) => entry.id === draft.id);
      const next = exists
        ? current.map((entry) => (entry.id === draft.id ? draft : entry))
        : [...current, draft];
      persistEvents(next);
      return next;
    });
    setEventDraft(null);
  }

  function updateEvent(id: string, patch: Partial<EventRule>) {
    setEvents((current) => {
      const next = current.map((entry) => (entry.id === id ? { ...entry, ...patch } : entry));
      persistEvents(next);
      return next;
    });
  }

  function deleteEvent(id: string) {
    setEvents((current) => {
      const next = current.filter((entry) => entry.id !== id);
      persistEvents(next);
      return next;
    });
    if (eventDraft?.id === id) setEventDraft(null);
  }

  function runEvent(event: EventRule) {
    const bundle = bundles.find((entry) => entry.id === event.bundleId);
    if (!bundle) {
      setSimulationStatus(`Assign a bundle to \u201c${event.name}\u201d first`);
      return;
    }
    playSavedBundle(bundle);
  }

  const selectedBundle = bundles.find((bundle) => bundle.id === selectedBundleId) ?? bundles[0];
  const editingBundle = bundles.find((bundle) => bundle.id === editingBundleId) ?? null;

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark" aria-hidden="true"><span /><span /></span>
          <div><strong>Physical Tokki</strong><small>Pet control desk</small></div>
        </div>
        <nav aria-label="Main navigation">
          <button className={`nav-item ${view === "device" ? "active" : ""}`} aria-current={view === "device" ? "page" : undefined} onClick={() => setView("device")}><Gauge size={18} /> Device</button>
          <button className={`nav-item ${view === "gestures" ? "active" : ""}`} aria-current={view === "gestures" ? "page" : undefined} onClick={() => setView("gestures")}><Sparkles size={18} /> Gestures <span className="nav-count">{snapshot.actions.length}</span></button>
          <button className={view === "bundles" ? "nav-item active" : "nav-item"} aria-current={view === "bundles" ? "page" : undefined} onClick={() => setView("bundles")}>
            <Layers size={18} /> Bundles <span className="nav-count">{bundles.length}</span>
          </button>
          <button className={`nav-item ${view === "events" ? "active" : ""}`} aria-current={view === "events" ? "page" : undefined} onClick={() => setView("events")}><Bell size={18} /> Events <span className="nav-count">{events.length}</span></button>
        </nav>
        <div className={`sidebar-status ${connected ? "is-connected" : "is-disconnected"}`}>
          <Radio size={18} />
          <div><strong>{statusText}</strong><small>{snapshot.port ?? "USB serial · 115200 8N1"}</small></div>
        </div>
      </aside>

      <main>
        <header className="topbar">
          <div><span className="eyebrow">YOUR COMPANION'S CORNER</span><h1>{view === "device" ? "Tokki's desk" : view === "gestures" ? "The playground" : view === "bundles" ? "Mix a little magic" : "Little rituals"}</h1></div>
          <div className="topbar-actions">
            {ownsPort && <button className="connection-button" disabled={busy} onClick={() => void command(() => client.disconnect())}><CircleStop size={17} /> Disconnect</button>}
            <PetConnectionBadge status={snapshot.status} native={client.native} port={snapshot.port} onOpen={() => setView("device")} />
          </div>
        </header>
        <div className="workspace">
          {!client.native && <div className="notice browser-notice" role="status"><Cable size={20} aria-hidden="true" /><div><strong>You're exploring in the browser</strong><p>Local previews work here. To connect the real Tokki over USB, open the desktop app with <code>npm run tauri dev</code>. This preview does not simulate a device connection.</p></div></div>}
          {error && <div className="notice error" role="alert"><strong>Operation failed</strong><p>{error}</p></div>}

          {view === "device" && (
            <>
            <PetWelcome connected={connected} onExplore={() => setView("gestures")} onCompose={() => setView("bundles")} onEvents={() => setView("events")} />
            <section aria-labelledby="device-heading">
              <div className="section-heading"><div><h2 id="device-heading">{connected ? "Tokki is here" : "Bring Tokki to your desk"}</h2><p>Choose your pet's USB port to say hello. We'll only connect when you ask.</p></div><span className={`state-pill ${connected ? "online" : "offline"}`}>{statusText}</span></div>
              <div className="connection-panel">
                <label htmlFor="serial-port">Serial port</label>
                <div className="connection-controls">
                  <select id="serial-port" value={selectedPort} onChange={(event) => setSelectedPort(event.target.value)} disabled={!client.native || ownsPort || scanning}>
                    {ports.length === 0 && <option value="">No serial ports found</option>}
                    {ports.map((port) => <option key={port.name} value={port.name}>{port.name} — {port.description}</option>)}
                  </select>
                  <button className="connection-button" disabled={!client.native || scanning || busy} onClick={() => void refreshPorts()}><RefreshCw size={16} /> {scanning ? "Scanning…" : "Rescan"}</button>
                  <button className="primary-button" disabled={!client.native || !selectedPort || ownsPort || busy} onClick={() => void command(() => client.connect(selectedPort))}><Cable size={16} /> Connect</button>
                </div>
                <p>Close ESP-IDF Monitor and other serial apps first. DTR / RTS are deasserted to avoid resetting the ESP32.</p>
              </div>
              <div className="metric-grid">
                <article><span>Firmware</span><strong>{snapshot.hello?.firmware ?? "—"}</strong><small>{snapshot.hello?.board ?? "Read from device after connection"}</small></article>
                <article><span>Protocol</span><strong>{snapshot.hello ? `v${snapshot.hello.protocol}` : "—"}</strong><small>TOKKI/1 · newline JSON</small></article>
                <article><span>Gestures</span><strong>{snapshot.actions.length}</strong><small>{connected ? "All catalog pages loaded" : "Waiting for device catalog"}</small></article>
              </div>
              <div className="activity-strip" role="status"><Radio size={18} /><span>{statusText}{snapshot.port ? ` · ${snapshot.port}` : ""}{snapshot.hello && !snapshot.hello.ready ? " · Firmware not ready yet" : ""}</span></div>
              <p className="help-text">Gestures for separate devices run in parallel; each device remains serial, with up to four additional actions waiting globally. Gestures are finite and cannot be cancelled. Disconnecting does not stop accepted gestures. When the OLED queue drains, the pet resumes idle behavior.</p>
              <p className="help-text">Connection readiness covers board, LED, and RGB startup only. OLED and speaker initialize on use; their driver failures are reported in Activity.</p>
              <button className="connection-button catalog-link" disabled={!connected} onClick={() => setView("gestures")}><Sparkles size={16} /> Browse device gestures</button>
            </section>
            </>
          )}

          {view === "gestures" && (
            <section aria-labelledby="gestures-heading">
              <div className="section-heading"><div><h2 id="gestures-heading">A mood for every moment</h2><p>Expressions, little lights, and sounds discovered from your connected pet.</p></div><button className="connection-button" disabled={!connected || busy} onClick={() => void command(() => client.refresh())}><RefreshCw size={16} /> Refresh catalog</button></div>
              <label className="search-label" htmlFor="gesture-filter">Find a gesture</label>
              <input id="gesture-filter" type="search" placeholder="Search name, device, or ID" value={filter} onChange={(event) => setFilter(event.target.value)} />
              <p className="help-text catalog-help">Send queues a real gesture on the pet. Preview plays on this PC and, while connected, on the pet. No cancellation or automatic retries.</p>
              <div className="action-list">
                {visibleActions.map((action) => (
                  <article className="action-row" key={action.id}>
                    <span className="device-icon"><Sparkles size={19} /></span>
                    <div className="action-name"><strong>{action.name}</strong><code>{action.id}</code></div>
                    <span className="action-device">{action.device}</span>
                    <button className="connection-button" title={`Send ${action.name}`} aria-label={`Send ${action.name}`} disabled={!connected || busy || queue.inFlight >= queueCapacity} onClick={() => void command(() => client.run(action.id))}><Play size={16} /> Send</button>
                  </article>
                ))}
                {visibleActions.length === 0 && <div className="empty-state">{connecting ? "Fetching all catalog pages…" : connected ? "No gestures match this view." : "Connect on the Device screen to discover gestures."}</div>}
              </div>
              <div className="section-heading preview-heading">
                <div><h2>Try a little personality</h2><p>{CATALOG.length} local previews to explore on this PC{connected ? " and the connected pet" : ". Connect Tokki to play them on the pet, too"}.</p></div>
                <span className="state-pill"><Radio size={13} /> {connected ? "PC + device" : "PC only"}</span>
              </div>
              <MediumStudio medium="oled" onPreviewGesture={previewOnDevice} />
              <MediumStudio medium="led" onPreviewGesture={previewOnDevice} />
              <MediumStudio medium="speaker" onPreviewGesture={previewOnDevice} />
            </section>
          )}

          {view === "bundles" && (
            <>
              <BundleComposer
                key={editingBundle?.id ?? "new"}
                initial={editingBundle}
                onSave={saveBundle}
                onCancelEdit={() => setEditingBundleId("")}
                onPreviewGesture={previewOnDevice}
              />
              <div className="activity-strip" role="status">
                <Radio size={18} /><span>{simulationStatus}</span>
              </div>
              <div className="saved-bundles">
                <div className="section-heading">
                  <div><h2>Saved bundles</h2><p>Stored on this PC</p></div>
                  <span className="state-pill">{bundles.length} stored</span>
                </div>
                {selectedBundle ? (
                  <>
                    <div className="bundle-picker">
                      <select
                        className="medium-select"
                        value={selectedBundle.id}
                        onChange={(event) => { stopPreview(); setSelectedBundleId(event.target.value); }}
                        aria-label="Saved bundle"
                      >
                        {bundles.map((bundle) => (
                          <option key={bundle.id} value={bundle.id}>{bundle.name}</option>
                        ))}
                      </select>
                      <span className="bundle-counts">
                        OLED {selectedBundle.lanes.oled.length} · LED {selectedBundle.lanes.led.length} · Speaker {selectedBundle.lanes.speaker.length}
                      </span>
                      <button
                        className={playingBundle === selectedBundle.id ? "primary-button danger" : "primary-button"}
                        onClick={() => (playingBundle === selectedBundle.id ? stopPreview() : playSavedBundle(selectedBundle))}
                        type="button"
                      >
                        {playingBundle === selectedBundle.id ? <CircleStop size={15} /> : <Play size={15} fill="currentColor" />}
                        {playingBundle === selectedBundle.id ? "Stop" : "Play"}
                      </button>
                      <button className="icon-button" title="Edit bundle" onClick={() => setEditingBundleId(selectedBundle.id)}>
                        <Pencil size={16} />
                      </button>
                      <button className="icon-button" title="Delete bundle" onClick={() => deleteBundle(selectedBundle.id)}>
                        <Trash2 size={16} />
                      </button>
                    </div>
                    <div className="gesture-preview-stage"><GesturePreview state={preview} /></div>
                  </>
                ) : (
                  <p className="composer-hint">No bundles yet — compose one above and save it.</p>
                )}
              </div>
            </>
          )}

          {view === "events" && (
            <section aria-labelledby="events-heading">
              <div className="section-heading notification-heading">
                <div><h2>Windows notification relay</h2><p>Access is local to this app and can be revoked in Windows Settings</p></div>
                {notificationRelay.permission === "allowed" ? (
                  <button
                    className={notificationRelay.enabled ? "toggle enabled" : "toggle"}
                    role="switch"
                    aria-checked={notificationRelay.enabled}
                    aria-label="Enable Windows notification relay"
                    disabled={busy}
                    onClick={() => void toggleNotificationRelay()}
                  ><span /></button>
                ) : (
                  <button className="primary-button" disabled={busy || !client.native} onClick={() => void requestNotificationAccess()}>
                    <Bell size={17} /> Allow access
                  </button>
                )}
              </div>
              <div className="relay-status" role="status">
                <span className={`readiness ${notificationRelay.permission === "allowed" ? "ready" : "pending"}`}>
                  {notificationRelay.permission === "allowed" ? (notificationRelay.enabled ? "Listening" : "Paused") : notificationRelay.permission}
                </span>
                <span>{notificationRelay.pending} queued</span>
                <span>{notificationRelay.lastEvent ?? "No notification relayed this session"}</span>
              </div>
              {notificationRelay.lastError && <p className="error-banner">{notificationRelay.lastError}</p>}
              <div className="notification-rules" aria-label="Notification routing rules">
                {NOTIFICATION_PRESETS.map((item, index) => {
                  const Icon = index === 0 ? MessageSquare : index === 1 ? Mail : CalendarClock;
                  const id = `rule-${index}`;
                  const playing = playingBundle === `notification:${id}`;
                  return (
                    <div className="integration-row" key={item.source}>
                      <span className={`rule-icon ${index === 1 ? "mail" : ""}`}><Icon size={19} /></span>
                      <div><strong>{item.source}</strong><small>OLED marquee · {item.sound} · {item.color} light</small></div>
                      <button className="icon-button" title={`Preview ${item.source}`} aria-label={`Preview ${item.source}`} onClick={() => (playing ? stopPreview() : playNotificationPreview(item, id))}>
                        {playing ? <CircleStop size={17} /> : <Play size={17} fill="currentColor" />}
                      </button>
                    </div>
                  );
                })}
              </div>

              <div className="notification-preview-heading">
                <div><strong>Local simulation</strong><small>{simulationStatus}</small></div>
                {playingBundle?.startsWith("notification:") && <button className="icon-button" title="Stop preview" onClick={stopPreview}><CircleStop size={17} /></button>}
              </div>
              <div className="gesture-preview-stage"><GesturePreview key={previewCycle} state={preview} /></div>

              <div className="section-heading queue-heading">
                <div><h2>Queued notifications</h2><p>Waiting for an available device slot, oldest first</p></div>
                <span className="state-pill">{notificationRelay.queued.length} of 5</span>
              </div>
              {notificationRelay.queued.length === 0 ? (
                <div className="empty-state notification-empty">No notifications are waiting.</div>
              ) : (
                <ol className="notification-queue">
                  {notificationRelay.queued.map((item, index) => {
                    const id = `queue-${index}`;
                    const playing = playingBundle === `notification:${id}`;
                    return (
                      <li className="notification-queue-row" key={`${item.source}-${item.text}-${index}`}>
                        <span className="queue-position">{index + 1}</span>
                        <div><strong>{item.source}</strong><p>{item.text}</p></div>
                        <span className="notification-medium"><i style={{ background: NOTIFICATION_COLORS[item.color] }} />{item.color} · {item.sound}</span>
                        <button className="icon-button" title="Preview queued notification" aria-label={`Preview queued notification ${index + 1}`} onClick={() => (playing ? stopPreview() : playNotificationPreview(item, id))}>
                          {playing ? <CircleStop size={17} /> : <Play size={17} fill="currentColor" />}
                        </button>
                      </li>
                    );
                  })}
                </ol>
              )}

              <div className="section-heading">
                <div><h2 id="events-heading">Small moments, made yours</h2><p>Save an event idea and assign a bundle to try manually. Automatic triggers are not connected yet.</p></div>
                <button className="primary-button" onClick={newEvent}>
                  <Plus size={17} /> New event
                </button>
              </div>

              {eventDraft && (
                <div className="event-editor">
                  <div className="event-editor-fields">
                    <label>
                      <span>Name</span>
                      <input
                        className="composer-input"
                        value={eventDraft.name}
                        placeholder="e.g. Hydration reminder"
                        onChange={(e) => setEventDraft({ ...eventDraft, name: e.target.value })}
                      />
                    </label>
                    <label>
                      <span>Trigger</span>
                      <input
                        className="composer-input"
                        value={eventDraft.trigger}
                        placeholder="e.g. Every 5 mins"
                        onChange={(e) => setEventDraft({ ...eventDraft, trigger: e.target.value })}
                      />
                    </label>
                    <label>
                      <span>Gesture bundle</span>
                      <select
                        className="medium-select"
                        value={eventDraft.bundleId ?? ""}
                        onChange={(e) => setEventDraft({ ...eventDraft, bundleId: e.target.value || null })}
                      >
                        <option value="">No bundle</option>
                        {bundles.map((bundle) => (
                          <option key={bundle.id} value={bundle.id}>{bundle.name}</option>
                        ))}
                      </select>
                    </label>
                  </div>
                  <div className="event-editor-actions">
                    <button className="primary-button" onClick={saveEvent} disabled={!eventDraft.name.trim()} type="button">
                      <Save size={15} /> Save event
                    </button>
                    <button className="ghost-button" onClick={() => setEventDraft(null)} type="button">
                      <X size={14} /> Cancel
                    </button>
                  </div>
                </div>
              )}

              {events.length === 0 && !eventDraft && (
                <p className="composer-hint">No events yet — create one and assign a gesture bundle.</p>
              )}

              {events.map((event) => {
                const bundle = bundles.find((entry) => entry.id === event.bundleId) ?? null;
                const running = playingBundle !== null && playingBundle === event.bundleId;
                return (
                  <article className="rule-row event-row" key={event.id}>
                    <span className="rule-icon"><Bell size={19} /></span>
                    <div className="rule-name"><strong>{event.name}</strong><small>{event.trigger || "No trigger set"}</small></div>
                    <div className="event-assign">
                      <select
                        className="medium-select"
                        value={event.bundleId ?? ""}
                        aria-label={`Bundle for ${event.name}`}
                        onChange={(e) => updateEvent(event.id, { bundleId: e.target.value || null })}
                      >
                        <option value="">No bundle</option>
                        {bundles.map((entry) => (
                          <option key={entry.id} value={entry.id}>{entry.name}</option>
                        ))}
                      </select>
                    </div>
                    <button
                      className={event.enabled ? "toggle enabled" : "toggle"}
                      role="switch"
                      aria-checked={event.enabled}
                      aria-label={`Enable ${event.name}`}
                      onClick={() => updateEvent(event.id, { enabled: !event.enabled })}
                    ><span /></button>
                    <button className="icon-button" title="Edit event" onClick={() => setEventDraft({ ...event })}>
                      <Pencil size={16} />
                    </button>
                    <button className="icon-button" title={bundle ? "Run event now" : "Assign a bundle first"} disabled={!bundle} onClick={() => (running ? stopPreview() : runEvent(event))}>
                      {running ? <CircleStop size={17} /> : <Play size={17} fill="currentColor" />}
                    </button>
                    <button className="icon-button" title="Delete event" onClick={() => deleteEvent(event.id)}>
                      <Trash2 size={16} />
                    </button>
                  </article>
                );
              })}

            </section>
          )}

          <section className="activity-panel" aria-labelledby="activity-heading">
            <div className="section-heading"><div><h2 id="activity-heading">Activity</h2><p>{queue.running} running · {queue.queued} queued · {queue.sending} awaiting acceptance</p></div><span className="state-pill">{snapshot.hello?.queueCapacity ?? 4} waiting slots</span></div>
            <p className="help-text">This app session only; newest first, up to 100 entries. Only firmware events mark completion.</p>
            {snapshot.activity.length === 0 ? <div className="empty-state activity-empty"><Sparkles size={22} aria-hidden="true" /><strong>A quiet moment, for now.</strong><span>Gestures sent to your pet will appear here. Only real device activity is logged.</span></div> : (
              <ol className="activity-list">
                {[...snapshot.activity].reverse().map((item) => (
                  <li key={item.requestId} className={`activity-row ${isInFlight(item.state) ? "in-flight" : ""}`}>
                    <div className="activity-title"><strong>{item.name}</strong><span className={`activity-state ${item.state}`}>{item.state.replace("_", " ")}</span></div>
                    <code>{item.actionId} · {item.requestId}</code>
                    <p>{item.message}</p><time dateTime={new Date(item.updatedAt).toISOString()}>{new Date(item.updatedAt).toLocaleTimeString()}</time>
                  </li>
                ))}
              </ol>
            )}
          </section>
        </div>
      </main>
    </div>
  );
}

export default App;
