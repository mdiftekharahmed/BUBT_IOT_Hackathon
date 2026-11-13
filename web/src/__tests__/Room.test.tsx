import { render, screen, within } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it } from "vitest";

import RoomPage from "../pages/Room";
import { installFetch, jsonRoute } from "../test/fetchMock";
import "../i18n";

const state = {
  status: "DANGER",
  temp_c: 41.0,
  rh: 70.0,
  heat_index_c: 36.1,
  heat_trend: "RISING",
  flame: false,
  door: "UNLOCKED",
  online: true,
  last_seen_at: "2026-11-13T06:00:05+00:00",
  quality: "good",
  fire_active: false,
  panic_active: false,
};

const events = [
  { device_id: "T17-D1", seq: 2, ts: "2026-11-13T06:00:02+00:00", type: "STATUS", detail: "SAFE->DANGER", received_at: null },
  { device_id: "T17-D1", seq: 1, ts: "2026-11-13T06:00:01+00:00", type: "CARD_TAP", detail: "uid=A1B2", received_at: null },
];

function mockRoomApi(role: string, online = true) {
  return installFetch([
    jsonRoute("/api/auth/me", 200, { username: "judge", role }),
    jsonRoute("/api/rooms/R-305/state", 200, { ...state, online }),
    jsonRoute("/api/rooms/R-305/events", 200, events),
  ]);
}

function renderRoom() {
  return render(
    <MemoryRouter initialEntries={["/rooms/R-305"]}>
      <Routes>
        <Route path="/rooms/:id" element={<RoomPage />} />
      </Routes>
    </MemoryRouter>,
  );
}

describe("Room page", () => {
  it("renders status, temperature, humidity, heat index, trend, door and online from state", async () => {
    mockRoomApi("teacher");
    renderRoom();
    const status = await screen.findByTestId("card-status");
    expect(within(status).getByText("DANGER")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-temp_c")).getByText("41.0")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-rh")).getByText("70.0")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-heat_index_c")).getByText("36.1")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-heat_trend")).getByText("RISING")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-door")).getByText("UNLOCKED")).toBeInTheDocument();
    expect(within(screen.getByTestId("card-online")).getByText("online")).toBeInTheDocument();
    expect(within(screen.getByTestId("recent-events")).getByText("CARD_TAP")).toBeInTheDocument();
  });

  it("shows the reset fire button for teacher and admin, hidden for student", async () => {
    mockRoomApi("teacher");
    const first = renderRoom();
    expect(await screen.findByRole("button", { name: "Reset fire" })).toBeEnabled();
    first.unmount();

    mockRoomApi("admin");
    const second = renderRoom();
    expect(await screen.findByRole("button", { name: "Reset fire" })).toBeInTheDocument();
    second.unmount();

    mockRoomApi("student");
    const third = renderRoom();
    await screen.findByTestId("card-status");
    expect(screen.queryByRole("button", { name: "Reset fire" })).not.toBeInTheDocument();
    third.unmount();
  });

  it("disables the reset fire button while the device is offline", async () => {
    mockRoomApi("teacher", false);
    renderRoom();
    expect(await screen.findByRole("button", { name: "Reset fire" })).toBeDisabled();
  });
});
