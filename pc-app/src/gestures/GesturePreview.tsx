import { Eye, Lightbulb, Volume2 } from "lucide-react";
import type { LedSim, Medium, OledSim, SpeakerSim } from "./catalog";
import { MEDIUM_LABEL } from "./catalog";

export type PreviewLane<T> = { sim?: T; label: string; active: boolean };

export type PreviewState = {
  oled: PreviewLane<OledSim>;
  led: PreviewLane<LedSim>;
  speaker: PreviewLane<SpeakerSim>;
};

export const IDLE_PREVIEW: PreviewState = {
  oled: { label: "Idle", active: false },
  led: { label: "Idle", active: false },
  speaker: { label: "Idle", active: false },
};

const ART_GLYPH: Record<string, string> = {
  drink_water: "DRINK WATER",
  water_drop: "💧",
  fire: "🔥",
  checkmark: "✓",
  thinking: "•••",
  heart: "♥",
  exclamation: "!",
};

function OledView({ sim }: { sim?: OledSim }) {
  if (!sim) return <span className="pv-idle">—</span>;
  if (sim.kind === "eyes") {
    return (
      <div className={`pv-eyes ${sim.eyes}`} aria-hidden="true">
        <span className="pv-eye" />
        <span className="pv-eye" />
      </div>
    );
  }
  const emoji = sim.art === "water_drop" || sim.art === "fire";
  return <span className={`pv-art ${sim.art}${emoji ? " emoji" : ""}`}>{ART_GLYPH[sim.art]}</span>;
}

function LedView({ sim, active }: { sim?: LedSim; active: boolean }) {
  const rainbow = sim?.effect === "rainbow";
  const className = ["pv-led", active && sim ? `on ${sim.effect}` : ""].filter(Boolean).join(" ");
  const style = sim && !rainbow ? ({ "--led-color": sim.color } as React.CSSProperties) : undefined;
  return (
    <div className="pv-led-stage">
      <span className={className} style={style} />
    </div>
  );
}

function SpeakerView({ active }: { active: boolean }) {
  return (
    <div className={`pv-eq ${active ? "playing" : ""}`} aria-hidden="true">
      {Array.from({ length: 7 }).map((_, index) => (
        <span key={index} style={{ animationDelay: `${index * 0.08}s` }} />
      ))}
    </div>
  );
}

const ICONS = { oled: Eye, led: Lightbulb, speaker: Volume2 };

function Panel({
  medium,
  lane,
  children,
}: {
  medium: Medium;
  lane: PreviewLane<unknown>;
  children: React.ReactNode;
}) {
  const Icon = ICONS[medium];
  return (
    <div className="pv-panel">
      <div className="pv-head">
        <span className={`device-icon ${medium === "led" ? "neopixel" : medium}`}><Icon size={16} /></span>
        <div>
          <strong>{MEDIUM_LABEL[medium]}</strong>
          <small>{lane.label}</small>
        </div>
        <span className={`pv-dot ${lane.active ? "on" : ""}`} />
      </div>
      <div className={`pv-screen ${medium} ${lane.active ? "on" : ""}`}>{children}</div>
    </div>
  );
}

export function MediumPreview({
  medium,
  lane,
}: {
  medium: Medium;
  lane: PreviewLane<OledSim | LedSim | SpeakerSim>;
}) {
  let body: React.ReactNode;
  if (medium === "oled") {
    body = <OledView sim={lane.sim as OledSim | undefined} />;
  } else if (medium === "led") {
    body = <LedView sim={lane.sim as LedSim | undefined} active={lane.active} />;
  } else {
    body = <SpeakerView active={lane.active} />;
  }
  return (
    <Panel medium={medium} lane={lane}>
      {body}
    </Panel>
  );
}

export function GesturePreview({ state }: { state: PreviewState }) {
  return (
    <div className="pv-grid">
      <MediumPreview medium="oled" lane={state.oled} />
      <MediumPreview medium="led" lane={state.led} />
      <MediumPreview medium="speaker" lane={state.speaker} />
    </div>
  );
}
