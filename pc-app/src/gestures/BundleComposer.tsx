import { useRef, useState } from "react";
import { ChevronDown, ChevronUp, CircleStop, Play, Plus, Save, X } from "lucide-react";
import {
  CATALOG_BY_ID,
  MEDIA,
  MEDIUM_LABEL,
  gesturesByMedium,
} from "./catalog";
import type { Bundle, GestureDef, Medium } from "./catalog";
import { GesturePreview, IDLE_PREVIEW } from "./GesturePreview";
import type { PreviewState } from "./GesturePreview";
import { bundleDuration, playBundle } from "./player";
import type { LaneGestures } from "./player";

type Lanes = Record<Medium, string[]>;

const EMPTY_LANES: Lanes = { oled: [], led: [], speaker: [] };

type Props = {
  initial?: Bundle | null;
  onSave: (bundle: Bundle) => void;
  onCancelEdit?: () => void;
  onPreviewGesture?: (gesture: GestureDef) => void;
};

export function BundleComposer({ initial, onSave, onCancelEdit, onPreviewGesture }: Props) {
  const editing = !!initial;
  const [lanes, setLanes] = useState<Lanes>(() =>
    initial
      ? { oled: [...initial.lanes.oled], led: [...initial.lanes.led], speaker: [...initial.lanes.speaker] }
      : EMPTY_LANES,
  );
  const [name, setName] = useState(initial?.name ?? "");
  const [preview, setPreview] = useState<PreviewState>(IDLE_PREVIEW);
  const [playing, setPlaying] = useState(false);
  const stopRef = useRef<(() => void) | null>(null);

  const laneGestures: LaneGestures = {
    oled: lanes.oled.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
    led: lanes.led.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
    speaker: lanes.speaker.map((id) => CATALOG_BY_ID[id]).filter(Boolean),
  };
  const totalMs = bundleDuration(laneGestures);
  const totalCount = MEDIA.reduce((sum, medium) => sum + lanes[medium].length, 0);

  function add(medium: Medium, id: string) {
    setLanes((current) => ({ ...current, [medium]: [...current[medium], id] }));
  }

  function removeAt(medium: Medium, index: number) {
    setLanes((current) => ({
      ...current,
      [medium]: current[medium].filter((_, position) => position !== index),
    }));
  }

  function move(medium: Medium, index: number, direction: -1 | 1) {
    setLanes((current) => {
      const next = [...current[medium]];
      const target = index + direction;
      if (target < 0 || target >= next.length) return current;
      [next[index], next[target]] = [next[target], next[index]];
      return { ...current, [medium]: next };
    });
  }

  function stop() {
    stopRef.current?.();
    stopRef.current = null;
    setPlaying(false);
    setPreview(IDLE_PREVIEW);
  }

  function play() {
    stop();
    if (totalCount === 0) return;
    setPlaying(true);
    stopRef.current = playBundle(
      laneGestures,
      (updater) => setPreview((prev) => updater(prev)),
      () => {
        setPlaying(false);
        setPreview(IDLE_PREVIEW);
        stopRef.current = null;
      },
      onPreviewGesture,
    );
  }

  function save() {
    const trimmed = name.trim();
    if (!trimmed || totalCount === 0) return;
    onSave({
      id: initial?.id ?? `bundle.${Date.now().toString(36)}`,
      name: trimmed,
      lanes: {
        oled: [...lanes.oled],
        led: [...lanes.led],
        speaker: [...lanes.speaker],
      },
    });
    if (!editing) {
      setLanes(EMPTY_LANES);
      setName("");
    }
  }

  return (
    <section className="bundle-composer" aria-labelledby="bundle-heading">
      <div className="section-heading">
        <div>
          <h2 id="bundle-heading">{editing ? "Edit bundle" : "Bundle composer"}</h2>
          <p>{editing ? `Editing \u201c${initial?.name}\u201d` : "Concurrent across mediums \u00b7 sequential within a medium"}</p>
        </div>
        <div className="composer-head-right">
          <span className="state-pill">
            {totalCount} gesture{totalCount === 1 ? "" : "s"} \u00b7 {(totalMs / 1000).toFixed(1)}s
          </span>
          {editing && onCancelEdit && (
            <button className="ghost-button" onClick={onCancelEdit} type="button">
              <X size={14} /> Cancel edit
            </button>
          )}
        </div>
      </div>

      <div className="lane-grid">
        {MEDIA.map((medium) => (
          <div className="lane" key={medium}>
            <div className="lane-head">
              <span className={`device-icon ${medium === "led" ? "neopixel" : medium}`} />
              <strong>{MEDIUM_LABEL[medium]}</strong>
            </div>

            <ol className="lane-seq">
              {lanes[medium].length === 0 && <li className="lane-empty">Add gestures below</li>}
              {lanes[medium].map((id, index) => {
                const gesture = CATALOG_BY_ID[id];
                return (
                  <li className="lane-item" key={`${id}-${index}`}>
                    <span className="lane-step">{index + 1}</span>
                    <span className="lane-name">{gesture?.name ?? id}</span>
                    <div className="lane-controls">
                      <button className="icon-button small" title="Move up" onClick={() => move(medium, index, -1)} disabled={index === 0}>
                        <ChevronUp size={14} />
                      </button>
                      <button className="icon-button small" title="Move down" onClick={() => move(medium, index, 1)} disabled={index === lanes[medium].length - 1}>
                        <ChevronDown size={14} />
                      </button>
                      <button className="icon-button small" title="Remove" onClick={() => removeAt(medium, index)}>
                        <X size={14} />
                      </button>
                    </div>
                  </li>
                );
              })}
            </ol>

            <div className="lane-palette">
              {gesturesByMedium(medium).map((gesture: GestureDef) => (
                <button
                  className="palette-chip"
                  key={gesture.id}
                  title={`Add ${gesture.name}`}
                  onClick={() => add(medium, gesture.id)}
                  type="button"
                >
                  <Plus size={13} /> {gesture.name}
                </button>
              ))}
            </div>
          </div>
        ))}
      </div>

      <div className="bundle-preview">
        <GesturePreview state={preview} />
        <div className="bundle-actions">
          <button className={playing ? "primary-button danger" : "primary-button"} onClick={playing ? stop : play} disabled={totalCount === 0} type="button">
            {playing ? <CircleStop size={15} /> : <Play size={15} fill="currentColor" />}
            {playing ? "Stop" : "Preview bundle"}
          </button>
          <input
            className="composer-input"
            value={name}
            placeholder="Bundle name (e.g. Hydration nudge)"
            onChange={(event) => setName(event.target.value)}
          />
          <button className="primary-button" onClick={save} disabled={!name.trim() || totalCount === 0} type="button">
            <Save size={15} /> {editing ? "Update bundle" : "Save bundle"}
          </button>
        </div>
      </div>
    </section>
  );
}
