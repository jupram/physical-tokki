import { SLEEP_FRAMES, SLEEP_FRAME_MS, SLEEP_STILL_FRAME, sleepingEyePath, sleepingPose } from "./sleepingEyes";
import { usePreviewFrame } from "./usePreviewFrame";
import "./SleepingPreview.css";

export function SleepingFrame({ frame }: { frame: number }) {
  const pose = sleepingPose(frame);
  const label = pose.closure === 0
    ? (pose.frame >= 46 ? "Restored open eyes" : "Open eyes")
    : pose.closure === 1 ? "Sleeping eyes with closed crescent lids and Zzz" : "Drowsy eyes gently closing or reopening";
  return (
    <svg className="pv-sleeping" viewBox="0 0 128 64" role="img"
      aria-label={label} data-frame={pose.frame}>
      <g transform={`translate(0 ${pose.bob})`}>
        <path d={sleepingEyePath(44, pose.closure)} />
        <path d={sleepingEyePath(84, pose.closure)} />
      </g>
      {pose.zeds.map(({ x, y, size }, index) => (
        <path key={index} className="pv-sleep-z"
          d={`M${x} ${y}h${size}l-${size} ${size}h${size}`} />
      ))}
    </svg>
  );
}

export function SleepingPreview({ active }: { active: boolean }) {
  const frame = usePreviewFrame(active, {
    frames: SLEEP_FRAMES, frameMs: SLEEP_FRAME_MS, stillFrame: SLEEP_STILL_FRAME,
  });
  return <SleepingFrame frame={frame} />;
}
