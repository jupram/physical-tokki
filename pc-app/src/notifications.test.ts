import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { CATALOG_BY_ID } from "./gestures/catalog";
import { IDLE_PREVIEW, type PreviewState } from "./gestures/GesturePreview";
import { bundleDuration, playBundle } from "./gestures/player";
import { scrollingTextDuration } from "./gestures/scrollingText";
import { NOTIFICATION_PRESETS, notificationDescription, notificationLanes } from "./notifications";
import { playSound } from "./gestures/audio";

vi.mock("./gestures/audio", () => ({ playSound: vi.fn(), cancelSpeech: vi.fn() }));

describe("fixed notification routing previews", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.stubGlobal("window", globalThis);
    vi.clearAllMocks();
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it("uses the approved qualified IDs and labels the email subject", () => {
    expect(NOTIFICATION_PRESETS).toEqual([
      { source: "Microsoft Teams", text: "Alex sent a new message", oled: "oled.curious", sound: "speaker.trill", light: "neopixel.rainbow" },
      { source: "Outlook mail", text: "Email :  Project update received", oled: "oled.happy", sound: "speaker.chime", light: "neopixel.pulse_blue" },
      { source: "Outlook meeting reminder", text: "Design review starts soon", oled: "oled.surprised", sound: "speaker.whistle", light: "neopixel.blink_yellow" },
    ]);
  });

  it.each([
    ["oled.curious", 4320], ["oled.happy", 4320], ["oled.surprised", 4320],
    ["neopixel.rainbow", 3840], ["neopixel.pulse_blue", 1020], ["neopixel.blink_yellow", 1800],
    ["speaker.trill", 760], ["speaker.chime", 960], ["speaker.whistle", 780],
  ])("matches firmware duration for %s", (id, ms) => {
    expect(CATALOG_BY_ID[id].ms).toBe(ms);
  });

  it.each(NOTIFICATION_PRESETS)("plays $source eyes first, with sound/light alongside, then the formatted text", (item) => {
    const original = structuredClone(item);
    const lanes = notificationLanes(item);
    expect(lanes.oled[0]).toBe(CATALOG_BY_ID[item.oled]);
    expect(lanes.speaker[0]).toBe(CATALOG_BY_ID[item.sound]);
    expect(lanes.led[0]).toBe(CATALOG_BY_ID[item.light]);
    expect(lanes.led[0].sim.led?.effect).not.toBe("solid");
    expect(lanes.oled[1]).toMatchObject({
      id: "oled.scrolling_text", name: "Scrolling text",
      ms: scrollingTextDuration(item.text), sim: { oled: { kind: "marquee", text: item.text } },
    });
    let state: PreviewState = IDLE_PREVIEW;
    const done = vi.fn();
    playBundle(lanes, (update) => { state = update(state); }, done);
    vi.advanceTimersByTime(0);
    expect(state.oled.sim).toBe(CATALOG_BY_ID[item.oled].sim.oled);
    expect(state.oled.active).toBe(true);
    expect(state.led.active).toBe(true);
    expect(state.speaker.active).toBe(true);
    expect(playSound).toHaveBeenCalledExactlyOnceWith(CATALOG_BY_ID[item.sound].sim.speaker!.sound);
    vi.advanceTimersByTime(4320);
    expect(state.oled.active).toBe(false);
    expect(state.speaker.active).toBe(false);
    expect(state.led.active).toBe(false);
    vi.advanceTimersByTime(120);
    expect(state.oled.sim).toEqual({ kind: "marquee", text: item.text });
    expect(state.oled.active).toBe(true);
    vi.advanceTimersByTime(scrollingTextDuration(item.text));
    expect(state.oled.active).toBe(false);
    vi.runAllTimers();
    expect(done).toHaveBeenCalledOnce();
    expect(item).toEqual(original);
    expect(notificationDescription(item)).toContain(" → Scrolling text");
  });

  it("stopping a preview prevents the title phase and completion callback", () => {
    const lanes = notificationLanes(NOTIFICATION_PRESETS[0]);
    const update = vi.fn();
    const done = vi.fn();
    const stop = playBundle(lanes, update, done);
    vi.advanceTimersByTime(0);
    stop();
    update.mockClear();
    vi.advanceTimersByTime(bundleDuration(lanes) + 60);
    expect(update).not.toHaveBeenCalled();
    expect(done).not.toHaveBeenCalled();
    expect(vi.getTimerCount()).toBe(0);
  });

  it("preserves a 50-character queued title and rejects malformed snapshots explicitly", () => {
    const item = { ...NOTIFICATION_PRESETS[0], text: "x".repeat(50) };
    expect(notificationLanes(item).oled[1].sim.oled).toEqual({ kind: "marquee", text: item.text });
    expect(() => notificationLanes({ ...item, text: "x".repeat(51) })).toThrow(/ASCII/);
    expect(() => notificationLanes({ ...item, sound: "trill" })).toThrow(/unavailable/);
  });

  it("preserves the email prefix and spacing without adding another label", () => {
    const item = { ...NOTIFICATION_PRESETS[1], text: `Email :  ${"x".repeat(41)}` };
    expect(item.text).toHaveLength(50);
    expect(notificationLanes(item).oled[1].sim.oled).toEqual({ kind: "marquee", text: item.text });
  });
});
