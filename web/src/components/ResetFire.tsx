import { useState } from "react";
import { useTranslation } from "react-i18next";

import { ApiError, resetFire } from "../api";

/** Teacher/Admin only (rendering gated by the caller; the server enforces too). */
export function ResetFire({ roomId, online }: { roomId: string; online: boolean }) {
  const { t } = useTranslation();
  const [message, setMessage] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const click = async () => {
    setBusy(true);
    setMessage(null);
    try {
      await resetFire(roomId);
      setMessage(t("room.reset_ok"));
    } catch (error) {
      setMessage(
        error instanceof ApiError && error.status === 409
          ? t("room.reset_offline")
          : t("room.reset_offline"),
      );
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="mt-4 flex items-center gap-3">
      <button
        type="button"
        onClick={() => void click()}
        disabled={!online || busy}
        className="rounded bg-orange-600 px-3 py-1.5 text-sm font-semibold text-white disabled:cursor-not-allowed disabled:opacity-40"
      >
        {t("room.reset_fire")}
      </button>
      {message && <span className="text-sm text-slate-600">{message}</span>}
    </div>
  );
}
