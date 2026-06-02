import { useQuery } from "@tanstack/react-query";
import { useMemo, useState } from "react";
import { api, eppLabel, type StreamDashboardItem } from "../api/client";

function EppList({
  required,
  results,
}: {
  required: string[];
  results: Record<string, boolean>;
}) {
  if (required.length === 0) {
    return <span className="muted">Sin EPP configurados</span>;
  }
  return (
    <ul className="epp-list">
      {required.map((epp) => {
        const ok = results[epp];
        return (
          <li key={epp} className={ok ? "epp-ok" : "epp-fail"}>
            {eppLabel(epp)}: {ok ? "✓" : "✗"}
          </li>
        );
      })}
    </ul>
  );
}

function StreamCard({
  item,
  focused,
  onFocus,
  onExpand,
}: {
  item: StreamDashboardItem;
  focused: boolean;
  onFocus: () => void;
  onExpand: () => void;
}) {
  const analysis = item.latest_analysis;
  const img = analysis?.image_url;

  return (
    <div
      className={`stream-card ${focused ? "focused" : ""}`}
      onClick={onFocus}
      onKeyDown={(e) => e.key === "Enter" && onFocus()}
      role="button"
      tabIndex={0}
    >
      <div className="stream-card-header">
        <strong>{item.stream_name}</strong>
        <span className={`badge ${item.device_online ? "ok" : "fail"}`}>
          {item.device_online ? "Online" : "Offline"}
        </span>
      </div>
      <p className="stream-meta">
        {item.device_name}
        {item.device_location ? ` · ${item.device_location}` : ""}
      </p>
      <p className="stream-meta">Perfil: {item.profile_name}</p>
      {img ? (
        <img
          className="stream-thumb"
          src={img}
          alt={item.stream_name}
          onClick={(e) => {
            e.stopPropagation();
            onExpand();
          }}
        />
      ) : (
        <div className="stream-thumb placeholder">Sin captura</div>
      )}
      {analysis && (
        <span className={`badge ${analysis.cumple_normativa ? "ok" : "fail"}`}>
          {analysis.cumple_normativa ? "Cumple" : "No cumple"}
        </span>
      )}
    </div>
  );
}

