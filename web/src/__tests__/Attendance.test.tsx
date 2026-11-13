import { render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it } from "vitest";

import AttendancePage from "../pages/Attendance";
import HomePage from "../pages/Home";
import { installFetch, jsonRoute } from "../test/fetchMock";
import "../i18n";

function renderAt(path: string) {
  return render(
    <MemoryRouter initialEntries={[path]}>
      <Routes>
        <Route path="/" element={<HomePage />} />
        <Route path="/students/:id/attendance" element={<AttendancePage />} />
      </Routes>
    </MemoryRouter>,
  );
}

describe("attendance page", () => {
  it("shows Not allowed and no data when the server refuses (S2 opens S1)", async () => {
    installFetch([jsonRoute("/api/students/1/attendance", 403, { detail: "not allowed" })]);
    renderAt("/students/1/attendance");
    expect(await screen.findByText(/Not allowed/)).toBeInTheDocument();
    expect(screen.getByText(/403/)).toBeInTheDocument();
    expect(screen.queryByRole("list")).not.toBeInTheDocument();
  });

  it("shows Not allowed when logged out (401)", async () => {
    installFetch([jsonRoute("/api/students/1/attendance", 401, { detail: "not logged in" })]);
    renderAt("/students/1/attendance");
    expect(await screen.findByText(/Not allowed/)).toBeInTheDocument();
    expect(screen.getByText(/401/)).toBeInTheDocument();
  });

  it("shows the student's own attendance (empty until Level 7)", async () => {
    installFetch([
      jsonRoute("/api/students/2/attendance", 200, { student_id: 2, attendance: [] }),
      jsonRoute("/api/students", 200, [{ id: 2, name: "Student 2" }]),
    ]);
    renderAt("/students/2/attendance");
    expect(await screen.findByText("Student 2")).toBeInTheDocument();
    expect(screen.getByText(/No attendance records yet/)).toBeInTheDocument();
  });
});

describe("home page for students and guardians", () => {
  it("links a student to their own attendance", async () => {
    installFetch([
      jsonRoute("/api/rooms", 403, { detail: "not allowed" }),
      jsonRoute("/api/students", 200, [{ id: 2, name: "Student 2" }]),
    ]);
    renderAt("/");
    const link = await screen.findByRole("link", { name: /Student 2/ });
    expect(link).toHaveAttribute("href", "/students/2/attendance");
  });
});
