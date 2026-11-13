import { act, render, screen, waitFor, within } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it } from "vitest";

import AppLayout from "../AppLayout";
import LoginPage from "../pages/Login";
import RoomPage from "../pages/Room";
import { installFetch, jsonRoute, type Route as FetchRoute } from "../test/fetchMock";
import { MockWebSocket } from "../test/wsMock";
import "../i18n";

const fireAndPanic = [
  { room_id: "R-305", kind: "FIRE", since: "2026-11-13T06:00:00+00:00" },
  { room_id: "R-306", kind: "PANIC", since: "2026-11-13T06:00:00+00:00" },
];

const alertsRoute = (body: unknown): FetchRoute => ({
  match: (url) => url.endsWith("/api/alerts/active"),
  reply: () => ({ status: 200, body }),
});

function renderPage(path: string) {
  return render(
    <MemoryRouter initialEntries={[path]}>
      <Routes>
        <Route element={<AppLayout />}>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/rooms/:id" element={<RoomPage />} />
        </Route>
      </Routes>
    </MemoryRouter>,
  );
}

describe("Banner", () => {
  it("shows FIRE and PANIC in red and is present on every page when fire_active", async () => {
    // student: no room data on the socket, banner comes from the /api/alerts/active poll
    installFetch([
      jsonRoute("/api/auth/me", 200, { username: "s2", role: "student" }),
      alertsRoute(fireAndPanic),
      jsonRoute("/api/rooms/R-305/state", 200, { status: "SAFE", online: true }),
      jsonRoute("/api/rooms/R-305/events", 200, []),
    ]);

    const loginPage = renderPage("/login");
    const banner = await screen.findByRole("alert");
    expect(banner).toHaveClass("bg-red-600");
    expect(within(banner).getByText("FIRE in room R-305")).toBeInTheDocument();
    expect(within(banner).getByText("PANIC in room R-306")).toBeInTheDocument();
    loginPage.unmount();

    const roomPage = renderPage("/rooms/R-305");
    const banner2 = await screen.findByRole("alert");
    expect(banner2).toHaveClass("bg-red-600");
    roomPage.unmount();
  });

  it("teacher sees the banner through the live socket within 3 s", async () => {
    let fire = false;
    installFetch([
      jsonRoute("/api/auth/me", 200, { username: "teacher", role: "teacher" }),
      {
        match: (url) => url.endsWith("/api/alerts/active"),
        reply: () => ({ status: 200, body: fire ? fireAndPanic.slice(0, 1) : [] }),
      },
    ]);
    renderPage("/login");
    expect(await screen.findByRole("button", { name: "Log in" })).toBeInTheDocument();

    const socket = await waitFor(() => {
      const s = MockWebSocket.instances.at(-1);
      expect(s).toBeDefined();
      return s as MockWebSocket;
    });
    act(() => {
      socket.open();
    });
    fire = true;
    act(() => {
      socket.message({ type: "event", room_id: "R-305", data: { type: "FIRE", detail: "" } });
    });

    expect(await screen.findByRole("alert")).toHaveTextContent("FIRE in room R-305");
  });
});
