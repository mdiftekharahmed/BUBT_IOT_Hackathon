import { vi } from "vitest";

export type Reply = { status: number; body?: unknown };

export type Route = {
  match: (url: string, init?: RequestInit) => boolean;
  reply: (url: string, init?: RequestInit) => Reply;
};

/** A route that answers any request whose URL ends with `path`. */
export const jsonRoute = (path: string, status: number, body?: unknown): Route => ({
  match: (url) => url.endsWith(path),
  reply: () => ({ status, body }),
});

/** Replace global fetch with a deterministic router; returns the recorded calls. */
export function installFetch(routes: Route[]) {
  const calls: string[] = [];
  const mock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = String(input);
    calls.push(url);
    for (const route of routes) {
      if (route.match(url, init)) {
        const { status, body } = route.reply(url, init);
        return new Response(JSON.stringify(body ?? null), {
          status,
          headers: { "Content-Type": "application/json" },
        });
      }
    }
    return new Response(JSON.stringify({ detail: `unmocked: ${url}` }), { status: 500 });
  });
  vi.stubGlobal("fetch", mock);
  return { calls, mock };
}
