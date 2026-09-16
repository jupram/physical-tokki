// Plays a bundle: every medium lane starts at t=0 (concurrent across mediums),
// and the gestures inside a lane run one after another (sequential within a
// medium). Returns a stop function.

import type { GestureDef, Medium } from "./catalog";
import { MEDIA } from "./catalog";
import { cancelSpeech, playSound } from "./audio";
import type { PreviewState } from "./GesturePreview";

export type LaneGestures = Record<Medium, GestureDef[]>;

const LANE_GAP_MS = 120;

export function bundleDuration(lanes: LaneGestures): number {
  return Math.max(
    0,
    ...MEDIA.map((medium) =>
      lanes[medium].reduce((total, gesture) => total + gesture.ms + LANE_GAP_MS, 0),
    ),
  );
}

export function playBundle(
  lanes: LaneGestures,
  setState: (updater: (prev: PreviewState) => PreviewState) => void,
  onDone: () => void,
): () => void {
  const timers: number[] = [];

  for (const medium of MEDIA) {
    let offset = 0;
    for (const gesture of lanes[medium]) {
      const startAt = offset;
      const endAt = offset + gesture.ms;

      timers.push(
        window.setTimeout(() => {
          setState(
            (prev) =>
              ({
                ...prev,
                [medium]: { sim: gesture.sim[medium], label: gesture.name, active: true },
              }) as PreviewState,
          );
          if (medium === "speaker" && gesture.sim.speaker) {
            playSound(gesture.sim.speaker.sound);
          }
        }, startAt),
      );

      timers.push(
        window.setTimeout(() => {
          setState((prev) =>
            prev[medium].label === gesture.name
              ? ({ ...prev, [medium]: { ...prev[medium], active: false } } as PreviewState)
              : prev,
          );
        }, endAt),
      );

      offset = endAt + LANE_GAP_MS;
    }
  }

  timers.push(window.setTimeout(onDone, bundleDuration(lanes) + 60));

  return () => {
    timers.forEach((timer) => window.clearTimeout(timer));
    cancelSpeech();
  };
}
