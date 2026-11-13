import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Link } from "react-router-dom";

import { ApiError, listRooms, listStudents, RoomInfo, StudentInfo } from "../api";

export default function HomePage() {
  const { t } = useTranslation();
  const [rooms, setRooms] = useState<RoomInfo[] | null>(null);
  const [denied, setDenied] = useState(false);
  const [students, setStudents] = useState<StudentInfo[] | null>(null);

  useEffect(() => {
    listRooms()
      .then(setRooms)
      .catch((err) => {
        if (err instanceof ApiError && err.status === 403) {
          // Student: own attendance. Guardian: linked children.
          setDenied(true);
          listStudents()
            .then(setStudents)
            .catch(() => setStudents([]));
        }
      });
  }, []);

  if (denied) {
    if (students === null) return <p className="text-slate-500">…</p>;
    if (students.length === 0) return <p className="text-slate-600">{t("home.none")}</p>;
    return (
      <div>
        <h1 className="mb-3 text-lg font-bold text-slate-900">{t("students.title")}</h1>
        <ul className="grid gap-3 sm:grid-cols-2">
          {students.map((student) => (
            <li key={student.id}>
              <Link
                to={`/students/${student.id}/attendance`}
                className="block rounded-lg bg-white p-4 shadow-sm hover:shadow"
              >
                {student.name}
              </Link>
            </li>
          ))}
        </ul>
      </div>
    );
  }
  if (rooms === null) return <p className="text-slate-500">…</p>;
  if (rooms.length === 0) return <p className="text-slate-600">{t("home.none")}</p>;

  return (
    <div>
      <h1 className="mb-3 text-lg font-bold text-slate-900">{t("home.title")}</h1>
      <ul className="grid gap-3 sm:grid-cols-2">
        {rooms.map((room) => (
          <li key={room.id}>
            <Link
              to={`/rooms/${room.id}`}
              className="block rounded-lg bg-white p-4 shadow-sm hover:shadow"
            >
              <span className="text-base font-semibold text-slate-900">{room.name}</span>
              <span className="ml-2 text-xs text-slate-400">{room.id}</span>
            </Link>
          </li>
        ))}
      </ul>
    </div>
  );
}
