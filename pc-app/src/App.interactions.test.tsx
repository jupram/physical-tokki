import { Children, isValidElement, type ReactElement, type ReactNode } from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import App from "./App";
import { MediumStudio } from "./gestures/MediumStudio";
import { ScrollingTextInput } from "./gestures/ScrollingTextInput";
import { CATALOG_BY_ID } from "./gestures/catalog";
import { playBundle } from "./gestures/player";
import { GesturePreview, IDLE_PREVIEW } from "./gestures/GesturePreview";
import { NOTIFICATION_PRESETS, notificationLanes } from "./notifications";
import { client, emptySnapshot, type NotificationSnapshot, type Snapshot } from "./native";

const hooks = vi.hoisted(() => ({
  slots: [] as unknown[],
  cursor: 0,
  snapshot: null as unknown,
  notifications: null as unknown,
}));

// Exercise component callbacks without introducing a DOM/test-renderer dependency.
vi.mock("react", async (importOriginal) => {
  const react = await importOriginal<typeof import("react")>();
  function slot(initial: unknown) {
    const index = hooks.cursor++;
    if (!(index in hooks.slots)) hooks.slots[index] = initial;
    return index;
  }
  return {
    ...react,
    useEffect: vi.fn(),
    useRef: (initial: unknown) => hooks.slots[slot({ current: initial })],
    useState: (initial: unknown) => {
      let value = typeof initial === "function" ? initial() : initial;
      if (value && typeof value === "object" && "revision" in value) value = hooks.snapshot;
      if (value && typeof value === "object" && "permission" in value) value = hooks.notifications;
      const index = slot(value);
      return [hooks.slots[index], (next: unknown) => {
        hooks.slots[index] = typeof next === "function" ? next(hooks.slots[index]) : next;
      }];
    },
  };
});

vi.mock("./gestures/player", async (importOriginal) => ({
  ...await importOriginal<typeof import("./gestures/player")>(),
  playBundle: vi.fn(() => vi.fn()),
}));

vi.mock("./native", async (importOriginal) => ({
  ...await importOriginal<typeof import("./native")>(),
  client: { native: true, run: vi.fn().mockResolvedValue(undefined), marquee: vi.fn() },
}));

type Element = ReactElement<Record<string, any>>;

function elements(node: ReactNode): Element[] {
  return Children.toArray(node).flatMap((child) => {
    if (!isValidElement(child)) return [];
    const element = child as Element;
    return [element, ...elements(element.props.children)];
  });
}

function find(tree: ReactNode, predicate: (element: Element) => boolean): Element {
  const result = elements(tree).find(predicate);
  if (!result) throw new Error("Expected UI element was not found");
  return result;
}

function text(node: ReactNode): string {
  return Children.toArray(node).map((child) => isValidElement(child)
    ? text((child as Element).props.children)
    : String(child)).join("");
}

function render(component: () => ReactNode = App) {
  hooks.cursor = 0;
  return component();
}

function openView(name: string) {
  const tree = render();
  find(tree, (node) => node.type === "button" && node.props.className?.includes("nav-item") && text(node).includes(name)).props.onClick();
  return render();
}

