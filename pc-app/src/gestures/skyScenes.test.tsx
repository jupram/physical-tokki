import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { CATALOG_BY_ID } from "./catalog";
import { MediumPreview } from "./GesturePreview";
import { SKY_FRAMES, SKY_FRAME_MS, skyPixels } from "./skyScenes";

describe("monochrome sky previews", () => {
  it.each([
    ["night_sky", 0xa5d255a7],
    ["sunrise", 0x44639cec],
  ] as const)("matches the firmware's full %s scene sequence", (scene, expectedHash) => {
    // FNV-1a over page-packed frames; also pinned in tests/host/test_gestures.c.
    let hash = 2166136261;
    for (let frame = 0; frame < SKY_FRAMES[scene] - 1; frame++) {
      const pixels = skyPixels(scene, frame);
      for (let page = 0; page < 8; page++) {
        for (let x = 0; x < 128; x++) {
          let byte = 0;
          for (let bit = 0; bit < 8; bit++) byte |= pixels[(page * 8 + bit) * 128 + x] << bit;
          hash = Math.imul(hash ^ byte, 16777619) >>> 0;
        }
      }
    }
    expect(hash).toBe(expectedHash);
  });

  it.each(["night_sky", "sunrise"] as const)("renders %s as pixel art, not text or emoji", scene => {
    const gesture = CATALOG_BY_ID[`oled.${scene}`];
    const markup = renderToStaticMarkup(<MediumPreview medium="oled" lane={{
      sim: gesture.sim.oled, active: false, label: gesture.name,
    }} />);
    expect(markup).toContain('viewBox="0 0 128 64"');
    expect(markup).toContain('class="pv-sky"');
    expect(markup).toContain('<path d="M');
    expect(gesture.ms).toBe(SKY_FRAMES[scene] * SKY_FRAME_MS);
    for (let frame = 0; frame < SKY_FRAMES[scene] - 1; frame++) {
      const pixels = skyPixels(scene, frame);
      expect(pixels.length).toBe(128 * 64);
      expect(pixels.every(value => value === 0 || value === 1)).toBe(true);
      const lit = pixels.reduce((sum, value) => sum + value, 0);
      expect(lit).toBeGreaterThan(100);
      expect(lit).toBeLessThan(1000);
    }
  });

  it("keeps the moon still while stars twinkle and a shooting star passes", () => {
    const still = skyPixels("night_sky", 0);
    const bright = skyPixels("night_sky", 12);
    expect(still[12 * 128 + 10]).toBe(1);
    expect(still[12 * 128 + 8]).toBe(0);
    expect(bright[12 * 128 + 8]).toBe(1);
    expect(still[17 * 128 + 91]).toBe(1);
    expect(still[17 * 128 + 102]).toBe(0);
    expect(skyPixels("night_sky", 20)[19 * 128 + 26]).toBe(1);
    expect(skyPixels("night_sky", 31)[30 * 128 + 70]).toBe(1);
    expect(skyPixels("night_sky", 32)[30 * 128 + 70]).toBe(0);
    expect(skyPixels("night_sky", 48)).toEqual(still);
  });

  it("raises the sun monotonically, clips it to the horizon, then holds", () => {
    let top = 48;
    for (let frame = 0; frame < 64; frame++) {
      const pixels = skyPixels("sunrise", frame);
      const nextTop = Array.from({ length: 64 }, (_, y) => pixels[y * 128 + 64]).indexOf(1);
      expect(nextTop).toBeLessThanOrEqual(top);
      expect(nextTop).toBeGreaterThanOrEqual(7);
      top = nextTop;
      expect(pixels[48 * 128 + 64]).toBe(1);
      expect(pixels[50 * 128 + 64]).toBe(0);
    }
    expect(skyPixels("sunrise", 0).slice(0, 128 * 48).every(value => value === 0)).toBe(true);
    expect(skyPixels("sunrise", 42)).toEqual(skyPixels("sunrise", 62));
    expect(skyPixels("sunrise", 42)).toEqual(skyPixels("sunrise", 0xffffffff));
  });
});
