export const SLEEP_FRAMES = 48;
export const SLEEP_FRAME_MS = 90;
export const SLEEP_STILL_FRAME = 20;

function ease(value: number) {
  return value * value * (3 - 2 * value);
}

export function sleepingPose(frame: number) {
  frame = Math.max(0, Math.min(SLEEP_FRAMES - 1, Math.trunc(frame)));
  const asleep = frame >= 8 && frame <= 39;
  const closure = frame < 8 ? ease(frame / 8) : frame <= 39 ? 1 : ease(Math.max(0, (46 - frame) / 7));
  const bob = asleep ? Math.sin((frame - 8) * Math.PI / 12) : 0;
  const zeds = asleep ? Array.from({ length: 3 }, (_, index) => {
    const age = (frame - 8 + index * 8) % 24;
    return {
      x: 78 + age,
      y: 22 - Math.trunc(age * 3 / 4),
      size: 3 + Math.trunc(age / 6),
    };
  }) : [];
  return { frame, closure, bob, zeds };
}

// Smoothly morph the approximate open eyes into happy-style closed crescents.
export function sleepingEyePath(centerX: number, closure: number) {
  const y = 32 + closure * 8;
  const top = y - 20 + closure * 8;
  const bottom = y + 20 - closure * 26;
  return `M${centerX - 12} ${y}C${centerX - 12} ${top} ${centerX + 12} ${top} ${centerX + 12} ${y}C${centerX + 12} ${bottom} ${centerX - 12} ${bottom} ${centerX - 12} ${y}Z`;
}
