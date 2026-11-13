import { vi } from "vitest";

/** Controllable WebSocket double: nothing happens until the test calls open()/close()/message(). */
export class MockWebSocket {
  static instances: MockWebSocket[] = [];
  url: string;
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  sent: string[] = [];

  constructor(url: string) {
    this.url = url;
    MockWebSocket.instances.push(this);
  }

  open(): void {
    this.onopen?.();
  }

  close(): void {
    this.onclose?.();
  }

  message(msg: unknown): void {
    this.onmessage?.({ data: JSON.stringify(msg) });
  }
}

export function installWS(): void {
  MockWebSocket.instances = [];
  vi.stubGlobal("WebSocket", MockWebSocket);
}
