import { describe, expect, it } from "vitest";
import { CATALOG, CATALOG_BY_ID, gesturesByMedium } from "./catalog";

describe("affectionate eye previews", () => {
  it.each([
    ["oled.lovey_dovey", "Lovey-dovey eyes", "lovey_dovey"],
    ["oled.shy", "Shy eyes", "shy"],
  ])("registers %s with firmware timing and the correct preview", (id, name, eyes) => {
    const gesture = CATALOG_BY_ID[id];
    expect(gesture).toEqual({
      id, name, device: "oled", medium: "oled", ms: 48 * 90,
      sim: { oled: { kind: "eyes", eyes } },
    });

    expect(gesturesByMedium("oled")).toContain(gesture);
    expect(CATALOG.filter((entry) => entry.id === id)).toHaveLength(1);
  });
});

describe("self-test tone previews", () => {
  it.each([
    ["tone_low", 800],
    ["tone_mid", 800],
    ["tone_high", 800],
    ["tone_rise", 1220],
  ])("registers %s with the firmware duration", (sound, ms) => {
    const id = `speaker.${sound}`;
    const gesture = CATALOG_BY_ID[id];
    expect(gesture.device).toBe("speaker");
    expect(gesture.ms).toBe(ms);
    expect(gesture.sim).toEqual({ speaker: { sound } });
    expect(gesturesByMedium("speaker")).toContain(gesture);
    expect(CATALOG.filter((entry) => entry.id === id)).toHaveLength(1);
  });

  it.each(["sigh", "boing", "downstep", "bark"])("does not offer the retired %s action", (sound) => {
    expect(CATALOG_BY_ID[`speaker.${sound}`]).toBeUndefined();
  });
});
