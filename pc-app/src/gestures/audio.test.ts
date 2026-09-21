import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

function oscillator() {
  return {
    type: "",
    frequency: { setValueAtTime: vi.fn() },
    connect: vi.fn((node: unknown) => node),
    start: vi.fn(),
    stop: vi.fn(),
  };
}

function gainNode() {
  return {
    gain: {
      setValueAtTime: vi.fn(),
      linearRampToValueAtTime: vi.fn(),
    },
    connect: vi.fn(),
  };
}

describe("self-test frequency previews", () => {
  let oscillators: ReturnType<typeof oscillator>[];
  let gains: ReturnType<typeof gainNode>[];
  let playSound: typeof import("./audio").playSound;

  beforeEach(async () => {
    vi.resetModules();
    oscillators = [];
    gains = [];
    class MockAudioContext {
      state = "running";
      currentTime = 10;
      destination = {};
      createOscillator() {
        const node = oscillator();
        oscillators.push(node);
        return node;
      }
      createGain() {
        const node = gainNode();
        gains.push(node);
        return node;
      }
    }
    vi.stubGlobal("window", { AudioContext: MockAudioContext });
    ({ playSound } = await import("./audio"));
  });

  afterEach(() => vi.unstubAllGlobals());

  function expectTone(index: number, frequency: number, start: number, duration: number) {
    const osc = oscillators[index];
    const gain = gains[index].gain;
    expect(osc.type).toBe("sine");
    expect(osc.frequency.setValueAtTime).toHaveBeenCalledOnce();
    expect(osc.frequency.setValueAtTime.mock.calls[0][0]).toBe(frequency);
    expect(osc.frequency.setValueAtTime.mock.calls[0][1]).toBeCloseTo(start);
    expect(osc.start.mock.calls[0][0]).toBeCloseTo(start);
    expect(osc.stop.mock.calls[0][0]).toBeCloseTo(start + duration);
    expect(gain.setValueAtTime.mock.calls[0]).toEqual([0, start]);
    expect(gain.linearRampToValueAtTime.mock.calls[0][0]).toBe(0.12);
    expect(gain.linearRampToValueAtTime.mock.calls[0][1]).toBeCloseTo(start + 0.025);
    expect(gain.setValueAtTime.mock.calls[1][1]).toBeCloseTo(start + duration - 0.025);
    expect(gain.linearRampToValueAtTime.mock.calls[1][0]).toBe(0);
    expect(gain.linearRampToValueAtTime.mock.calls[1][1]).toBeCloseTo(start + duration);
  }

  it.each([
    ["tone_low", 440],
    ["tone_mid", 660],
    ["tone_high", 880],
  ] as const)("plays %s as a steady 300 ms note with soft edges", (sound, frequency) => {
    playSound(sound);
    expect(oscillators).toHaveLength(1);
    expectTone(0, frequency, 10.26, 0.3);
  });

  it("plays 440, 660, 880 Hz in order with 60 ms gaps", () => {
    playSound("tone_rise");
    expect(oscillators).toHaveLength(3);
    [440, 660, 880].forEach((frequency, index) => {
      expectTone(index, frequency, 10.26 + index * 0.26, 0.2);
    });
  });
});
