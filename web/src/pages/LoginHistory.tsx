import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import { ApiError, LoginAttempt, loginHistory } from "../api";

export default function LoginHistoryPage() {
  const { t } = useTranslation();
  const [rows, setRows] = useState<LoginAttempt[] | null>(null);
  const [denied, setDenied] = useState(false);

  useEffect(() => {
    loginHistory()
      .then(setRows)
      .catch((err) => setDenied(err instanceof ApiError && err.status === 403));
  }, []);

  if (denied) return <p className="text-slate-600">{t("admin.only")}</p>;
  if (rows === null) return <p className="text-slate-500">…</p>;

  return (
    <div>
      <h1 className="mb-3 text-lg font-bold text-slate-900">{t("admin.title")}</h1>
      <table className="w-full rounded-lg bg-white text-left text-sm shadow-sm">
        <thead>
          <tr className="border-b border-slate-200 text-xs uppercase text-slate-500">
            <th className="px-3 py-2">{t("admin.time")}</th>
            <th className="px-3 py-2">{t("admin.user")}</th>
            <th className="px-3 py-2">{t("admin.success")}</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((row, index) => (
            <tr key={`${row.at}-${index}`} className="border-b border-slate-100">
              <td className="px-3 py-2 text-slate-600">{row.at}</td>
              <td className="px-3 py-2 font-medium text-slate-900">{row.username}</td>
              <td className="px-3 py-2">
                <span className={row.success ? "text-green-600" : "text-red-600"}>
                  {row.success ? t("admin.success") : t("admin.failed")}
                </span>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
