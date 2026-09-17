import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import type { GestureDef } from "./catalog";
import { IDLE_PREVIEW, type PreviewState } from "./GesturePreview";
import { playBundle, type LaneGestures } from "./player";

function gesture(id: string, medium: GestureDef["medium"], ms: number): GestureDef {
  const device = medium === "led" ? "neopixel" : medium;
  const sim = medium === "oled"
    ? { oled: { kind: "eyes" as const, eyes: "happy" as const } }
    : medium === "led"
      ? { led: { effect: "solid" as const, color: "#ffffff" } }
      : { speaker: { sound: "ping" as const } };
  return { id, name: id, device, medium, ms, sim };
}

describe("bundle player", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.stubGlobal("window", globalThis);
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it("starts separate medium lanes together and dispatches each gesture", () => {
    const oledOne = gesture("oled.one", "oled", 100);
    const oledTwo = gesture("oled.two", "oled", 200);
    const ledOne = gesture("neopixel.one", "led", 300);
    const lanes: LaneGestures = {
      oled: [oledOne, oledTwo],
      led: [ledOne],
      speaker: [],
    };
    let state: PreviewState = IDLE_PREVIEW;
    const started: string[] = [];
    const done = vi.fn();

    playBundle(
      lanes,
      (update) => {
        state = update(state);
      },
      done,
      (startedGesture) => started.push(startedGesture.id),
    );

    vi.advanceTimersByTime(0);
    expect(started).toEqual(["oled.one", "neopixel.one"]);
    expect(state.oled.active).toBe(true);
    expect(state.led.active).toBe(true);

    vi.advanceTimersByTime(220);
    expect(started).toEqual(["oled.one", "neopixel.one", "oled.two"]);
    expect(done).not.toHaveBeenCalled();

    vi.advanceTimersByTime(380);
    expect(done).toHaveBeenCalledOnce();
  });
});
