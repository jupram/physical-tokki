export const SCROLLING_TEXT_ID = "oled.scrolling_text";
export const DEFAULT_SCROLLING_TEXT = "Hello from Tokki!";
export const MAX_SCROLLING_TEXT_LENGTH = 50;

export function scrollingTextError(text: string): string | null {
  if (text.length < 1 || text.length > MAX_SCROLLING_TEXT_LENGTH) {
    return "Enter 1–50 printable ASCII characters.";
  }
  if (!/^[\x20-\x7e]+$/.test(text)) {
    return "Use printable ASCII only (spaces, letters, numbers, and punctuation).";
  }
  return null;
}

export function scrollingTextDuration(text: string): number {
  return Math.ceil((128 + (text.length * 12 - 2)) / 2) * 45;
}
