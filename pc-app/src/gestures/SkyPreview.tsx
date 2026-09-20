import { SKY_FRAMES, SKY_FRAME_MS, SKY_STILL_FRAME, skyPath, type SkyScene } from "./skyScenes";
import { usePreviewFrame } from "./usePreviewFrame";

export function SkyPreview({ scene, active }: { scene: SkyScene; active: boolean }) {
  const frame = usePreviewFrame(active, {
    frames: SKY_FRAMES[scene], frameMs: SKY_FRAME_MS, stillFrame: SKY_STILL_FRAME[scene],
  });

  if (frame === SKY_FRAMES[scene] - 1) {
    return <div className="pv-eyes" role="img" aria-label="Restored open eyes"><span className="pv-eye" /><span className="pv-eye" /></div>;
  }
  return (
    <svg className="pv-sky" viewBox="0 0 128 64" role="img"
      aria-label={scene === "night_sky" ? "Night sky with twinkling stars and a crescent moon" : "Sun rising above the horizon"}
      data-frame={frame}>
      <path d={skyPath(scene, frame)} />
    </svg>
  );
}
