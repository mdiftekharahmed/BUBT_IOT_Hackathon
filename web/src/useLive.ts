import { useCallback, useEffect, useState } from "react";

import { roomEvents, roomState, RoomEvent, RoomState } from "./api";
import { LiveClient, LiveMessage, liveUrl } from "./live";

export type Live = {
  state: RoomState | null;
  events: RoomEvent[];
  connected: boolean;
};

/** Room state + recent events, refreshed on every live push and after each reconnect. */
export function useLive(roomId: string): Live {
  const [state, setState] = useState<RoomState | null>(null);
  const [events, setEvents] = useState<RoomEvent[]>([]);
  const [connected, setConnected] = useState(false);

  const refetch = useCallback(async () => {
    try {
      const [nextState, nextEvents] = await Promise.all([roomState(roomId), roomEvents(roomId)]);
      setState(nextState);
      setEvents(nextEvents);
    } catch {
      /* role without room access (or offline): the banner still works */
    }
  }, [roomId]);

  useEffect(() => {
    const client = new LiveClient(liveUrl(), {
      onOpen: () => {
        void refetch();
      },
      onStatus: setConnected,
      onMessage: (message: LiveMessage) => {
        if (!message || typeof message !== "object" || typeof message.type !== "string") return;
        if (message.room_id !== roomId) return;
        if (
          message.type === "telemetry" ||
          message.type === "event" ||
          message.type === "device_online" ||
          message.type === "device_offline"
        ) {
          void refetch();
        }
      },
    });
    client.connect();
    void refetch();
    return () => client.close();
  }, [roomId, refetch]);

  return { state, events, connected };
}
