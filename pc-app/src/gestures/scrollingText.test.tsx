import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it, vi } from "vitest";
import { CATALOG_BY_ID, scrollingTextGesture } from "./catalog";
import { MediumPreview } from "./GesturePreview";
import { ScrollingTextInput } from "./ScrollingTextInput";
import { DEFAULT_SCROLLING_TEXT, SCROLLING_TEXT_ID, scrollingTextDuration, scrollingTextError } from "./scrollingText";

describe("Scrolling text", () => {
  it("offers the firmware default as a normal catalog action", () => {
    expect(CATALOG_BY_ID[SCROLLING_TEXT_ID]).toEqual({
      id: "oled.scrolling_text", name: "Scrolling text", device: "oled", medium: "oled",
      ms: 7425, sim: { oled: { kind: "marquee", text: "Hello from Tokki!" } },
    });
    expect(scrollingTextGesture()).toEqual(CATALOG_BY_ID[SCROLLING_TEXT_ID]);
  });

  it.each([
    ["A", 3105],
    [DEFAULT_SCROLLING_TEXT, 7425],
    ["A".repeat(50), 16335],
  ])("uses the exact firmware scroll duration for %s", (text, ms) => {
    expect(scrollingTextDuration(text)).toBe(ms);
    expect(scrollingTextGesture(text).ms).toBe(ms);
    const markup = renderToStaticMarkup(<MediumPreview medium="oled" lane={{
      label: "Scrolling text", active: true, sim: { kind: "marquee", text },
    }} />);
    expect(markup).toContain(`--marquee-duration:${ms}ms`);
    expect(markup).toContain("animation-play-state:running");
  });

  it.each(["", "a".repeat(51), "Café", "New\nline", "Tab\ttext", "\x00", "\x1f", "\x7f", "🙂"])("rejects invalid manual input without truncating or normalizing: %j", (text) => {
    expect(scrollingTextError(text)).toBeTruthy();
    expect(() => scrollingTextGesture(text)).toThrow(/ASCII/);
    const markup = renderToStaticMarkup(<ScrollingTextInput id="test-title" value={text} onChange={() => {}} />);
    expect(markup).toContain('aria-invalid="true"');
    expect(markup).toContain('role="alert"');
    expect(markup).toContain("test-title-error");
    expect(markup).not.toContain("maxLength");
    expect(markup).not.toContain("maxlength");
  });

  it.each([" ", " ~", 'Build "42" \\ ready!', "A".repeat(50)])("preserves valid printable ASCII: %j", (text) => {
    expect(scrollingTextError(text)).toBeNull();
    expect(scrollingTextGesture(text).sim.oled).toEqual({ kind: "marquee", text });
  });

  it("passes the full entered value through the reusable input", () => {
    const onChange = vi.fn();
    const field = ScrollingTextInput({ id: "test", value: DEFAULT_SCROLLING_TEXT, onChange });
    const input = field.props.children[1];
    const value = "x".repeat(51);
    input.props.onChange({ target: { value } });
    expect(onChange).toHaveBeenCalledExactlyOnceWith(value);
  });
});
