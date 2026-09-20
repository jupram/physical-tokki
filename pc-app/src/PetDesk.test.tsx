import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { PetConnectionBadge, PetWelcome } from "./PetDesk";
import type { Snapshot } from "./native";

function badge(status: Snapshot["status"], native = true, port: string | null = null) {
  return renderToStaticMarkup(
    <PetConnectionBadge status={status} native={native} port={port} onOpen={() => {}} />,
  );
}

describe("pet control desk connection indicator", () => {
  it("shows the connected icon only after native discovery completes", () => {
    const markup = badge("connected", true, "COM12");
    expect(markup).toContain("is-connected");
    expect(markup).not.toContain("is-disconnected");
    expect(markup).toContain("Connected · COM12");
    expect(markup).toContain('role="status"');
    expect(markup).toContain('aria-live="polite"');
    expect(markup).toContain("Open device settings");
    expect(markup).not.toContain("\\u");
  });

  it.each([
    ["disconnected", "Not connected"],
    ["connecting", "Connecting to Tokki"],
    ["loading", "Discovering gestures"],
    ["error", "Connection lost or failed"],
  ] as const)("keeps %s red and explains the state in text", (status, label) => {
    const markup = badge(status);
    expect(markup).toContain("is-disconnected");
    expect(markup).not.toContain('class="pet-connection-button is-connected"');
    expect(markup).toContain(label);
  });

  it("never presents browser previews as a live device", () => {
    const markup = badge("connected", false);
    expect(markup).toContain("is-disconnected");
    expect(markup).toContain("Not connected · browser preview");
  });

  it("renders a connected device even without a reported port name", () => {
    const markup = badge("connected");
    expect(markup).toContain("<small>Connected</small>");
    expect(markup).not.toContain("null");
  });
});

describe("pet desk welcome", () => {
  it.each([false, true])("labels the portrait as an illustration (connected: %s)", (connected) => {
    const markup = renderToStaticMarkup(
      <PetWelcome connected={connected} onExplore={() => {}} onCompose={() => {}} onEvents={() => {}} />,
    );
    expect(markup).toContain("illustration, not a live device view");
    expect(markup).toContain("Explore gestures");
    expect(markup).toContain("try them manually");
    expect(markup).toContain(connected ? "Your little companion is connected" : "local previews");
  });
});
