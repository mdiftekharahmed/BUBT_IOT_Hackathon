import { ReactNode } from "react";

export function StatusCard({ id, label, value }: { id: string; label: string; value: ReactNode }) {
  return (
    <div data-testid={`card-${id}`} className="rounded-lg border border-slate-200 bg-white p-4 shadow-sm">
      <span className="block text-xs font-medium uppercase tracking-wide text-slate-500">{label}</span>
      <span className="mt-1 block text-xl font-semibold text-slate-900">{value}</span>
    </div>
  );
}
