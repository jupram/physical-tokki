import {
  Bell,
  Cable,
  CircleStop,
  Eye,
  Gauge,
  Lightbulb,
  Mail,
  Play,
  Plus,
  Radio,
  Sparkles,
  Volume2,
} from "lucide-react";
import { useState } from "react";
import "./App.css";

type View = "device" | "gestures" | "events";
type DeviceKind = "oled" | "speaker" | "led" | "neopixel";

type Action = {
  id: string;
  name: string;
  device: DeviceKind;
  duration: string;
};

const actions: Action[] = [
  { id: "led.blink", name: "Blink status LED", device: "led", duration: "1.2 sec" },
  { id: "neopixel.rainbow", name: "Rainbow", device: "neopixel", duration: "2.6 sec" },
];

const deviceIcons = {
  oled: Eye,
  speaker: Volume2,
  led: Lightbulb,
  neopixel: Sparkles,
};

function App() {
  const [view, setView] = useState<View>("device");
  const [connected, setConnected] = useState(false);
  const [activeAction, setActiveAction] = useState<string | null>(null);
  const [ruleEnabled, setRuleEnabled] = useState(true);
  const [activity, setActivity] = useState("Mock transport ready");

  function toggleConnection() {
    const nextConnected = !connected;
    setConnected(nextConnected);
    setActiveAction(null);
    setActivity(nextConnected ? "Connected to mock Feather V2" : "Disconnected");
  }

  function previewAction(action: Action) {
    if (!connected) {
      setActivity("Connect a device before previewing an action");
      return;
    }

    setActiveAction(action.id);
    setActivity(`Running ${action.id}`);
    window.setTimeout(() => {
      setActiveAction(null);
      setActivity(`Completed ${action.id}`);
    }, 900);
  }

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark" aria-hidden="true"><span /><span /></span>
          <div><strong>Physical Tokki</strong><small>Pet control desk</small></div>
        </div>

        <nav aria-label="Main navigation">
          <button className={view === "device" ? "nav-item active" : "nav-item"} onClick={() => setView("device")}>
            <Gauge size={18} /> Device
          </button>
          <button className={view === "gestures" ? "nav-item active" : "nav-item"} onClick={() => setView("gestures")}>
            <Sparkles size={18} /> Gestures <span className="nav-count">{actions.length}</span>
          </button>
          <button className={view === "events" ? "nav-item active" : "nav-item"} onClick={() => setView("events")}>
            <Bell size={18} /> Events <span className="nav-count">1</span>
          </button>
        </nav>

        <div className="sidebar-status">
          <span className="status-dot mock" />
          <div><strong>Mock transport</strong><small>Serial backend pending</small></div>
        </div>
      </aside>

      <main>
        <header className="topbar">
          <div>
            <span className="eyebrow">TOKKI DESKTOP / 0.1.0</span>
            <h1>{view === "device" ? "Device" : view === "gestures" ? "Gestures" : "Events"}</h1>
          </div>
          <button className={connected ? "connection-button connected" : "connection-button"} onClick={toggleConnection}>
            {connected ? <CircleStop size={17} /> : <Cable size={17} />}
            {connected ? "Disconnect" : "Connect mock"}
          </button>
        </header>

        <div className="workspace">
          {view === "device" && (
            <section aria-labelledby="device-heading">
              <div className="section-heading">
                <div><h2 id="device-heading">Feather V2</h2><p>USB serial / COM3</p></div>
                <span className={connected ? "state-pill online" : "state-pill"}>
                  <span className="status-dot" /> {connected ? "Online" : "Offline"}
                </span>
              </div>

              <div className="metric-grid">
                <article><span>Firmware</span><strong>0.1.0</strong><small>physical_tokki</small></article>
                <article><span>Protocol</span><strong>v1</strong><small>Newline JSON</small></article>
                <article><span>Actions</span><strong>{actions.length}</strong><small>Registry entries</small></article>
              </div>

              <div className="activity-strip" role="status">
                <Radio size={18} /><span>{activity}</span><time>now</time>
              </div>

              <div className="device-list">
                <div className="list-heading"><h3>Hardware</h3><span>Initialization state</span></div>
                {(["oled", "speaker", "led", "neopixel"] as DeviceKind[]).map((device) => {
                  const Icon = deviceIcons[device];
                  const implemented = device === "led" || device === "neopixel";
                  return (
                    <div className="device-row" key={device}>
                      <span className={`device-icon ${device}`}><Icon size={18} /></span>
                      <div>
                        <strong>{device === "neopixel" ? "NeoPixel" : device.toUpperCase()}</strong>
                        <small>{implemented ? "Shared component ready" : "Component moved; actions pending"}</small>
                      </div>
                      <span className={implemented ? "readiness ready" : "readiness pending"}>
                        {implemented ? "Ready" : "Pending"}
                      </span>
                    </div>
                  );
                })}
              </div>
            </section>
          )}

          {view === "gestures" && (
            <section aria-labelledby="gestures-heading">
              <div className="section-heading">
                <div><h2 id="gestures-heading">Action catalog</h2><p>Stable IDs reported by firmware</p></div>
                <span className="state-pill"><Radio size={13} /> Mock catalog</span>
              </div>

              <div className="action-list">
                {actions.map((action) => {
                  const Icon = deviceIcons[action.device];
                  const running = activeAction === action.id;
                  return (
                    <article className="action-row" key={action.id}>
                      <span className={`device-icon ${action.device}`}><Icon size={19} /></span>
                      <div className="action-name"><strong>{action.name}</strong><code>{action.id}</code></div>
                      <span className="action-device">{action.device}</span>
                      <span className="duration">{action.duration}</span>
                      <button
                        className="icon-button"
                        title={running ? "Action running" : `Preview ${action.name}`}
                        aria-label={running ? `${action.name} running` : `Preview ${action.name}`}
                        disabled={running}
                        onClick={() => previewAction(action)}
                      >
                        {running ? <CircleStop size={17} /> : <Play size={17} fill="currentColor" />}
                      </button>
                    </article>
                  );
                })}
              </div>
            </section>
          )}

          {view === "events" && (
            <section aria-labelledby="events-heading">
              <div className="section-heading">
                <div><h2 id="events-heading">Automation rules</h2><p>Stored and evaluated on this PC</p></div>
                <button className="primary-button" onClick={() => setActivity("New event editor is the next UI slice")}>
                  <Plus size={17} /> New event
                </button>
              </div>

              <article className="rule-row">
                <span className="rule-icon"><Bell size={19} /></span>
                <div className="rule-name"><strong>Hydration reminder</strong><small>Every 1 hour</small></div>
                <div className="rule-action"><code>neopixel.rainbow</code><span>then</span><code>led.blink</code></div>
                <button
                  className={ruleEnabled ? "toggle enabled" : "toggle"}
                  role="switch"
                  aria-checked={ruleEnabled}
                  aria-label="Enable hydration reminder"
                  onClick={() => setRuleEnabled(!ruleEnabled)}
                ><span /></button>
                <button className="icon-button" title="Run hydration reminder now" aria-label="Run hydration reminder now" onClick={() => previewAction(actions[1])}>
                  <Play size={17} fill="currentColor" />
                </button>
              </article>

              <div className="integration-row">
                <span className="rule-icon mail"><Mail size={19} /></span>
                <div><strong>Manager mail</strong><small>Mock event source</small></div>
                <span className="readiness pending">Adapter pending</span>
              </div>
            </section>
          )}
        </div>
      </main>
    </div>
  );
}

export default App;
