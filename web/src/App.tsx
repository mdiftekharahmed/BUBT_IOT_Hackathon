import { Route, Routes } from "react-router-dom";

import AppLayout from "./AppLayout";
import HealthPage from "./pages/Health";
import HomePage from "./pages/Home";
import LoginPage from "./pages/Login";
import LoginHistoryPage from "./pages/LoginHistory";
import RoomPage from "./pages/Room";
import AttendancePage from "./pages/Attendance";

export default function App() {
  return (
    <Routes>
      <Route element={<AppLayout />}>
        <Route path="/login" element={<LoginPage />} />
        <Route path="/" element={<HomePage />} />
        <Route path="/rooms/:id" element={<RoomPage />} />
        <Route path="/rooms/:id/health" element={<HealthPage />} />
        <Route path="/admin/logins" element={<LoginHistoryPage />} />
        <Route path="/students/:id/attendance" element={<AttendancePage />} />
        <Route path="*" element={<HomePage />} />
      </Route>
    </Routes>
  );
}
