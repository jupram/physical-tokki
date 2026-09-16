import { Bell, Cable, CircleStop, Gauge, Play, Radio, RefreshCw, Sparkles } from "lucide-react";
import { useEffect, useState } from "react";
import { client, emptySnapshot, isInFlight, queueSummary, type Port, type Snapshot } from "./native";
import "./App.css";

type View = "device" | "gestures" | "events";

function App() {
  const [view, setView] = useState<View>("device");
  const [snapshot, setSnapshot] = useState<Snapshot>(emptySnapshot);
  const [ports, setPorts] = useState<Port[]>([]);
  const [selectedPort, setSelectedPort] = useState("");
  const [localError, setLocalError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [filter, setFilter] = useState("");

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

  async function command(operation: () => Promise<void>) {
    setBusy(true);
    setLocalError(null);
    try { await operation(); }
    catch (error) { setLocalError(String(error)); }
    finally { setBusy(false); }
  }

  const connected = snapshot.status === "connected";
  const connecting = snapshot.status === "connecting" || snapshot.status === "loading";
  const ownsPort = connected || connecting;
  const queue = queueSummary(snapshot.activity);
  const visibleActions = snapshot.actions.filter((action) =>
    `${action.id} ${action.name} ${action.device}`.toLowerCase().includes(filter.toLowerCase()));
  const error = localError ?? snapshot.lastError;
  const statusText = !client.native ? "Native app required" : ({
    disconnected: "Disconnected", connecting: "Identifying device…", loading: "Loading catalog…",
    connected: "Connected", error: "Connection failed",
  }[snapshot.status] ?? snapshot.status);

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark" aria-hidden="true"><span /><span /></span>
          <div><strong>Physical Tokki</strong><small>Pet control desk</small></div>
        </div>
        <nav aria-label="Main navigation">
          <button className={`nav-item ${view === "device" ? "active" : ""}`} onClick={() => setView("device")}><Gauge size={18} /> Device</button>
          <button className={`nav-item ${view === "gestures" ? "active" : ""}`} onClick={() => setView("gestures")}><Sparkles size={18} /> Gestures <span className="nav-count">{snapshot.actions.length}</span></button>
          <button className={`nav-item ${view === "events" ? "active" : ""}`} onClick={() => setView("events")}><Bell size={18} /> Events <span className="nav-count">Later</span></button>
        </nav>
        <div className="sidebar-status">
          <Radio size={18} />
          <div><strong>{statusText}</strong><small>{snapshot.port ?? "USB serial · 115200 8N1"}</small></div>
        </div>
      </aside>

      <main>
        <header className="topbar">
          <div><span className="eyebrow">TOKKI DESKTOP / NATIVE SERIAL</span><h1>{view === "device" ? "Device" : view === "gestures" ? "Gestures" : "Events"}</h1></div>
          {ownsPort && <button className="connection-button" disabled={busy} onClick={() => void command(() => client.disconnect())}><CircleStop size={17} /> Disconnect</button>}
        </header>
        <div className="workspace">
          {!client.native && <div className="notice" role="status"><strong>Native desktop app required</strong><p>This browser preview cannot open USB serial ports. Run <code>npm run tauri dev</code> to connect to your pet. There is no mock device or simulated success.</p></div>}
          {error && <div className="notice error" role="alert"><strong>Operation failed</strong><p>{error}</p></div>}

          {view === "device" && (
            <section aria-labelledby="device-heading">
              <div className="section-heading"><div><h2 id="device-heading">Connect your pet</h2><p>Choose its USB UART port. Discovery never opens ports automatically.</p></div><span className={`state-pill ${connected ? "online" : ""}`}>{statusText}</span></div>
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
              <p className="help-text">Gestures run serially: one active, up to four waiting. They are finite and cannot be cancelled. Disconnecting does not stop accepted gestures. When the device queue drains, the pet resumes idle behavior.</p>
              <p className="help-text">Connection readiness covers board, LED, and RGB startup only. OLED and speaker initialize on use; their driver failures are reported in Activity.</p>
              <button className="connection-button catalog-link" disabled={!connected} onClick={() => setView("gestures")}><Sparkles size={16} /> Browse device gestures</button>
            </section>
          )}

          {view === "gestures" && (
            <section aria-labelledby="gestures-heading">
              <div className="section-heading"><div><h2 id="gestures-heading">Action catalog</h2><p>Names and stable IDs discovered from your connected pet.</p></div><button className="connection-button" disabled={!connected || busy} onClick={() => void command(() => client.refresh())}><RefreshCw size={16} /> Refresh catalog</button></div>
              <label className="search-label" htmlFor="gesture-filter">Find a gesture</label>
              <input id="gesture-filter" type="search" placeholder="Search name, device, or ID" value={filter} onChange={(event) => setFilter(event.target.value)} />
              <p className="help-text catalog-help">Send queues a real gesture on the pet. No cancellation or automatic retries.</p>
              <div className="action-list">
                {visibleActions.map((action) => (
                  <article className="action-row" key={action.id}>
                    <span className="device-icon"><Sparkles size={19} /></span>
                    <div className="action-name"><strong>{action.name}</strong><code>{action.id}</code></div>
                    <span className="action-device">{action.device}</span>
                    <button className="connection-button" title={`Send ${action.name}`} aria-label={`Send ${action.name}`} disabled={!connected || busy || queue.inFlight >= 5} onClick={() => void command(() => client.run(action.id))}><Play size={16} /> Send</button>
                  </article>
                ))}
                {visibleActions.length === 0 && <div className="empty-state">{connecting ? "Fetching all catalog pages…" : connected ? "No gestures match this view." : "Connect on the Device screen to discover gestures."}</div>}
              </div>
            </section>
          )}

          {view === "events" && (
            <section aria-labelledby="events-heading"><div className="section-heading"><div><h2 id="events-heading">Events are deferred</h2><p>This prototype is manual control only.</p></div></div><div className="empty-state"><Bell size={24} /><h3>No automation is running</h3><p>Scheduling, reminders, email integrations, and event-to-gesture rules are not implemented. Use Gestures to send actions explicitly.</p></div></section>
          )}

          <section className="activity-panel" aria-labelledby="activity-heading">
            <div className="section-heading"><div><h2 id="activity-heading">Activity</h2><p>{queue.running} running · {queue.queued} queued · {queue.sending} awaiting acceptance</p></div><span className="state-pill">{snapshot.hello?.queueCapacity ?? 4} waiting slots</span></div>
            <p className="help-text">This app session only; newest first, up to 100 entries. Only firmware events mark completion.</p>
            {snapshot.activity.length === 0 ? <div className="empty-state">No gestures sent in this app session.</div> : (
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
