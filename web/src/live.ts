/** Live WebSocket client with reconnect gaps 1, 2, 4, 8 s (then stays at 8). */

export type LiveMessage = {
  type: string;
  room_id: string;
  data: unknown;
  sent_at?: string;
  ping?: boolean;
};

export type LiveHooks = {
  onMessage?: (message: LiveMessage) => void;
  onStatus?: (connected: boolean) => void;
  onOpen?: () => void;
};

const RECONNECT_GAPS_S = [1, 2, 4, 8];

export class LiveClient {
  private ws: WebSocket | null = null;
  private attempt = 0;
  private stopped = false;
  private timer: ReturnType<typeof setTimeout> | null = null;

  constructor(
    private readonly url: string,
    private readonly hooks: LiveHooks,
  ) {}

  connect(): void {
    this.stopped = false;
    this.open();
  }

  private open(): void {
    if (this.stopped) return;
    const WSCtor = (globalThis as { WebSocket?: typeof WebSocket }).WebSocket;
    if (!WSCtor) return; // no socket implementation available (some tests)
    const ws = new WSCtor(this.url);
    this.ws = ws;
    ws.onopen = () => {
      this.attempt = 0;
      this.hooks.onStatus?.(true);
      this.hooks.onOpen?.(); // catch up on anything missed while offline
    };
    ws.onmessage = (event: MessageEvent) => {
      try {
        this.hooks.onMessage?.(JSON.parse(String(event.data)) as LiveMessage);
      } catch {
        /* non-JSON frame: ignore */
      }
    };
    ws.onclose = () => {
      this.hooks.onStatus?.(false);
      if (this.stopped) return;
      const gap = RECONNECT_GAPS_S[Math.min(this.attempt, RECONNECT_GAPS_S.length - 1)];
      this.attempt += 1;
      this.timer = setTimeout(() => this.open(), gap * 1000);
    };
    ws.onerror = () => {
      try {
        ws.close();
      } catch {
        /* already closing */
      }
    };
  }

  close(): void {
    this.stopped = true;
    if (this.timer !== null) clearTimeout(this.timer);
    if (this.ws) {
      this.ws.onclose = null;
      this.ws.onerror = null;
      try {
        this.ws.close();
      } catch {
        /* already closed */
      }
    }
    this.hooks.onStatus?.(false);
  }
}

/** Same-origin socket URL: the browser sends the session cookie with the upgrade. */
export function liveUrl(): string {
  const scheme = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${scheme}//${window.location.host}/api/live`;
}
