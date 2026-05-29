import { useQuery } from "@tanstack/react-query";
import { useState } from "react";
import { api } from "../api/client";

export default function DashboardPage() {
  const [page, setPage] = useState(1);
  const [cumple, setCumple] = useState<string>("");
  const [search, setSearch] = useState("");
  const [fromDate, setFromDate] = useState("");
  const [toDate, setToDate] = useState("");

  const params = new URLSearchParams({ page: String(page), page_size: "20" });
  if (cumple !== "") params.set("cumple", cumple);
  if (search) params.set("search", search);
  if (fromDate) params.set("from_date", new Date(fromDate).toISOString());
  if (toDate) params.set("to_date", new Date(toDate).toISOString());

  const { data, isLoading, refetch } = useQuery({
    queryKey: ["analyses", params.toString()],
    queryFn: () => api.analyses(params),
  });

  async function handleExport() {
    const res = await api.exportAnalyses(params);
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
      <h1>Dashboard de análisis</h1>
      <div className="card filters">
        <div>
          <label>Cumplimiento</label>
          <select value={cumple} onChange={(e) => setCumple(e.target.value)}>
            <option value="">Todos</option>
            <option value="true">Cumple</option>
            <option value="false">No cumple</option>
          </select>
        </div>
        <div>
          <label>Desde</label>
          <input type="date" value={fromDate} onChange={(e) => setFromDate(e.target.value)} />
        </div>
        <div>
          <label>Hasta</label>
          <input type="date" value={toDate} onChange={(e) => setToDate(e.target.value)} />
        </div>
        <div>
          <label>Búsqueda</label>
          <input
            placeholder="Observaciones, dispositivo..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        </div>
        <button type="button" onClick={() => refetch()}>
          Filtrar
        </button>
        <button type="button" className="secondary" onClick={handleExport}>
          Exportar CSV
        </button>
      </div>

      <div className="card">
        {isLoading && <p>Cargando...</p>}
        {data && (
          <>
            <p>
              Total: {data.total} · Página {data.page}
            </p>
            <table>
              <thead>
                <tr>
                  <th>Imagen</th>
                  <th>Fecha</th>
                  <th>Dispositivo</th>
                  <th>Cumple</th>
                  <th>EPP</th>
                  <th>Observaciones</th>
                </tr>
              </thead>
              <tbody>
                {data.items.map((row) => (
                  <tr key={row.id}>
                    <td>
                      <img className="thumb" src={row.image_url} alt="captura" />
                    </td>
                    <td>{new Date(row.analyzed_at).toLocaleString()}</td>
                    <td>{row.device_name}</td>
                    <td>
                      <span className={`badge ${row.cumple_normativa ? "ok" : "fail"}`}>
                        {row.cumple_normativa ? "Sí" : "No"}
                      </span>
                    </td>
                    <td>
                      {Object.entries(row.epp_results)
                        .map(([k, v]) => `${k.replace(/_/g, " ")}: ${v ? "✓" : "✗"}`)
                        .join(", ")}
                    </td>
                    <td>{row.observaciones || "—"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            <div style={{ marginTop: "1rem", display: "flex", gap: "0.5rem" }}>
              <button
                type="button"
                disabled={page <= 1}
                onClick={() => setPage((p) => p - 1)}
              >
                Anterior
              </button>
              <button
                type="button"
                disabled={!data || page * data.page_size >= data.total}
                onClick={() => setPage((p) => p + 1)}
              >
                Siguiente
              </button>
            </div>
          </>
        )}
      </div>
    </div>
  );
}
