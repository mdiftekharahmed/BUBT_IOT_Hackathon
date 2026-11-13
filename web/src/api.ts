/** Fetch wrapper: same-origin cookies, JSON, and 401/403/423 surfaced as ApiError. */
export class ApiError extends Error {
  status: number;
  detail: unknown;

  constructor(status: number, detail: unknown) {
    super(`api error ${status}`);
    this.status = status;
    this.detail = detail;
  }
}

async function request(path: string, init?: RequestInit): Promise<unknown> {
  const response = await fetch(path, {
    credentials: "same-origin",
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  if (!response.ok) {
    let detail: unknown = null;
    try {
      detail = await response.json();
    } catch {
      detail = null;
    }
    throw new ApiError(response.status, detail);
  }
  try {
    return await response.json();
  } catch {
    return null;
  }
}

export const getJson = (path: string): Promise<unknown> => request(path);
export const postJson = (path: string, body?: unknown): Promise<unknown> =>
  request(path, { method: "POST", body: body === undefined ? undefined : JSON.stringify(body) });

export type Role = "admin" | "teacher" | "student" | "guardian";
export type Me = { username: string; role: Role };
export type RoomInfo = { id: string; name: string };
export type RoomState = {
  status: string | null;
  temp_c: number | null;
  rh: number | null;
  heat_index_c: number | null;
  heat_trend: string | null;
  flame: boolean | null;
  door: string | null;
  online: boolean;
  last_seen_at: string | null;
  quality: string | null;
  fire_active: boolean;
  panic_active: boolean;
};
export type RoomEvent = {
  device_id: string;
  seq: number;
  ts: string;
  type: string;
  detail: string;
  received_at: string | null;
};
export type RoomHealth = {
  last_seen_at: string | null;
  uptime_s: number | null;
  rssi: number | null;
  heap_kb: number | null;
  chip_temp_c: number | null;
  fw: string | null;
  reset_reason: string | null;
};
export type ActiveAlert = { room_id: string; kind: "FIRE" | "PANIC"; since: string };
export type LoginAttempt = { at: string; username: string; success: boolean };

export const me = (): Promise<Me> => getJson("/api/auth/me") as Promise<Me>;
export const listRooms = (): Promise<RoomInfo[]> => getJson("/api/rooms") as Promise<RoomInfo[]>;
export const roomState = (id: string): Promise<RoomState> =>
  getJson(`/api/rooms/${id}/state`) as Promise<RoomState>;
export const roomEvents = (id: string): Promise<RoomEvent[]> =>
  getJson(`/api/rooms/${id}/events`) as Promise<RoomEvent[]>;
export const roomHealth = (id: string): Promise<RoomHealth> =>
  getJson(`/api/rooms/${id}/health`) as Promise<RoomHealth>;
export const activeAlerts = (): Promise<ActiveAlert[]> =>
  getJson("/api/alerts/active") as Promise<ActiveAlert[]>;
export const loginHistory = (): Promise<LoginAttempt[]> =>
  getJson("/api/auth/history") as Promise<LoginAttempt[]>;
export const login = (username: string, password: string): Promise<Me> =>
  postJson("/api/auth/login", { username, password }) as Promise<Me>;
export const logout = (): Promise<unknown> => postJson("/api/auth/logout");
export const resetFire = (roomId: string): Promise<unknown> =>
  postJson(`/api/rooms/${roomId}/reset-fire`);