export default function DashboardPage() {
  const [focusedId, setFocusedId] = useState<string | null>(null);
  const [expanded, setExpanded] = useState<StreamDashboardItem | null>(null);
  const [historyPage, setHistoryPage] = useState(1);
  const [cumple, setCumple] = useState("");
  const [search, setSearch] = useState("");

  const { data: streams, isLoading, refetch } = useQuery({
    queryKey: ["dashboard-streams"],
    queryFn: api.dashboardStreams,
    refetchInterval: 30_000,
  });

  const focused = useMemo(() => {
    if (!streams?.length) return null;
    if (focusedId) return streams.find((s) => s.stream_id === focusedId) ?? streams[0];
    return streams[0];
  }, [streams, focusedId]);

  const historyParams = new URLSearchParams({ page: String(historyPage), page_size: "15" });
  if (cumple !== "") historyParams.set("cumple", cumple);
  if (search) historyParams.set("search", search);
  if (focused) historyParams.set("stream_id", focused.stream_id);

  const { data: history } = useQuery({
    queryKey: ["analyses", historyParams.toString()],
    queryFn: () => api.analyses(historyParams),
    enabled: !!focused,
  });

  async function handleExport() {
    const res = await api.exportAnalyses(historyParams);
    const blob = await res.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "analyses_export.csv";
    a.click();
    URL.revokeObjectURL(url);
  }

  return (
    <div>
      <div className="page-header">
        <h1>Dashboard de monitoreo</h1>
        <button type="button" className="secondary" onClick={() => refetch()}>
          Actualizar
        </button>
      </div>

      <div className="card">
        {isLoading && <p>Cargando streams...</p>}
        {!isLoading && (!streams || streams.length === 0) && (
          <p>No hay streams configurados. Agregue nodos y streams en Configuración.</p>
        )}
        {streams && streams.length > 0 && (
          <div className="stream-grid">
            {streams.map((item) => (
              <StreamCard
                key={item.stream_id}
                item={item}
                focused={focused?.stream_id === item.stream_id}
                onFocus={() => setFocusedId(item.stream_id)}
                onExpand={() => setExpanded(item)}
              />
            ))}
          </div>
        )}
      </div>

      {focused && (
        <div className="card">
          <h2>
            EPP — {focused.stream_name} ({focused.profile_name})
          </h2>
          {focused.latest_analysis ? (
            <>
              <EppList
                required={focused.required_epp}
                results={focused.latest_analysis.epp_results}
              />
              <p className="muted" style={{ marginTop: "0.75rem" }}>
                Último análisis: {new Date(focused.latest_analysis.analyzed_at).toLocaleString()}
                {focused.latest_analysis.observaciones
                  ? ` · ${focused.latest_analysis.observaciones}`
                  : ""}
              </p>
            </>
          ) : (
            <p className="muted">Aún no hay análisis para este stream.</p>
          )}
        </div>
      )}

      <div className="card">
        <h2>Historial {focused ? `— ${focused.stream_name}` : ""}</h2>
        <div className="filters">
          <div>
            <label>Cumplimiento</label>
            <select value={cumple} onChange={(e) => setCumple(e.target.value)}>
              <option value="">Todos</option>
              <option value="true">Cumple</option>
              <option value="false">No cumple</option>
            </select>
          </div>
          <div>
            <label>Búsqueda</label>
            <input
              placeholder="Observaciones..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
            />
          </div>
          <button type="button" className="secondary" onClick={handleExport}>
            Exportar CSV
          </button>
        </div>
        {history && (
          <>
            <table>
              <thead>
                <tr>
                  <th>Imagen</th>
                  <th>Fecha</th>
                  <th>Cumple</th>
                  <th>EPP (perfil)</th>
                  <th>Observaciones</th>
                </tr>
              </thead>
              <tbody>
                {history.items.map((row) => (
                  <tr key={row.id}>
                    <td>
                      <img
                        className="thumb clickable"
                        src={row.image_url}
                        alt="captura"
                        onClick={() => {
                          const item = streams?.find((s) => s.stream_id === row.stream_id);
                          if (item) setExpanded({ ...item, latest_analysis: row });
                        }}
                      />
                    </td>
                    <td>{new Date(row.analyzed_at).toLocaleString()}</td>
                    <td>
                      <span className={`badge ${row.cumple_normativa ? "ok" : "fail"}`}>
                        {row.cumple_normativa ? "Sí" : "No"}
                      </span>
                    </td>
                    <td>
                      <EppList
                        required={row.required_epp.length ? row.required_epp : focused?.required_epp ?? []}
                        results={row.epp_results}
                      />
                    </td>
                    <td>{row.observaciones || "—"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            <div className="pager">
              <button
                type="button"
                disabled={historyPage <= 1}
                onClick={() => setHistoryPage((p) => p - 1)}
              >
                Anterior
              </button>
              <span>
                Página {history.page} · Total {history.total}
              </span>
              <button
                type="button"
                disabled={historyPage * history.page_size >= history.total}
                onClick={() => setHistoryPage((p) => p + 1)}
              >
                Siguiente
              </button>
            </div>
          </>
        )}
      </div>

      {expanded && (
        <div className="modal-overlay" onClick={() => setExpanded(null)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <button type="button" className="modal-close" onClick={() => setExpanded(null)}>
              ✕ Cerrar
            </button>
            <h2>{expanded.stream_name}</h2>
            <p className="stream-meta">
              {expanded.device_name}
              {expanded.device_location ? ` · ${expanded.device_location}` : ""} · Perfil:{" "}
              {expanded.profile_name}
            </p>
            {(expanded.latest_analysis?.image_url) && (
              <img
                className="modal-image"
                src={expanded.latest_analysis.image_url}
                alt={expanded.stream_name}
              />
            )}
            {expanded.latest_analysis && (
              <>
                <span
                  className={`badge ${expanded.latest_analysis.cumple_normativa ? "ok" : "fail"}`}
                >
                  {expanded.latest_analysis.cumple_normativa ? "Cumple normativa" : "No cumple"}
                </span>
                <h3>EPP detectados</h3>
                <EppList
                  required={expanded.required_epp}
                  results={expanded.latest_analysis.epp_results}
                />
                {expanded.latest_analysis.observaciones && (
                  <p>{expanded.latest_analysis.observaciones}</p>
                )}
              </>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
