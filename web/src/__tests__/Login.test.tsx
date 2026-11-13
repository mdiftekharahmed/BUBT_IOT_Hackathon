import { act, fireEvent, render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";

import LoginPage from "../pages/Login";
import HomePage from "../pages/Home";
import { installFetch, jsonRoute } from "../test/fetchMock";
import "../i18n";

function renderLogin() {
  return render(
    <MemoryRouter initialEntries={["/login"]}>
      <Routes>
        <Route path="/login" element={<LoginPage />} />
        <Route path="/" element={<HomePage />} />
      </Routes>
    </MemoryRouter>,
  );
}

function fillAndSubmit() {
  fireEvent.change(screen.getByLabelText("Username"), { target: { value: "teacher" } });
  fireEvent.change(screen.getByLabelText("Password"), { target: { value: "pw-teacher" } });
  fireEvent.click(screen.getByRole("button", { name: "Log in" }));
}

describe("Login page", () => {
  it("shows the locked message with seconds remaining", async () => {
    vi.useFakeTimers();
    installFetch([
      jsonRoute("/api/auth/login", 423, { detail: { locked_for_s: 30 } }),
      jsonRoute("/api/auth/me", 401, { detail: "not logged in" }),
    ]);
    renderLogin();
    fillAndSubmit();
    await act(async () => {});
    expect(screen.getByText("Too many attempts — locked. Try again in 30 s")).toBeInTheDocument();
    await act(async () => {
      vi.advanceTimersByTime(1000);
    });
    expect(screen.getByText("Too many attempts — locked. Try again in 29 s")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Log in" })).toBeDisabled();

    await act(async () => {
      vi.advanceTimersByTime(29_000); // the lock expires
    });
    expect(screen.getByRole("button", { name: "Log in" })).toBeEnabled();
  });

  it("shows an error for wrong credentials", async () => {
    installFetch([jsonRoute("/api/auth/login", 401, { detail: "invalid credentials" })]);
    renderLogin();
    fillAndSubmit();
    expect(await screen.findByText("Wrong username or password")).toBeInTheDocument();
  });

  it("logs in and opens the rooms home page", async () => {
    installFetch([
      jsonRoute("/api/auth/login", 200, { username: "teacher", role: "teacher" }),
      jsonRoute("/api/rooms", 200, [{ id: "R-305", name: "Room 305" }]),
    ]);
    renderLogin();
    fillAndSubmit();
    expect(await screen.findByText("Room 305")).toBeInTheDocument();
  });
});
