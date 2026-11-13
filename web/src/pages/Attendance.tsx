import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { useParams } from "react-router-dom";

import { ApiError, listStudents, studentAttendance, StudentAttendance } from "../api";

/** One student's attendance. The server decides who may see it; a refusal shows no data. */
export default function AttendancePage() {
  const { id = "" } = useParams();
  const { t } = useTranslation();
  const [data, setData] = useState<StudentAttendance | null>(null);
  const [name, setName] = useState<string | null>(null);
  const [error, setError] = useState<number | null>(null);

  useEffect(() => {
    setData(null);
    setError(null);
    studentAttendance(id)
      .then((result) => {
        setData(result);
        listStudents()
          .then((students) => setName(students.find((s) => String(s.id) === id)?.name ?? null))
          .catch(() => setName(null));
      })
      .catch((err) => setError(err instanceof ApiError ? err.status : 0));
  }, [id]);

  if (error === 401 || error === 403) {
    return (
      <div role="alert" className="rounded bg-red-50 p-4 text-red-700">
        <p className="font-semibold">{t("students.not_allowed")}</p>
        <p className="text-sm">HTTP {error}</p>
      </div>
    );
  }
  if (error === 404) return <p className="text-slate-600">{t("students.not_found")}</p>;
  if (error !== null) return <p className="text-slate-600">{t("students.error")}</p>;
  if (!data) return <p className="text-slate-500">…</p>;

  return (
    <div>
      <h1 className="text-lg font-bold text-slate-900">{t("students.title")}</h1>
      <p className="mb-3 text-slate-700">{name ?? `#${data.student_id}`}</p>
      {data.attendance.length === 0 ? (
        <p className="text-slate-600">{t("students.none")}</p>
      ) : (
        <ul className="divide-y rounded-lg bg-white shadow-sm">
          {data.attendance.map((row) => (
            <li key={row.date} className="flex justify-between px-4 py-2 text-sm">
              <span>{row.date}</span>
              <span>{row.entry_time ?? "—"}</span>
              <span className="font-semibold">{row.status}</span>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
