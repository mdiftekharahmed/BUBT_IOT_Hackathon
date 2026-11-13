import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Link, useParams } from "react-router-dom";

import { roomHealth, RoomHealth } from "../api";
import { StatusCard } from "../components/StatusCard";

export default function HealthPage() {
  const { id = "" } = useParams();
  const { t } = useTranslation();
  const [health, setHealth] = useState<RoomHealth | null>(null);

  useEffect(() => {
    roomHealth(id)
      .then(setHealth)
      .catch(() => setHealth(null));
  }, [id]);

  if (!health) return <p className="text-slate-500">…</p>;

  return (
    <div>
      <div className="flex items-center gap-3">
        <h1 className="text-lg font-bold text-slate-900">{t("health.title")}</h1>
        <Link to={`/rooms/${id}`} className="ml-auto text-xs text-slate-500 underline">
          {id}
        </Link>
      </div>
      <div className="mt-4 grid grid-cols-2 gap-3 md:grid-cols-4">
        <StatusCard id="uptime_s" label={t("health.uptime_s")} value={health.uptime_s ?? "—"} />
        <StatusCard id="rssi" label={t("health.rssi")} value={health.rssi ?? "—"} />
        <StatusCard id="heap_kb" label={t("health.heap_kb")} value={health.heap_kb ?? "—"} />
        <StatusCard id="chip_temp_c" label={t("health.chip_temp_c")} value={health.chip_temp_c ?? "—"} />
        <StatusCard id="fw" label={t("health.fw")} value={health.fw ?? "—"} />
        <StatusCard id="reset_reason" label={t("health.reset_reason")} value={health.reset_reason ?? "—"} />
        <StatusCard id="last_seen_at" label={t("health.last_seen_at")} value={health.last_seen_at ?? "—"} />
      </div>
    </div>
  );
}
