import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Link, useParams } from "react-router-dom";

import { me } from "../api";
import { RecentEvents } from "../components/RecentEvents";
import { ResetFire } from "../components/ResetFire";
import { StatusCard } from "../components/StatusCard";
import { useLive } from "../useLive";

const fmt = (value: number | null | undefined) => (value == null ? "—" : value.toFixed(1));

export default function RoomPage() {
  const { id = "" } = useParams();
  const { t } = useTranslation();
  const { state, events, connected } = useLive(id);
  const [role, setRole] = useState<string | null>(null);

  useEffect(() => {
    me()
      .then((user) => setRole(user.role))
      .catch(() => setRole(null));
  }, []);

  if (!state) return <p className="text-slate-500">…</p>;

  const canReset = role === "teacher" || role === "admin";

  return (
    <div>
      <div className="flex items-center gap-3">
        <h1 className="text-lg font-bold text-slate-900">{id}</h1>
        <span className={connected ? "text-xs text-green-600" : "text-xs text-slate-400"}>
          {connected ? t("room.connected") : t("room.disconnected")}
        </span>
        <Link to={`/rooms/${id}/health`} className="ml-auto text-xs text-slate-500 underline">
          {t("nav.health")}
        </Link>
      </div>

      {state.fire_active && (
        <p className="mt-2 rounded bg-red-100 px-3 py-2 text-sm font-semibold text-red-700">
          {t("room.fire_latch")}
        </p>
      )}

      <div className="mt-4 grid grid-cols-2 gap-3 md:grid-cols-4">
        <StatusCard id="status" label={t("room.status")} value={state.status ? t(`status.${state.status}`) : "—"} />
        <StatusCard id="temp_c" label={t("room.temp_c")} value={fmt(state.temp_c)} />
        <StatusCard id="rh" label={t("room.rh")} value={fmt(state.rh)} />
        <StatusCard id="heat_index_c" label={t("room.heat_index_c")} value={fmt(state.heat_index_c)} />
        <StatusCard id="heat_trend" label={t("room.heat_trend")} value={state.heat_trend ?? "—"} />
        <StatusCard id="door" label={t("room.door")} value={state.door ?? "—"} />
        <StatusCard id="flame" label={t("room.flame")} value={state.flame ? "🔥" : "—"} />
        <StatusCard
          id="online"
          label={t("room.online")}
          value={state.online ? t("room.online_yes") : t("room.online_no")}
        />
      </div>

      {canReset && <ResetFire roomId={id} online={state.online} />}

      <RecentEvents events={events} />
    </div>
  );
}
