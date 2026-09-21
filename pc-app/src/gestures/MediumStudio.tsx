import { useEffect, useRef, useState } from "react";
import { CircleStop, Play } from "lucide-react";
import { CATALOG_BY_ID, MEDIUM_LABEL, gesturesByMedium, scrollingTextGesture } from "./catalog";
import type { GestureDef, LedSim, Medium, OledSim, SpeakerSim } from "./catalog";
import { MediumPreview } from "./GesturePreview";
import type { PreviewLane } from "./GesturePreview";
import { cancelSpeech, playSound } from "./audio";
import { ScrollingTextInput } from "./ScrollingTextInput";
import { DEFAULT_SCROLLING_TEXT, SCROLLING_TEXT_ID, scrollingTextError } from "./scrollingText";

type LaneSim = OledSim | LedSim | SpeakerSim;

// One medium row on the Gestures page: a dropdown on the left drives a live
// preview of that medium on the right.
type Props = {
  medium: Medium;
  onPreviewGesture?: (gesture: GestureDef, text?: string) => void;
};

export function MediumStudio({ medium, onPreviewGesture }: Props) {
  const gestures = gesturesByMedium(medium);
  const first = gestures[0];
  const [selectedId, setSelectedId] = useState(first?.id ?? "");
  const [lane, setLane] = useState<PreviewLane<LaneSim>>({
    sim: first?.sim[medium] as LaneSim | undefined,
    label: first?.name ?? "Idle",
    active: false,
  });
  const [playing, setPlaying] = useState(false);
  const [text, setText] = useState(DEFAULT_SCROLLING_TEXT);
  const [previewCycle, setPreviewCycle] = useState(0);
  const stopRef = useRef<(() => void) | null>(null);
  const scrolling = selectedId === SCROLLING_TEXT_ID;
  const textError = scrolling ? scrollingTextError(text) : null;
  const selected = scrolling && !textError ? scrollingTextGesture(text) : CATALOG_BY_ID[selectedId];

  useEffect(() => () => stopRef.current?.(), []);

  function stop() {
    stopRef.current?.();
    stopRef.current = null;
    setPlaying(false);
    setLane((current) => ({ ...current, active: false }));
  }

  function selectGesture(id: string) {
    stop();
    setSelectedId(id);
    const gesture = CATALOG_BY_ID[id];
    setLane({
      sim: gesture?.sim[medium] as LaneSim | undefined,
      label: gesture?.name ?? "Idle",
      active: false,
    });
  }

  function play() {
    if (!selected || textError) return;
    stopRef.current?.();
    const sim = selected.sim[medium] as LaneSim | undefined;
    setPlaying(true);
    setPreviewCycle((cycle) => cycle + 1);
    setLane({ sim, label: selected.name, active: true });
    if (medium === "speaker" && selected.sim.speaker) {
      playSound(selected.sim.speaker.sound);
    }
    onPreviewGesture?.(selected, scrolling ? text : undefined);
    const timer = window.setTimeout(() => {
      setPlaying(false);
      setLane({ sim, label: selected.name, active: false });
      stopRef.current = null;
    }, selected.ms);
    stopRef.current = () => {
      window.clearTimeout(timer);
      cancelSpeech();
    };
  }

  return (
    <div className="medium-row">
      <div className="medium-left">
        <div className="list-heading">
          <h3>{MEDIUM_LABEL[medium]}</h3>
          <span>{gestures.length} gestures</span>
        </div>
        <select
          className="medium-select"
          value={selectedId}
          onChange={(event) => selectGesture(event.target.value)}
          aria-label={`${MEDIUM_LABEL[medium]} gesture`}
        >
          {gestures.map((gesture) => (
            <option key={gesture.id} value={gesture.id}>
              {gesture.name}
            </option>
          ))}
        </select>
        {scrolling && (
          <ScrollingTextInput id="preview-scrolling-text" value={text} onChange={(value) => {
            stop();
            setText(value);
          }} />
        )}
        <div className="medium-meta">
          <code>{selected?.id}</code>
          <span>{selected ? `${(selected.ms / 1000).toFixed(1)}s` : ""}</span>
        </div>
        <button
          className={playing ? "primary-button danger" : "primary-button"}
          onClick={playing ? stop : play}
          disabled={!playing && !!textError}
          type="button"
        >
          {playing ? <CircleStop size={15} /> : <Play size={15} fill="currentColor" />}
          {playing ? "Stop" : "Preview"}
        </button>
      </div>
      <div className="medium-right">
        <MediumPreview key={previewCycle} medium={medium} lane={lane} />
      </div>
    </div>
  );
}
