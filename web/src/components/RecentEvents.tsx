import { useTranslation } from "react-i18next";

import { RoomEvent } from "../api";

export function RecentEvents({ events }: { events: RoomEvent[] }) {
  const { t } = useTranslation();
  return (
    <section data-testid="recent-events" className="mt-6 rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <h2 className="text-sm font-semibold uppercase tracking-wide text-slate-500">{t("room.events")}</h2>
      {events.length === 0 ? (
        <p className="mt-2 text-slate-500">{t("room.no_events")}</p>
      ) : (
        <ul className="mt-2 divide-y divide-slate-100">
          {events.map((event) => (
            <li key={`${event.device_id}-${event.seq}`} className="flex items-baseline gap-3 py-2 text-sm">
              <span className="font-semibold text-slate-900">{event.type}</span>
              <span className="text-slate-600">{event.detail}</span>
              <time className="ml-auto text-xs text-slate-400">{event.ts}</time>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
