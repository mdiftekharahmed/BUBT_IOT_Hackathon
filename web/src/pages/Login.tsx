import { FormEvent, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { useNavigate } from "react-router-dom";

import { ApiError, login } from "../api";

export default function LoginPage() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [lock, setLock] = useState(0);

  // countdown after a 423: the server says how long the lock lasts
  useEffect(() => {
    if (lock <= 0) return undefined;
    const tick = setInterval(() => setLock((s) => (s > 0 ? s - 1 : 0)), 1000);
    return () => clearInterval(tick);
  }, [lock]);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    setError(null);
    try {
      await login(username, password);
      navigate("/");
    } catch (err) {
      if (err instanceof ApiError && err.status === 423) {
        const detail = err.detail as { locked_for_s?: number } | null;
        setLock(detail?.locked_for_s ?? 30);
      } else {
        setError(t("login.invalid"));
      }
    }
  };

  return (
    <form onSubmit={(e) => void submit(e)} className="mx-auto mt-8 max-w-sm rounded-lg bg-white p-6 shadow">
      <h1 className="mb-4 text-xl font-bold text-slate-900">{t("app.title")}</h1>
      <label htmlFor="username" className="block text-sm text-slate-600">
        {t("login.username")}
      </label>
      <input
        id="username"
        value={username}
        onChange={(e) => setUsername(e.target.value)}
        autoComplete="username"
        className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5"
      />
      <label htmlFor="password" className="mt-3 block text-sm text-slate-600">
        {t("login.password")}
      </label>
      <input
        id="password"
        type="password"
        value={password}
        onChange={(e) => setPassword(e.target.value)}
        autoComplete="current-password"
        className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5"
      />
      {error && <p className="mt-3 text-sm text-red-600">{error}</p>}
      {lock > 0 && <p className="mt-3 text-sm text-red-600">{t("login.locked", { seconds: lock })}</p>}
      <button
        type="submit"
        disabled={lock > 0}
        className="mt-4 w-full rounded bg-slate-900 px-3 py-2 text-sm font-semibold text-white disabled:cursor-not-allowed disabled:opacity-40"
      >
        {t("login.submit")}
      </button>
    </form>
  );
}
