// Mirrors the firmware gesture registry (components/tokki_gestures). Until the
// serial `actions.list` method exists, the desktop app carries the same catalog
// so bundles can be composed and previewed offline.

export type Medium = "oled" | "led" | "speaker";
export type DeviceKind = "oled" | "speaker" | "led" | "neopixel";

export type OledEyes =
  | "happy"
  | "sad"
  | "curious"
  | "surprised"
  | "wink"
  | "look_left"
  | "look_right"
  | "look_up"
  | "look_down"
  | "sleepy"
  | "blink";

export type OledArt =
  | "drink_water"
  | "water_drop"
  | "fire"
  | "checkmark"
  | "thinking"
  | "heart"
  | "exclamation";

export type OledSim =
  | { kind: "eyes"; eyes: OledEyes }
  | { kind: "art"; art: OledArt }
  | { kind: "marquee"; text: string };

export type LedEffect = "solid" | "blink" | "breathe" | "rainbow";
export type LedSim = { effect: LedEffect; color: string };

export type SpeakerSound =
  | "drink_water"
  | "chirp"
  | "alert"
  | "chime"
  | "ping"
  | "dog_bark"
  | "bubble"
  | "whistle"
  | "sigh"
  | "boing"
  | "question"
  | "downstep"
  | "sparkle"
  | "trill"
  | "bark"
  | "knock"
  | "sonar";
export type SpeakerSim = { sound: SpeakerSound };

export type GestureSim = { oled?: OledSim; led?: LedSim; speaker?: SpeakerSim };

export type GestureDef = {
  id: string;
  name: string;
  device: DeviceKind;
  medium: Medium;
  ms: number;
  sim: GestureSim;
};

export type Bundle = {
  id: string;
  name: string;
  lanes: Record<Medium, string[]>;
};

export const MEDIA: Medium[] = ["oled", "led", "speaker"];

export const MEDIUM_LABEL: Record<Medium, string> = {
  oled: "OLED",
  led: "LED",
  speaker: "Speaker",
};

function eyes(id: string, name: string, expr: OledEyes, ms: number): GestureDef {
  return { id, name, device: "oled", medium: "oled", ms, sim: { oled: { kind: "eyes", eyes: expr } } };
}

function art(id: string, name: string, artName: OledArt, ms: number): GestureDef {
  return { id, name, device: "oled", medium: "oled", ms, sim: { oled: { kind: "art", art: artName } } };
}

function led(
  id: string,
  name: string,
  device: DeviceKind,
  effect: LedEffect,
  color: string,
  ms: number,
): GestureDef {
  return { id, name, device, medium: "led", ms, sim: { led: { effect, color } } };
}

function speaker(id: string, name: string, sound: SpeakerSound, ms: number): GestureDef {
  return { id, name, device: "speaker", medium: "speaker", ms, sim: { speaker: { sound } } };
}

export const CATALOG: GestureDef[] = [
  // LED / status
  led("led.blink", "Blink status LED", "led", "blink", "#ff3b30", 1200),

  // NeoPixel (RGB) effects
  led("neopixel.rainbow", "Rainbow", "neopixel", "rainbow", "rainbow", 2560),
  led("neopixel.blink_red", "Blink red", "neopixel", "blink", "#ff2a2a", 1200),
  led("neopixel.blink_yellow", "Blink yellow", "neopixel", "blink", "#ffd21e", 1200),
  led("neopixel.blink_green", "Blink green", "neopixel", "blink", "#33d15b", 1200),
  led("neopixel.breathe_teal", "Breathe teal", "neopixel", "breathe", "#1fd1c4", 1320),
  led("neopixel.pulse_blue", "Pulse blue", "neopixel", "breathe", "#2f6bff", 720),

  // OLED eyes
  eyes("oled.happy", "Happy eyes", "happy", 2880),
  eyes("oled.sad", "Sad eyes", "sad", 2880),
  eyes("oled.surprised", "Surprised eyes", "surprised", 2880),
  eyes("oled.blink", "Blink eyes", "blink", 540),
  eyes("oled.curious", "Curious eyes", "curious", 2880),
  eyes("oled.wink", "Wink", "wink", 600),
  eyes("oled.look_left", "Look left", "look_left", 1440),
  eyes("oled.look_right", "Look right", "look_right", 1440),
  eyes("oled.look_up", "Look up", "look_up", 1440),
  eyes("oled.look_down", "Look down", "look_down", 1440),
  eyes("oled.sleepy", "Sleepy eyes", "sleepy", 1440),

  // OLED art
  art("oled.drink_water", "Drink water message", "drink_water", 2000),
  art("oled.water_drop", "Water drop", "water_drop", 1440),
  art("oled.fire", "Fire", "fire", 1440),
  art("oled.checkmark", "Checkmark", "checkmark", 1020),
  art("oled.thinking", "Thinking dots", "thinking", 1200),
  art("oled.heart", "Heart pulse", "heart", 1440),
  art("oled.exclamation", "Exclamation mark", "exclamation", 1020),

  // Speaker
  speaker("speaker.drink_water", "Drink water phrase", "drink_water", 1400),
  speaker("speaker.chirp", "Bird-like chirp", "chirp", 700),
  speaker("speaker.alert", "Short alert tone", "alert", 900),
  speaker("speaker.chime", "Completion chime", "chime", 1100),
  speaker("speaker.ping", "Short ping", "ping", 500),
  speaker("speaker.dog_bark", "Dog bark (recording, twice)", "dog_bark", 1020),
  speaker("speaker.bubble", "Bubble pop (synthesized)", "bubble", 590),
  speaker("speaker.whistle", "Rising whistle (synthesized)", "whistle", 780),
  speaker("speaker.sigh", "Sleepy sigh (synthesized)", "sigh", 860),
  speaker("speaker.boing", "Boing (synthesized)", "boing", 740),
  speaker("speaker.question", "Question cue (synthesized)", "question", 820),
  speaker("speaker.downstep", "Gentle down-step (synthesized)", "downstep", 860),
  speaker("speaker.sparkle", "Sparkle (synthesized)", "sparkle", 830),
  speaker("speaker.trill", "Trill (synthesized)", "trill", 760),
  speaker("speaker.bark", "Dog bark (CC0 recording, once)", "bark", 1000),
  speaker("speaker.knock", "Knock-knock (synthesized)", "knock", 730),
  speaker("speaker.sonar", "Sonar echo (synthesized)", "sonar", 850),
];

export const CATALOG_BY_ID: Record<string, GestureDef> = Object.fromEntries(
  CATALOG.map((gesture) => [gesture.id, gesture]),
);

export function gesturesByMedium(medium: Medium): GestureDef[] {
  return CATALOG.filter((gesture) => gesture.medium === medium);
}
