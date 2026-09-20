// Approximates the locally previewable firmware sounds with Web Audio so
// bundles can be previewed. A single AudioContext is created lazily after a user
// gesture to satisfy autoplay policies.

import type { SpeakerSound } from "./catalog";

let context: AudioContext | null = null;

function getContext(): AudioContext | null {
  if (typeof window === "undefined") return null;
  if (!context) {
    const Ctor =
      window.AudioContext ??
      (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!Ctor) return null;
    context = new Ctor();
  }
  return context;
}

function tone(ctx: AudioContext, start: number, freq: number, ms: number, gain = 0.12, fadeMs?: number) {
  const osc = ctx.createOscillator();
  const env = ctx.createGain();
  const end = start + ms / 1000;
  osc.type = "sine";
  osc.frequency.setValueAtTime(freq, start);
  env.gain.setValueAtTime(0, start);
  env.gain.linearRampToValueAtTime(gain, start + (fadeMs ?? 12) / 1000);
  env.gain.setValueAtTime(gain, end - (fadeMs ?? 20) / 1000);
  env.gain.linearRampToValueAtTime(0, end);
  osc.connect(env).connect(ctx.destination);
  osc.start(start);
  osc.stop(end);
}

function chirp(ctx: AudioContext, t: number) {
  const osc = ctx.createOscillator();
  const env = ctx.createGain();
  osc.type = "sine";
  osc.frequency.setValueAtTime(1400, t);
  osc.frequency.exponentialRampToValueAtTime(2600, t + 0.12);
  osc.frequency.exponentialRampToValueAtTime(1800, t + 0.24);
  env.gain.setValueAtTime(0, t);
  env.gain.linearRampToValueAtTime(0.1, t + 0.02);
  env.gain.linearRampToValueAtTime(0, t + 0.26);
  osc.connect(env).connect(ctx.destination);
  osc.start(t);
  osc.stop(t + 0.28);
}

// Plays the requested sound; returns a stop function.
export function playSound(sound: SpeakerSound): () => void {
  if (sound === "drink_water") {
    if (typeof window !== "undefined" && window.speechSynthesis) {
      window.speechSynthesis.cancel();
      window.speechSynthesis.speak(new SpeechSynthesisUtterance("Time to drink water"));
    }
    return () => window.speechSynthesis?.cancel();
  }

  const ctx = getContext();
  if (!ctx) return () => {};
  if (ctx.state === "suspended") void ctx.resume();
  const t = ctx.currentTime + 0.01;

  switch (sound) {
    case "chirp":
      chirp(ctx, t);
      break;
    case "alert":
      tone(ctx, t, 880, 120);
      tone(ctx, t + 0.16, 880, 120);
      tone(ctx, t + 0.32, 880, 120);
      break;
    case "chime":
      tone(ctx, t, 523, 180);
      tone(ctx, t + 0.16, 659, 180);
      tone(ctx, t + 0.32, 784, 320);
      break;
    case "ping":
      tone(ctx, t, 1200, 140, 0.14);
      break;
    case "tone_low":
    case "tone_mid":
    case "tone_high": {
      const frequencies = { tone_low: 440, tone_mid: 660, tone_high: 880 };
      tone(ctx, t + 0.25, frequencies[sound], 300, 0.12, 25);
      break;
    }
    case "tone_rise":
      [440, 660, 880].forEach((frequency, index) => {
        tone(ctx, t + 0.25 + index * 0.26, frequency, 200, 0.12, 25);
      });
      break;
  }
  return () => {};
}

export function cancelSpeech() {
  if (typeof window !== "undefined" && window.speechSynthesis) {
    window.speechSynthesis.cancel();
  }
}
