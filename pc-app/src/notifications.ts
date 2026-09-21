import { CATALOG_BY_ID, scrollingTextGesture } from "./gestures/catalog";
import type { LaneGestures } from "./gestures/player";
import type { QueuedNotification } from "./native";

export const NOTIFICATION_PRESETS: QueuedNotification[] = [
  { source: "Microsoft Teams", text: "Alex sent a new message", oled: "oled.curious", sound: "speaker.trill", light: "neopixel.rainbow" },
  { source: "Outlook mail", text: "Email :  Project update received", oled: "oled.happy", sound: "speaker.chime", light: "neopixel.pulse_blue" },
  { source: "Outlook meeting reminder", text: "Design review starts soon", oled: "oled.surprised", sound: "speaker.whistle", light: "neopixel.blink_yellow" },
];

export function notificationLanes(item: QueuedNotification): LaneGestures {
  const eyes = CATALOG_BY_ID[item.oled];
  const sound = CATALOG_BY_ID[item.sound];
  const light = CATALOG_BY_ID[item.light];
  if (eyes?.sim.oled?.kind !== "eyes" || sound?.medium !== "speaker" || light?.device !== "neopixel") {
    throw new Error("Notification contains an unavailable gesture.");
  }
  return {
    oled: [eyes, scrollingTextGesture(item.text)],
    speaker: [sound],
    led: [light],
  };
}

export function notificationDescription(item: QueuedNotification): string {
  const name = (id: string) => CATALOG_BY_ID[id]?.name ?? id;
  return `${name(item.oled)} → Scrolling text · ${name(item.sound)} · ${name(item.light)}`;
}
