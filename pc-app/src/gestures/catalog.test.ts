import { describe, expect, it } from "vitest";
import { CATALOG, CATALOG_BY_ID, gesturesByMedium } from "./catalog";
import { SLEEP_FRAMES, SLEEP_FRAME_MS } from "./sleepingEyes";

describe("offline gesture catalog", () => {
  it("mirrors the firmware action counts with unique IDs", () => {
    const counts = CATALOG.reduce<Record<string, number>>((result, gesture) => {
      result[gesture.device] = (result[gesture.device] ?? 0) + 1;
      return result;
    }, {});

    expect(CATALOG).toHaveLength(48);
    expect(new Set(CATALOG.map((gesture) => gesture.id))).toHaveLength(48);
    expect(counts).toEqual({
      oled: 24,
      neopixel: 6,
      led: 1,
      speaker: 17,
    });
  });

  it("includes every added speaker action", () => {
    const addedSpeakerIds = [
      "speaker.dog_bark",
      "speaker.bubble",
      "speaker.whistle",
      "speaker.question",
      "speaker.sparkle",
      "speaker.trill",
      "speaker.knock",
      "speaker.sonar",
    ];

    for (const id of addedSpeakerIds) {
      expect(CATALOG_BY_ID[id]?.device).toBe("speaker");
    }
  });

});

describe("sleeping eye preview", () => {
  it("registers the distinct sleeping action in the 24-item OLED catalog", () => {
    expect(CATALOG_BY_ID["oled.sleeping"]).toEqual({
      id: "oled.sleeping", name: "Sleeping eyes (Zzz)", device: "oled", medium: "oled",
      ms: SLEEP_FRAMES * SLEEP_FRAME_MS,
      sim: { oled: { kind: "eyes", eyes: "sleeping" } },
    });
    expect(SLEEP_FRAMES * SLEEP_FRAME_MS).toBe(4320);
    expect(CATALOG.filter(entry => entry.id === "oled.sleeping")).toHaveLength(1);
    expect(gesturesByMedium("oled")).toHaveLength(24);
    expect(gesturesByMedium("oled")).toContain(CATALOG_BY_ID["oled.sleeping"]);
    expect(CATALOG_BY_ID["oled.sleepy"].sim.oled).toEqual({ kind: "eyes", eyes: "sleepy" });
  });
});

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

  describe("OLED sky scenes", () => {
    it.each([
      ["night_sky", "Night sky", 4320],
      ["sunrise", "Sunrise", 5760],
    ] as const)("registers %s with firmware timing", (art, name, ms) => {
      const id = `oled.${art}`;
      expect(CATALOG_BY_ID[id]).toEqual({
        id, name, device: "oled", medium: "oled", ms,
        sim: { oled: { kind: "art", art } },
      });
      expect(gesturesByMedium("oled")).toContain(CATALOG_BY_ID[id]);
      expect(CATALOG.filter(entry => entry.id === id)).toHaveLength(1);
    });
  });

  it.each(["sigh", "boing", "downstep", "bark"])("does not offer the retired %s action", (sound) => {
    expect(CATALOG_BY_ID[`speaker.${sound}`]).toBeUndefined();
  });
});
