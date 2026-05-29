import { useQuery } from "@tanstack/react-query";
import { useState } from "react";
import { api } from "../api/client";

export default function LogsPage() {
  const [eventType, setEventType] = useState("");
  const params = new URLSearchParams({ limit: "200" });
  if (eventType) params.set("event_type", eventType);

  const { data, isLoading } = useQuery({
    queryKey: ["audit-logs", params.toString()],
    queryFn: () => api.auditLogs(params),
  });

  return (
    <div>
      <h1>Logs de auditoría</h1>
      <div className="card filters">
        <div>
          <label>Tipo de evento</label>
          <select value={eventType} onChange={(e) => setEventType(e.target.value)}>
            <option value="">Todos</option>
            <option value="capture_received">capture_received</option>
            <option value="upload_success">upload_success</option>
            <option value="capture_failed">capture_failed</option>
            <option value="connection_error">connection_error</option>
            <option value="ai_response">ai_response</option>
            <option value="ai_error">ai_error</option>
            <option value="config_change">config_change</option>
          </select>
        </div>
      </div>
      <div className="card">
        {isLoading && <p>Cargando...</p>}
        {data && (
          <table>
            <thead>
              <tr>
                <th>Fecha</th>
                <th>Tipo</th>
                <th>Mensaje</th>
                <th>Dispositivo</th>
                <th>Usuario</th>
              </tr>
            </thead>
            <tbody>
              {data.items.map((log) => (
                <tr key={log.id}>
                  <td>{new Date(log.created_at).toLocaleString()}</td>
                  <td>{log.event_type}</td>
                  <td>{log.message}</td>
                  <td>{log.device_id || "—"}</td>
                  <td>{log.user || "—"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
