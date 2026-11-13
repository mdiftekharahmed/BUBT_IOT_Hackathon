import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import { activeAlerts, ActiveAlert, me } from "../api";
import { LiveClient, liveUrl } from "../live";

const POLL_MS = 2000; // Student/Guardian have no room socket: they poll the banner

/** Red FIRE/PANIC banner, present on every page.
 *
 * Teacher/Admin: triggered through the live socket (refetch on each push, well under 3 s).
 * Student/Guardian: 2 s polling of /api/alerts/active — the only alert source they get.
 */
export function Banner() {
  const { t } = useTranslation();
  const [alerts, setAlerts] = useState<ActiveAlert[]>([]);
  const [role, setRole] = useState<string | null>(null);

  useEffect(() => {
    me()
      .then((user) => setRole(user.role))
      .catch(() => setRole(null)); // logged out: no banner
  }, []);

  useEffect(() => {
    const load = () => {
      activeAlerts()
        .then(setAlerts)
        .catch(() => setAlerts([]));
    };
    if (role === "student" || role === "guardian") {
      load();
      const poll = setInterval(load, POLL_MS);
      return () => clearInterval(poll);
    }
    if (role === "teacher" || role === "admin") {
      load();
      const client = new LiveClient(liveUrl(), { onMessage: () => load() });
      client.connect();
      return () => client.close();
    }
    return undefined;
  }, [role]);

  if (alerts.length === 0) return null;
  return (
    <div role="alert" className="bg-red-600 px-4 py-2 text-center text-sm font-semibold text-white">
      {alerts.map((alert) => (
        <span key={`${alert.room_id}-${alert.kind}`} className="mx-2">
          {t(`alert.${alert.kind}`, { room: alert.room_id })}
        </span>
      ))}
    </div>
  );
}
