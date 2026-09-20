import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { SLEEP_FRAMES, SLEEP_FRAME_MS, SLEEP_STILL_FRAME } from "./sleepingEyes";
import { SKY_FRAMES, SKY_FRAME_MS, SKY_STILL_FRAME } from "./skyScenes";
import { watchPreviewFrames } from "./usePreviewFrame";

const sleep = { frames: SLEEP_FRAMES, frameMs: SLEEP_FRAME_MS, stillFrame: SLEEP_STILL_FRAME };

describe("shared OLED preview lifecycle", () => {
  let reducedMotion: boolean;
  let listeners: Set<() => void>;
  let cleanups: (() => void)[];
  const changeMotionPreference = (reduced: boolean) => {
    reducedMotion = reduced;
    listeners.forEach(listener => listener());
  };
  function watch(active: boolean, timing = sleep) {
    const onFrame = vi.fn();
    const stop = watchPreviewFrames(active, timing, onFrame);
    cleanups.push(stop);
    return { onFrame, stop };
  }

  beforeEach(() => {
    vi.useFakeTimers();
    reducedMotion = false;
    listeners = new Set();
    cleanups = [];
    vi.stubGlobal("window", {
      setInterval: globalThis.setInterval,
      clearInterval: globalThis.clearInterval,
      matchMedia: vi.fn((query: string) => {
        expect(query).toBe("(prefers-reduced-motion: reduce)");
        return {
          get matches() { return reducedMotion; },
          addEventListener: (_: string, listener: () => void) => listeners.add(listener),
          removeEventListener: (_: string, listener: () => void) => listeners.delete(listener),
        };
      }),
    });
  });

  afterEach(() => {
    cleanups.forEach(cleanup => cleanup());
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it.each([
    ["sleeping", sleep],
    ...(["night_sky", "sunrise"] as const).map(scene => [scene, {
      frames: SKY_FRAMES[scene], frameMs: SKY_FRAME_MS, stillFrame: SKY_STILL_FRAME[scene],
    }] as const),
  ] as const)("plays %s once at 90 ms per frame and holds its final open-eye frame", (_, timing) => {
    const { onFrame } = watch(true, timing);
    expect(onFrame.mock.calls).toEqual([[0]]);
    vi.advanceTimersByTime(timing.frameMs - 1);
    expect(onFrame).toHaveBeenCalledTimes(1);
    vi.advanceTimersByTime(1);
    expect(onFrame).toHaveBeenLastCalledWith(1);
    vi.advanceTimersByTime((timing.frames - 2) * timing.frameMs);
    expect(onFrame.mock.calls.map(([frame]) => frame)).toEqual(
      Array.from({ length: timing.frames }, (_, frame) => frame),
    );
    expect(vi.getTimerCount()).toBe(0);
    vi.advanceTimersByTime(timing.frameMs * 10);
    expect(onFrame).toHaveBeenCalledTimes(timing.frames);
  });

  it("does not start a timer for an inactive catalog still", () => {
    const { onFrame } = watch(false);
    vi.advanceTimersByTime(4320);
    expect(onFrame).not.toHaveBeenCalled();
    expect(vi.getTimerCount()).toBe(0);
  });

  it("stops without further updates and restarts replay at frame zero", () => {
    const first = watch(true);
    vi.advanceTimersByTime(900);
    expect(first.onFrame).toHaveBeenLastCalledWith(10);
    first.stop();
    expect(listeners.size).toBe(0);
    const inactive = watch(false);
    vi.advanceTimersByTime(4320);
    expect(first.onFrame).toHaveBeenCalledTimes(11);
    expect(inactive.onFrame).not.toHaveBeenCalled();
    expect(vi.getTimerCount()).toBe(0);
    inactive.stop();
    const replay = watch(true);
    expect(replay.onFrame.mock.calls).toEqual([[0]]);
    vi.advanceTimersByTime(90);
    expect(replay.onFrame).toHaveBeenLastCalledWith(1);
    expect(vi.getTimerCount()).toBe(1);
  });

  it.each([false, true])("shows a still with no animation when motion is reduced (active: %s)", active => {
    reducedMotion = true;
    const { onFrame } = watch(active);
    expect(onFrame.mock.calls).toEqual([[SLEEP_STILL_FRAME]]);
    vi.advanceTimersByTime(4320);
    expect(onFrame).toHaveBeenCalledOnce();
    expect(vi.getTimerCount()).toBe(0);
  });

  it("handles live motion-preference changes and removes all work on unmount", () => {
    const { onFrame, stop } = watch(true);
    vi.advanceTimersByTime(900);
    changeMotionPreference(true);
    expect(onFrame).toHaveBeenLastCalledWith(SLEEP_STILL_FRAME);
    expect(vi.getTimerCount()).toBe(0);
    const count = onFrame.mock.calls.length;
    vi.advanceTimersByTime(4320);
    expect(onFrame).toHaveBeenCalledTimes(count);
    changeMotionPreference(false);
    expect(onFrame).toHaveBeenLastCalledWith(0);
    expect(vi.getTimerCount()).toBe(1);
    stop();
    expect(listeners.size).toBe(0);
    expect(vi.getTimerCount()).toBe(0);
    changeMotionPreference(true);
    vi.advanceTimersByTime(4320);
    expect(onFrame).toHaveBeenCalledTimes(count + 1);
  });
});
