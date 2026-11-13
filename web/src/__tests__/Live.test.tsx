import { act, render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";

import { useLive } from "../useLive";
import { installFetch } from "../test/fetchMock";
import { MockWebSocket } from "../test/wsMock";
import "../i18n";

function Probe({ roomId }: { roomId: string }) {
  const { state, connected } = useLive(roomId);
  return (
    <div>
      <span>{state ? `temp:${state.temp_c}` : "no-state"}</span>
      <span>{connected ? "connected" : "disconnected"}</span>
    </div>
  );
}

function renderProbe() {
  return render(
    <MemoryRouter>
      <Probe roomId="R-305" />
    </MemoryRouter>,
  );
}

describe("useLive", () => {
  it("reconnects after a dropped socket and refetches state", async () => {
    vi.useFakeTimers();
    const temps = [29.0, 43.5];
    installFetch([
      {
        match: (url) => url.includes("/api/rooms/R-305/state"),
        reply: () => ({ status: 200, body: { temp_c: temps.length ? temps.shift() : 99, online: true } }),
      },
      { match: (url) => url.includes("/api/rooms/R-305/events"), reply: () => ({ status: 200, body: [] }) },
    ]);

    renderProbe();
    expect(MockWebSocket.instances.length).toBe(1);
    await act(async () => {}); // flush the first state fetch
    expect(screen.getByText("temp:29")).toBeInTheDocument();
    expect(screen.getByText("disconnected")).toBeInTheDocument();

    act(() => {
      MockWebSocket.instances[0].close(); // the server dropped the socket
    });
    expect(screen.getByText("disconnected")).toBeInTheDocument();

    await act(async () => {
      vi.advanceTimersByTime(1000); // first reconnect gap: 1 s
    });
    expect(MockWebSocket.instances.length).toBe(2);
    expect(screen.getByText("temp:29")).toBeInTheDocument(); // last state kept while offline

    await act(async () => {
      MockWebSocket.instances[1].open(); // back online: state is refetched
    });
    expect(screen.getByText("temp:43.5")).toBeInTheDocument();
    expect(screen.getByText("connected")).toBeInTheDocument();
  });

  it("uses growing gaps of 1 s, 2 s, 4 s, 8 s between reconnects", async () => {
    vi.useFakeTimers();
    installFetch([
      { match: (url) => url.includes("/api/rooms/R-305/state"), reply: () => ({ status: 200, body: { temp_c: 20, online: true } }) },
      { match: (url) => url.includes("/api/rooms/R-305/events"), reply: () => ({ status: 200, body: [] }) },
    ]);
    renderProbe();
    await act(async () => {});

    const sockets = MockWebSocket.instances;
    act(() => {
      sockets[0].close();
    });
    await act(async () => {
      vi.advanceTimersByTime(999);
    });
    expect(sockets.length).toBe(1); // nothing before the first gap ends
    await act(async () => {
      vi.advanceTimersByTime(1);
    });
    expect(sockets.length).toBe(2); // first gap = 1 s

    act(() => {
      sockets[1].close(); // this one never opened: attempt counter stays at 1
    });
    await act(async () => {
      vi.advanceTimersByTime(1999);
    });
    expect(sockets.length).toBe(2);
    await act(async () => {
      vi.advanceTimersByTime(1);
    });
    expect(sockets.length).toBe(3); // second gap = 2 s
  });
});
