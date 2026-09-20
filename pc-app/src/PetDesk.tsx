import { ArrowRight, Eye, Heart, Layers, Lightbulb, Sparkles, Volume2 } from "lucide-react";
import type { Snapshot } from "./native";

export function PetPortrait() {
  return (
    <svg className="pet-portrait" viewBox="0 0 240 220" aria-hidden="true">
      <ellipse className="pet-shadow" cx="120" cy="202" rx="76" ry="9" />
      <path className="pet-ear" d="M65 78C44 41 50 16 65 18C81 20 86 50 88 73Z" />
      <path className="pet-ear" d="M151 73C154 43 163 19 179 22C193 26 187 56 174 82Z" />
      <rect className="pet-body" x="35" y="63" width="170" height="132" rx="52" />
      <rect className="pet-face" x="53" y="83" width="134" height="84" rx="32" />
      <g className="pet-eyes">
        <ellipse cx="94" cy="120" rx="13" ry="22" />
        <ellipse cx="146" cy="120" rx="13" ry="22" />
        <ellipse className="pet-pupil" cx="98" cy="123" rx="6" ry="12" />
        <ellipse className="pet-pupil" cx="150" cy="123" rx="6" ry="12" />
      </g>
      <path className="pet-smile" d="M111 149Q120 156 129 149" />
      <circle className="pet-heart-light" cx="120" cy="181" r="5" />
      <path className="pet-foot" d="M64 188V198H85M155 198H176V188" />
    </svg>
  );
}

type ConnectionProps = {
  status: Snapshot["status"];
  native: boolean;
  port: string | null;
  onOpen: () => void;
};

export function PetConnectionBadge({ status, native, port, onOpen }: ConnectionProps) {
  const connected = native && status === "connected";
  const label = !native ? "Not connected · browser preview" : {
    disconnected: "Not connected",
    connecting: "Connecting to Tokki",
    loading: "Discovering gestures",
    connected: `Connected${port ? ` · ${port}` : ""}`,
    error: "Connection lost or failed",
  }[status];
  return (
    <div className="pet-connection" role="status" aria-live="polite" aria-atomic="true">
      <button
        className={`pet-connection-button ${connected ? "is-connected" : "is-disconnected"}`}
        onClick={onOpen}
        aria-label={`Pet control desk: ${label}. Open device settings`}
        title={label}
      >
        <span className="connection-pet"><PetPortrait /></span>
        <span className="connection-copy"><strong>Pet control desk</strong><small>{label}</small></span>
        <span className="connection-dot" aria-hidden="true" />
      </button>
    </div>
  );
}

export function PetWelcome({
  connected, onExplore, onCompose, onEvents,
}: {
  connected: boolean;
  onExplore: () => void;
  onCompose: () => void;
  onEvents: () => void;
}) {
  return (
    <>
      <section className="pet-welcome" aria-labelledby="welcome-heading">
        <div className="welcome-copy">
          <span className="eyebrow"><Heart size={14} /> A LITTLE COMPANY FOR YOUR DESK</span>
          <h2 id="welcome-heading">{connected ? "Hey, Tokki.\nLet's play." : "Small pet.\nBig personality."}</h2>
          <p>{connected
            ? "Your little companion is connected. Send a happy glance, a gentle glow, or a tiny tune to brighten the day."
            : "A curious glance. A little glow. A happy tune. Connect your Tokki below, or explore its playful side with local previews."}</p>
          <button className="primary-button" onClick={onExplore}><Sparkles size={17} /> Explore gestures <ArrowRight size={16} /></button>
          <span className="welcome-note">{connected ? "Previews also play on your connected pet." : "No pet connected? You can still preview on this PC."}</span>
        </div>
        <div className="pet-scene">
          <span className="scene-spark scene-spark-one" aria-hidden="true"><Sparkles size={25} /></span>
          <span className="scene-spark scene-spark-two" aria-hidden="true"><Heart size={22} /></span>
          <div className="pet-stage"><PetPortrait /></div>
          <div className="pet-traits" aria-hidden="true"><span><Eye size={13} /> Eyes</span><span><Lightbulb size={13} /> Glow</span><span><Volume2 size={13} /> Voice</span></div>
          <small>Meet Tokki · illustration, not a live device view</small>
        </div>
      </section>
      <div className="desk-shortcuts" aria-label="Explore your pet">
        <button onClick={onExplore}><span className="shortcut-icon"><Eye size={21} /></span><span><strong>A little expression</strong><small>Find your pet's next mood</small></span><ArrowRight size={16} /></button>
        <button onClick={onCompose}><span className="shortcut-icon"><Layers size={21} /></span><span><strong>Make it your own</strong><small>Mix eyes, lights, and sounds</small></span><ArrowRight size={16} /></button>
        <button onClick={onEvents}><span className="shortcut-icon"><Heart size={21} /></span><span><strong>Little daily rituals</strong><small>Save ideas and try them manually</small></span><ArrowRight size={16} /></button>
      </div>
    </>
  );
}
