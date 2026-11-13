import { render, screen, within } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it } from "vitest";

import i18n from "../i18n";
import LoginPage from "../pages/Login";
import RoomPage from "../pages/Room";
import { installFetch, jsonRoute } from "../test/fetchMock";
import en from "../locales/en.json";
import bn from "../locales/bn.json";

function flatKeys(value: unknown, prefix = ""): string[] {
  if (value === null || typeof value !== "object") return [prefix];
  return Object.entries(value as Record<string, unknown>).flatMap(([key, val]) =>
    flatKeys(val, prefix ? `${prefix}.${key}` : key),
  );
}

describe("locales", () => {
  it("en.json and bn.json have the same key set", () => {
    expect(new Set(flatKeys(bn))).toEqual(new Set(flatKeys(en)));
  });
});

function renderRoom() {
  return render(
    <MemoryRouter initialEntries={["/rooms/R-305"]}>
      <Routes>
        <Route path="/rooms/:id" element={<RoomPage />} />
      </Routes>
    </MemoryRouter>,
  );
}

function renderLogin() {
  return render(
    <MemoryRouter initialEntries={["/login"]}>
      <LoginPage />
    </MemoryRouter>,
  );
}

describe("language switch", () => {
  it("switches every visible label to Bangla and back (snapshots of two pages)", async () => {
    installFetch([
      jsonRoute("/api/auth/me", 200, { username: "teacher", role: "teacher" }),
      jsonRoute("/api/rooms/R-305/state", 200, {
        status: "DANGER",
        temp_c: 41.0,
        rh: 70.0,
        heat_index_c: 36.1,
        heat_trend: "RISING",
        door: "UNLOCKED",
        online: true,
      }),
      jsonRoute("/api/rooms/R-305/events", 200, []),
    ]);

    await i18n.changeLanguage("en");
    const enLogin = renderLogin();
    expect(screen.getByRole("button", { name: "Log in" })).toBeInTheDocument();
    const enLoginHtml = enLogin.container.innerHTML;
    expect(enLoginHtml).toMatchSnapshot();
    enLogin.unmount();

    const enRoom = renderRoom();
    expect(within(await screen.findByTestId("card-status")).getByText("DANGER")).toBeInTheDocument();
    const enRoomHtml = enRoom.container.innerHTML;
    expect(enRoomHtml).toMatchSnapshot();
    enRoom.unmount();

    await i18n.changeLanguage("bn");
    const bnLogin = renderLogin();
    expect(screen.getByRole("button", { name: "লগইন" })).toBeInTheDocument();
    const bnLoginHtml = bnLogin.container.innerHTML;
    expect(bnLoginHtml).toMatchSnapshot();
    bnLogin.unmount();

    const bnRoom = renderRoom();
    expect(within(await screen.findByTestId("card-status")).getByText("বিপদ")).toBeInTheDocument();
    const bnRoomHtml = bnRoom.container.innerHTML;
    expect(bnRoomHtml).toMatchSnapshot();
    expect(bnRoomHtml).not.toBe(enRoomHtml); // the labels really changed
    bnRoom.unmount();

    await i18n.changeLanguage("en");
    const back = renderLogin();
    expect(back.container.innerHTML).toBe(enLoginHtml); // byte-identical to the first English page
  });
});
