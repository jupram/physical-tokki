import { useEffect, useState } from "react";

type PreviewTiming = {
  frames: number;
  frameMs: number;
  stillFrame: number;
};

// Keep the timer and media-query subscription in one cancellable lifecycle.
export function watchPreviewFrames(
  active: boolean,
  { frames, frameMs, stillFrame }: PreviewTiming,
  onFrame: (frame: number) => void,
): () => void {
  const preference = window.matchMedia("(prefers-reduced-motion: reduce)");
  let timer: number | undefined;
  const clearTimer = () => {
    if (timer !== undefined) window.clearInterval(timer);
    timer = undefined;
  };
  const update = () => {
    clearTimer();
    if (preference.matches) {
      onFrame(stillFrame);
    } else if (active) {
      let frame = 0;
      onFrame(frame);
      timer = window.setInterval(() => {
        onFrame(++frame);
        if (frame === frames - 1) clearTimer();
      }, frameMs);
    }
  };
  update();
  preference.addEventListener("change", update);
  return () => {
    clearTimer();
    preference.removeEventListener("change", update);
  };
}

export function usePreviewFrame(active: boolean, { frames, frameMs, stillFrame }: PreviewTiming) {
  const [frame, setFrame] = useState(active ? 0 : stillFrame);
  useEffect(
    () => watchPreviewFrames(active, { frames, frameMs, stillFrame }, setFrame),
    [active, frames, frameMs, stillFrame],
  );
  return frame;
}
