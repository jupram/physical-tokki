import { describe, expect, it, vi } from "vitest";
import { createClient, emptySnapshot, queueSummary, type Activity } from "./native";

describe("native serial bridge", () => {
  it("browser mode refuses every operation without calling native or inventing data", async () => {
    const call = vi.fn();
    const client = createClient(false, call);
    for (const operation of [() => client.ports(), () => client.snapshot(), () => client.connect("COM5"),
      () => client.disconnect(), () => client.refresh(), () => client.run("discovered.action")]) {
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
    await client.refresh();
    await client.disconnect();
    expect(call.mock.calls.slice(1)).toEqual([
      ["serial_connect", { port: "COM12" }], ["serial_run", { actionId: "future.device.gesture" }],
      ["serial_refresh", undefined], ["serial_disconnect", undefined],
    ]);
  });

  it("reports real native failures without retrying gesture sends", async () => {
    const call = vi.fn().mockRejectedValue(new Error("Serial worker unavailable"));
    await expect(createClient(true, call).run("a")).rejects.toThrow("Serial worker unavailable");
    expect(call).toHaveBeenCalledTimes(1);
  });

  it("counts queued/current work from real backend lifecycle states only", () => {
    const activity = ["sending", "queued", "running", "completed", "failed", "timed_out"]
      .map((state) => ({ state }) as Activity);
    expect(queueSummary(activity)).toEqual({ inFlight: 3, sending: 1, queued: 1, running: 1 });
    expect(queueSummary([]).inFlight).toBe(0);
  });

  it("returns backend snapshots unchanged, including failed outcomes", async () => {
    const snapshot = { ...emptySnapshot, revision: 7, status: "error", lastError: "USB unplugged" };
    const call = vi.fn().mockResolvedValue(snapshot);
    expect(await createClient(true, call).snapshot()).toBe(snapshot);
  });
});
