import { describe, expect, it, vi } from "vitest";
import { actionCapacity, createClient, emptySnapshot, queueSummary, type Activity } from "./native";

describe("native serial bridge", () => {
  it("browser mode refuses every operation without calling native or inventing data", async () => {
    const call = vi.fn();
    const client = createClient(false, call);
    for (const operation of [() => client.ports(), () => client.snapshot(), () => client.connect("COM5"),
      () => client.disconnect(), () => client.refresh(), () => client.run("discovered.action"),
      () => client.run("oled.scrolling_text", "Custom title"),
      () => client.marquee("Teams: Build 42!"), () => client.notificationSnapshot(),
      () => client.requestNotificationAccess(), () => client.setNotificationRelay(true)]) {
      await expect(operation()).rejects.toThrow("Native desktop app required");
    }
    expect(call).not.toHaveBeenCalled();
    expect(emptySnapshot.actions).toEqual([]);
  });

  it("enumerating ports never opens one; explicit commands preserve dynamic IDs", async () => {
    const call = vi.fn().mockResolvedValue(undefined);
    const client = createClient(true, call);
    await client.ports();
    expect(call.mock.calls).toEqual([["serial_ports", undefined]]);
    await client.connect("COM12");
    await client.run("future.device.gesture");
    await client.marquee("Teams: Build 42!");
    await client.notificationSnapshot();
    await client.requestNotificationAccess();
    await client.setNotificationRelay(true);
    await client.refresh();
    await client.disconnect();
    expect(call.mock.calls.slice(1)).toEqual([
      ["serial_connect", { port: "COM12" }], ["serial_run", { actionId: "future.device.gesture" }],
      ["serial_marquee", { text: "Teams: Build 42!" }],
      ["notification_snapshot", undefined], ["notification_request_access", undefined],
      ["notification_set_enabled", { enabled: true }],
      ["serial_refresh", undefined], ["serial_disconnect", undefined],
    ]);
  });

  it("reports real native failures without retrying gesture sends", async () => {
    const call = vi.fn().mockRejectedValue(new Error("Serial worker unavailable"));
    await expect(createClient(true, call).run("a")).rejects.toThrow("Serial worker unavailable");
    expect(call).toHaveBeenCalledTimes(1);
  });

  it("forwards custom scrolling text verbatim and omits absent text", async () => {
    const call = vi.fn().mockResolvedValue(undefined);
    const client = createClient(true, call);
    await client.run("oled.scrolling_text", 'Build "42" \\ ready!');
    await client.run("oled.scrolling_text");
    await client.run("oled.scrolling_text", undefined);
    expect(call.mock.calls).toEqual([
      ["serial_run", { actionId: "oled.scrolling_text", text: 'Build "42" \\ ready!' }],
      ["serial_run", { actionId: "oled.scrolling_text" }],
      ["serial_run", { actionId: "oled.scrolling_text" }],
    ]);
  });

  it("returns notification FIFO snapshots unchanged with qualified gesture IDs", async () => {
    const snapshot = {
      supported: true, permission: "allowed", enabled: true, pending: 1,
      queued: [{ source: "Microsoft Teams", text: "New message", oled: "oled.curious", sound: "speaker.trill", light: "neopixel.rainbow" }],
      lastEvent: null, lastError: null,
    };
    const call = vi.fn().mockResolvedValue(snapshot);
    expect(await createClient(true, call).notificationSnapshot()).toBe(snapshot);
    expect(call).toHaveBeenCalledExactlyOnceWith("notification_snapshot", undefined);
  });

  it("counts queued/current work from real backend lifecycle states only", () => {
    const activity = ["sending", "queued", "running", "completed", "failed", "timed_out"]
      .map((state) => ({ state }) as Activity);
    expect(queueSummary(activity)).toEqual({ inFlight: 3, sending: 1, queued: 1, running: 1 });
    expect(queueSummary([]).inFlight).toBe(0);
  });

  it("allows one running action per discovered device plus the firmware queue", () => {
    const snapshot = {
      ...emptySnapshot,
      hello: { protocol: 1, firmware: "0.2.0", board: "test", ready: true, queueCapacity: 4 },
      actions: [
        { id: "oled.a", name: "A", device: "oled", cancellable: false },
        { id: "oled.b", name: "B", device: "oled", cancellable: false },
        { id: "speaker.a", name: "C", device: "speaker", cancellable: false },
        { id: "neopixel.a", name: "D", device: "neopixel", cancellable: false },
      ],
    };
    expect(actionCapacity(snapshot)).toBe(7);
    expect(actionCapacity(emptySnapshot)).toBe(0);
  });

  it("returns backend snapshots unchanged, including failed outcomes", async () => {
    const snapshot = { ...emptySnapshot, revision: 7, status: "error", lastError: "USB unplugged" };
    const call = vi.fn().mockResolvedValue(snapshot);
    expect(await createClient(true, call).snapshot()).toBe(snapshot);
  });
});
