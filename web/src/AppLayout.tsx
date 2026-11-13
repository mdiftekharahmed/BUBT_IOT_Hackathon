import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Link, Outlet, useNavigate } from "react-router-dom";

import { logout, me } from "./api";
import { Banner } from "./components/Banner";
import { LangSwitch } from "./components/LangSwitch";

export default function AppLayout() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const [role, setRole] = useState<string | null>(null);

  useEffect(() => {
    me()
      .then((user) => setRole(user.role))
      .catch(() => setRole(null));
  }, []);

  const onLogout = async () => {
    await logout();
    navigate("/login");
  };

  return (
    <div className="min-h-screen bg-slate-100">
      <Banner />
      <header className="flex items-center gap-4 bg-white px-4 py-3 shadow">
        <span className="text-lg font-bold text-slate-900">{t("app.title")}</span>
        <nav className="flex gap-3 text-sm">
          {(role === "admin" || role === "teacher") && <Link to="/">{t("nav.home")}</Link>}
          {role === "admin" && <Link to="/admin/logins">{t("nav.logins")}</Link>}
        </nav>
        <div className="ml-auto flex items-center gap-2">
          <LangSwitch />
          {role && (
            <button
              type="button"
              onClick={() => void onLogout()}
              className="rounded border border-slate-300 px-2 py-1 text-xs"
            >
              {t("nav.logout")}
            </button>
          )}
        </div>
      </header>
      <main className="mx-auto max-w-5xl p-4">
        <Outlet />
      </main>
    </div>
  );
}
