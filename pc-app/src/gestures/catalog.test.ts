import { describe, expect, it } from "vitest";
import { CATALOG, CATALOG_BY_ID } from "./catalog";

describe("offline gesture catalog", () => {
  it("mirrors the firmware action counts with unique IDs", () => {
    const counts = CATALOG.reduce<Record<string, number>>((result, gesture) => {
      result[gesture.device] = (result[gesture.device] ?? 0) + 1;
      return result;
    }, {});

    expect(CATALOG).toHaveLength(42);
    expect(new Set(CATALOG.map((gesture) => gesture.id))).toHaveLength(42);
    expect(counts).toEqual({
      oled: 18,
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
      "speaker.sigh",
      "speaker.boing",
      "speaker.question",
      "speaker.downstep",
      "speaker.sparkle",
      "speaker.trill",
      "speaker.bark",
      "speaker.knock",
      "speaker.sonar",
    ];

    for (const id of addedSpeakerIds) {
      expect(CATALOG_BY_ID[id]?.device).toBe("speaker");
    }
  });

});