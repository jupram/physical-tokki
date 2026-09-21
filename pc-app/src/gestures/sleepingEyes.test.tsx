import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { CATALOG_BY_ID } from "./catalog";
import { MediumPreview } from "./GesturePreview";
import { MediumStudio } from "./MediumStudio";
import { SleepingFrame, SleepingPreview } from "./SleepingPreview";
import { SLEEP_FRAMES, SLEEP_STILL_FRAME, sleepingEyePath, sleepingPose } from "./sleepingEyes";

describe("sleeping eye poses", () => {
  it("eases closed by frame 8, holds through 39, and is open again at 46–47", () => {
    const poses = Array.from({ length: SLEEP_FRAMES }, (_, frame) => sleepingPose(frame));
    expect(poses[0].closure).toBe(0);
    expect(poses[4].closure).toBe(0.5);
    expect(poses[1].closure).toBeLessThan(1 / 8);
    expect(poses[7].closure).toBeGreaterThan(7 / 8);
    for (let frame = 1; frame <= 8; frame++) {
      expect(poses[frame].closure).toBeGreaterThan(poses[frame - 1].closure);
    }
    for (let frame = 8; frame <= 39; frame++) expect(poses[frame].closure).toBe(1);
    for (let frame = 40; frame <= 46; frame++) {
      expect(poses[frame].closure).toBeLessThan(poses[frame - 1].closure);
    }
    expect(poses[46].closure).toBe(0);
    expect(poses[47].closure).toBe(0);
    expect(sleepingPose(-1)).toEqual(poses[0]);
    expect(sleepingPose(48)).toEqual(poses[47]);
  });

  it("uses lower happy-style crescents with at most a one-pixel breathing bob", () => {
    expect(sleepingEyePath(44, 0)).toBe("M32 32C32 12 56 12 56 32C56 52 32 52 32 32Z");
    expect(sleepingEyePath(44, 1)).toBe("M32 40C32 28 56 28 56 40C56 34 32 34 32 40Z");
    const bobs = Array.from({ length: SLEEP_FRAMES }, (_, frame) => sleepingPose(frame).bob);
    expect(Math.max(...bobs)).toBe(1);
    expect(Math.min(...bobs)).toBe(-1);
    expect(bobs[0]).toBe(0);
    expect(bobs[47]).toBe(0);
  });

  it("floats three staggered integer-sized Zs up-right only during frames 8–39", () => {
    for (let frame = 0; frame < SLEEP_FRAMES; frame++) {
      const { zeds } = sleepingPose(frame);
      if (frame < 8 || frame > 39) {
        expect(zeds).toEqual([]);
        continue;
      }
      expect(zeds).toHaveLength(3);
      zeds.forEach((zed, index) => {
        const age = (frame - 8 + index * 8) % 24;
        expect(zed).toEqual({
          x: 78 + age, y: 22 - Math.trunc(age * 3 / 4), size: 3 + Math.trunc(age / 6),
        });
        expect(zed.x).toBeGreaterThan(64);
        expect(zed.y + zed.size).toBeLessThan(31);
      });
    }
    expect(sleepingPose(8).zeds).toEqual([
      { x: 78, y: 22, size: 3 }, { x: 86, y: 16, size: 4 }, { x: 94, y: 10, size: 5 },
    ]);
    expect(sleepingPose(32).zeds).toEqual(sleepingPose(8).zeds);
  });
});

describe("sleeping preview surfaces", () => {
  it("shows a labeled monochrome crescent-and-Zzz still in the OLED preview", () => {
    const gesture = CATALOG_BY_ID["oled.sleeping"];
    const markup = renderToStaticMarkup(<MediumPreview medium="oled" lane={{
      sim: gesture.sim.oled, active: false, label: gesture.name,
    }} />);
    expect(markup).toContain("Sleeping eyes (Zzz)");
    expect(markup).toContain('viewBox="0 0 128 64"');
    expect(markup).toContain('role="img"');
    expect(markup).toContain('aria-label="Sleeping eyes with closed crescent lids and Zzz"');
    expect(markup).toContain(`data-frame="${SLEEP_STILL_FRAME}"`);
    expect(markup.match(/class="pv-sleep-z"/g)).toHaveLength(3);
    expect(markup).toContain(`d="${sleepingEyePath(44, 1)}"`);
    expect(markup).not.toContain("<text");
    expect(markup.match(/<svg class="pv-sleeping"[^>]*>/)?.[0]).not.toContain('aria-hidden="true"');
  });

  it("starts active playback with open eyes and restores those same paths on completion", () => {
    const initial = renderToStaticMarkup(<SleepingPreview active />);
    const finished = renderToStaticMarkup(<SleepingFrame frame={47} />);
    expect(initial).toContain('data-frame="0"');
    expect(finished).toContain('aria-label="Restored open eyes"');
    expect(finished).not.toContain("pv-sleep-z");
    for (const center of [44, 84]) {
      expect(initial).toContain(`d="${sleepingEyePath(center, 0)}"`);
      expect(finished).toContain(`d="${sleepingEyePath(center, 0)}"`);
    }
  });

  it("offers the new action alongside the existing sky scenes in the gesture picker", () => {
    const markup = renderToStaticMarkup(<MediumStudio medium="oled" />);
    expect(markup).toContain("23 gestures");
    expect(markup).toContain('<option value="oled.sleeping">Sleeping eyes (Zzz)</option>');
    expect(markup).toContain('<option value="oled.night_sky">Night sky</option>');
    expect(markup).toContain('<option value="oled.sunrise">Sunrise</option>');
  });
});