describe("notification and manual scrolling controls", () => {
  beforeEach(() => {
    hooks.slots = [];
    hooks.cursor = 0;
    hooks.snapshot = {
      ...emptySnapshot, status: "connected",
      hello: { protocol: 1, firmware: "test", board: "test", ready: true, queueCapacity: 4 },
      actions: [
        { id: "oled.scrolling_text", name: "Scrolling text", device: "oled", cancellable: false },
        { id: "oled.happy", name: "Happy eyes", device: "oled", cancellable: false },
      ],
    } satisfies Snapshot;
    hooks.notifications = {
      supported: true, permission: "allowed", enabled: true, pending: 1,
      queued: [{ ...NOTIFICATION_PRESETS[0], text: "Only the queued title" }],
      lastEvent: null, lastError: null,
    } satisfies NotificationSnapshot;
    vi.clearAllMocks();
    vi.useFakeTimers();
    vi.stubGlobal("window", { setTimeout: globalThis.setTimeout, clearTimeout: globalThis.clearTimeout });
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it.each(["Preview Microsoft Teams", "Preview queued notification 1"])("keeps %s PC-only even while connected and preserves the FIFO", (label) => {
    const original = structuredClone(hooks.notifications);
    const tree = openView("Events");
    find(tree, (node) => node.props["aria-label"] === label).props.onClick();
    const item = label.includes("queued")
      ? (hooks.notifications as NotificationSnapshot).queued[0]
      : NOTIFICATION_PRESETS[0];
    expect(playBundle).toHaveBeenCalledExactlyOnceWith(notificationLanes(item), expect.any(Function), expect.any(Function));
    expect(client.run).not.toHaveBeenCalled();
    expect(client.marquee).not.toHaveBeenCalled();
    expect(hooks.notifications).toEqual(original);
    expect(text(render())).toContain("Title: Only the queued title");
    expect(text(render())).toContain("Curious eyes → Scrolling text");
  });

  it.each([
    ["Preview Microsoft Teams", "Stop preview"],
    ["Preview queued notification 1", "Preview queued notification 1"],
  ])("resets the status and lanes when stopping %s", (startLabel, stopLabel) => {
    const original = structuredClone(hooks.notifications);
    let tree = openView("Events");
    find(tree, (node) => node.props["aria-label"] === startLabel).props.onClick();
    const player = vi.mocked(playBundle);
    player.mock.calls[0][1]((previous) => ({
      ...previous,
      oled: { label: "Curious eyes", active: true, sim: CATALOG_BY_ID["oled.curious"].sim.oled },
    }));
    tree = render();
    expect(text(tree)).toContain("Previewing Microsoft Teams");
    find(tree, (node) => node.props.title === stopLabel || node.props["aria-label"] === stopLabel).props.onClick();
    tree = render();
    expect(player.mock.results[0].value).toHaveBeenCalledOnce();
    expect(text(tree)).toContain("Local preview ready · PC only; no hardware sends or FIFO changes");
    expect(text(tree)).not.toContain("Previewing Microsoft Teams");
    expect(find(tree, (node) => node.type === GesturePreview).props.state).toEqual(IDLE_PREVIEW);
    expect(elements(tree).some((node) => node.props.title === "Stop preview")).toBe(false);
    expect(hooks.notifications).toEqual(original);
    expect(client.run).not.toHaveBeenCalled();
    expect(client.marquee).not.toHaveBeenCalled();
  });

  it("sends custom text unchanged from the discovered action and blocks invalid text", () => {
    let tree = openView("Gestures");
    const custom = 'Custom "title" \\ text';
    find(tree, (node) => node.type === ScrollingTextInput).props.onChange(custom);
    tree = render();
    find(tree, (node) => node.props["aria-label"] === "Send Scrolling text").props.onClick();
    expect(client.run).toHaveBeenCalledExactlyOnceWith("oled.scrolling_text", custom);
    vi.mocked(client.run).mockClear();
    find(tree, (node) => node.type === ScrollingTextInput).props.onChange("x".repeat(51));
    tree = render();
    const send = find(tree, (node) => node.props["aria-label"] === "Send Scrolling text");
    expect(send.props.disabled).toBe(true);
    send.props.onClick();
    expect(client.run).not.toHaveBeenCalled();
  });

  it("previews custom OLED text and forwards it to the connected-preview callback", () => {
    const onPreviewGesture = vi.fn();
    const studio = () => MediumStudio({ medium: "oled", onPreviewGesture });
    let tree = render(studio);
    find(tree, (node) => node.type === "select").props.onChange({ target: { value: "oled.scrolling_text" } });
    tree = render(studio);
    find(tree, (node) => node.type === ScrollingTextInput).props.onChange("A".repeat(50));
    tree = render(studio);
    find(tree, (node) => node.type === "button").props.onClick();
    expect(onPreviewGesture).toHaveBeenCalledExactlyOnceWith(
      { ...CATALOG_BY_ID["oled.scrolling_text"], ms: 16335, sim: { oled: { kind: "marquee", text: "A".repeat(50) } } },
      "A".repeat(50),
    );
    tree = render(studio);
    find(tree, (node) => node.type === ScrollingTextInput).props.onChange("Not ASCII 🙂");
    tree = render(studio);
    const preview = find(tree, (node) => node.type === "button");
    expect(preview.props.disabled).toBe(true);
    onPreviewGesture.mockClear();
    preview.props.onClick();
    expect(onPreviewGesture).not.toHaveBeenCalled();
  });

  it("forwards optional text through connected previews but sends nothing when disconnected", () => {
    let tree = openView("Gestures");
    find(tree, (node) => node.type === MediumStudio && node.props.medium === "oled")
      .props.onPreviewGesture(CATALOG_BY_ID["oled.scrolling_text"], "Preview title");
    expect(client.run).toHaveBeenCalledExactlyOnceWith("oled.scrolling_text", "Preview title");
    vi.mocked(client.run).mockClear();
    hooks.slots = [];
    hooks.snapshot = emptySnapshot;
    tree = openView("Gestures");
    find(tree, (node) => node.type === MediumStudio && node.props.medium === "oled")
      .props.onPreviewGesture(CATALOG_BY_ID["oled.scrolling_text"], "Preview title");
    expect(client.run).not.toHaveBeenCalled();
  });
});
