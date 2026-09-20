import type { OledArt } from "./catalog";

export type SkyScene = Extract<OledArt, "night_sky" | "sunrise">;

export const SKY_FRAME_MS = 90;
export const SKY_FRAMES: Record<SkyScene, number> = { night_sky: 48, sunrise: 64 };
export const SKY_STILL_FRAME: Record<SkyScene, number> = { night_sky: 12, sunrise: 42 };

// Match the integer, page-packed geometry in components/tokki_oled/oled_art.c.
export function skyPixels(scene: SkyScene, frame: number): Uint8Array {
  const pixels = new Uint8Array(128 * 64);
  function pixel(x: number, y: number) {
    if (x >= 0 && x < 128 && y >= 0 && y < 64) pixels[y * 128 + x] = 1;
  }
  function line(x1: number, y1: number, x2: number, y2: number) {
    const dx = x2 - x1;
    const dy = y2 - y1;
    const steps = Math.abs(dx) + Math.abs(dy);
    for (let i = 0; i <= steps; i++) {
      pixel(x1 + (steps ? Math.trunc(dx * i / steps) : 0),
        y1 + (steps ? Math.trunc(dy * i / steps) : 0));
    }
  }
  if (scene === "night_sky") {
    const stars = [
      [10, 12, 0], [29, 8, 5], [52, 16, 9], [75, 8, 2], [17, 33, 7],
      [38, 26, 12], [67, 35, 4], [86, 42, 10], [113, 38, 14], [46, 43, 1],
    ];
    const twinkle = [0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0];
    const phase = frame % 48;
    for (const [x, y, offset] of stars) {
      const radius = twinkle[(Math.trunc(phase / 3) + offset) % 16];
      line(x - radius, y, x + radius, y);
      line(x, y - radius, x, y + radius);
    }
    for (let y = -11; y <= 11; y++) {
      for (let x = -11; x <= 11; x++) {
        if (x * x + y * y <= 121 && (x - 5) ** 2 + (y + 3) ** 2 > 100) {
          pixel(102 + x, 17 + y);
        }
      }
    }
    if (phase >= 20 && phase < 32) {
      const travel = phase - 20;
      const x = 26 + travel * 4;
      const y = 19 + travel;
      line(x - 6, y - 2, x, y);
      pixel(x, y);
      pixel(x - 1, y);
      pixel(x + 1, y);
      pixel(x, y - 1);
      pixel(x, y + 1);
    }
    for (let x = 0; x < 128; x++) pixel(x, 55 + Math.trunc(Math.abs(x % 48 - 24) / 4));
  } else {
    const progress = Math.min(frame, 36);
    const rise = Math.trunc(30 * progress * progress * (108 - 2 * progress) / (36 ** 3));
    const centerY = 60 - rise;
    for (let y = -12; y <= 12; y++) {
      for (let x = -12; x <= 12; x++) {
        if (x * x + y * y <= 144 && centerY + y < 48) pixel(64 + x, centerY + y);
      }
    }
    if (frame >= 18) {
      const extension = frame < 42 ? Math.trunc((frame - 18) / 4) + 1 : 7;
      for (const [x, y] of [[-16, 0], [-14, -8], [-8, -14], [0, -16], [8, -14], [14, -8], [16, 0]]) {
        if (centerY + y < 48) {
          line(64 + x, centerY + y,
            64 + Math.trunc(x * (16 + extension) / 16),
            centerY + Math.trunc(y * (16 + extension) / 16));
        }
      }
    }
    line(8, 48, 119, 48);
    line(8, 49, 119, 49);
    line(45, 54, 83, 54);
    line(53, 59, 75, 59);
  }
  return pixels;
}

export function skyPath(scene: SkyScene, frame: number): string {
  return skyPixels(scene, frame).reduce((path, lit, index) =>
    lit ? `${path}M${index % 128} ${Math.trunc(index / 128)}h1v1h-1z` : path, "");
}
