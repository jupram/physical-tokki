import { invoke, isTauri } from "@tauri-apps/api/core";

export interface Port { name: string; description: string }
export interface Action { id: string; name: string; device: string; cancellable: boolean }
export interface Hello { protocol: number; firmware: string; board: string; ready: boolean; queueCapacity: number }
export interface Activity {
  requestId: string;
  actionId: string;
  name: string;
  state: "sending" | "queued" | "running" | "completed" | "failed" | "timed_out";
  message: string;
  updatedAt: number;
}
export interface Snapshot {
  revision: number;
  status: "disconnected" | "connecting" | "loading" | "connected" | "error";
  port: string | null;
  hello: Hello | null;
  actions: Action[];
  activity: Activity[];
  lastError: string | null;
}
export interface NotificationSnapshot {
  supported: boolean;
  permission: "unknown" | "unspecified" | "allowed" | "denied" | "unsupported";
  enabled: boolean;
  pending: number;
  queued: QueuedNotification[];
  lastEvent: string | null;
  lastError: string | null;
}
export interface QueuedNotification {
  source: string;
  text: string;
  oled: string;
  sound: string;
  light: string;
}
export const emptySnapshot: Snapshot = {
  revision: 0, status: "disconnected", port: null, hello: null, actions: [], activity: [], lastError: null,
};

export function isInFlight(state: Activity["state"]) {
  return state === "sending" || state === "queued" || state === "running";
}

export function queueSummary(activity: Activity[]) {
  return {
    inFlight: activity.filter((item) => isInFlight(item.state)).length,
    sending: activity.filter((item) => item.state === "sending").length,
    queued: activity.filter((item) => item.state === "queued").length,
    running: activity.filter((item) => item.state === "running").length,
  };
}

export function actionCapacity(snapshot: Snapshot) {
  const activeDevices = new Set(snapshot.actions.map((action) => action.device)).size;
  return (snapshot.hello?.queueCapacity ?? 0) + activeDevices;
}

export function createClient(native: boolean, call: typeof invoke = invoke) {
  function nativeCall<T>(command: string, args?: Record<string, unknown>): Promise<T> {
    if (!native) return Promise.reject(new Error("Native desktop app required. Run npm run tauri dev; browser serial is not supported."));
    return call<T>(command, args);
  }
  return {
    native,
    ports: () => nativeCall<Port[]>("serial_ports"),
    snapshot: () => nativeCall<Snapshot>("serial_snapshot"),
    connect: (port: string) => nativeCall<void>("serial_connect", { port }),
    disconnect: () => nativeCall<void>("serial_disconnect"),
    refresh: () => nativeCall<void>("serial_refresh"),
    run: (actionId: string, text?: string) => nativeCall<void>("serial_run", {
      actionId,
      ...(text === undefined ? {} : { text }),
    }),
    marquee: (text: string) => nativeCall<void>("serial_marquee", { text }),
    notificationSnapshot: () => nativeCall<NotificationSnapshot>("notification_snapshot"),
    requestNotificationAccess: () => nativeCall<NotificationSnapshot>("notification_request_access"),
    setNotificationRelay: (enabled: boolean) => nativeCall<void>("notification_set_enabled", { enabled }),
  };
}

export const client = createClient(isTauri());
