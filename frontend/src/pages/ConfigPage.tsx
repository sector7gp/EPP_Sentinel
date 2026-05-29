import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { FormEvent, useEffect, useState } from "react";
import { api, EPP_TYPES, type Device, type ImageSettings, type Schedule } from "../api/client";

export default function ConfigPage() {
  const qc = useQueryClient();
  const { data: devices } = useQuery({ queryKey: ["devices"], queryFn: api.devices });
  const { data: profiles } = useQuery({ queryKey: ["profiles"], queryFn: api.profiles });
  const { data: aiSettings } = useQuery({ queryKey: ["ai-settings"], queryFn: api.aiSettings });

  const [deviceId, setDeviceId] = useState("");
  const [schedule, setSchedule] = useState<Schedule>({
    start_time: "07:00:00",
    end_time: "18:00:00",
    interval_value: 5,
    interval_unit: "minutes",
    enabled_days: "0,1,2,3,4,5,6",
  });
  const [image, setImage] = useState<ImageSettings>({
    width: 1280,
    height: 720,
    jpeg_quality: 75,
    max_kb: 500,
  });
  const [profileId, setProfileId] = useState("");
  const [newDeviceName, setNewDeviceName] = useState("");
  const [newProfile, setNewProfile] = useState({ name: "", description: "", required_epp: [] as string[] });
  const [aiForm, setAiForm] = useState({
    active_provider: "openai",
    model_name: "gpt-4o-mini",
    openai_api_key: "",
    anthropic_api_key: "",
    gemini_api_key: "",
  });

  useEffect(() => {
    if (devices?.length && !deviceId) setDeviceId(devices[0].id);
  }, [devices, deviceId]);

  useEffect(() => {
    if (aiSettings) {
      setAiForm((f) => ({
        ...f,
        active_provider: aiSettings.active_provider,
        model_name: aiSettings.model_name || f.model_name,
      }));
    }
  }, [aiSettings]);

  const saveSchedule = useMutation({
    mutationFn: () => api.updateSchedule(deviceId, schedule),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["devices"] }),
  });

  const saveImage = useMutation({
    mutationFn: () => api.updateImageSettings(deviceId, image),
  });

  const assignProfile = useMutation({
    mutationFn: () => api.assignProfile(deviceId, profileId),
  });

  const createDevice = useMutation({
    mutationFn: () => api.createDevice(newDeviceName),
    onSuccess: () => {
      setNewDeviceName("");
      qc.invalidateQueries({ queryKey: ["devices"] });
    },
  });

  const createProfile = useMutation({
    mutationFn: () => api.createProfile(newProfile),
    onSuccess: () => {
      setNewProfile({ name: "", description: "", required_epp: [] });
      qc.invalidateQueries({ queryKey: ["profiles"] });
    },
  });

  const saveAi = useMutation({
    mutationFn: () =>
      api.updateAiSettings({
        active_provider: aiForm.active_provider,
        model_name: aiForm.model_name,
        ...(aiForm.openai_api_key ? { openai_api_key: aiForm.openai_api_key } : {}),
        ...(aiForm.anthropic_api_key ? { anthropic_api_key: aiForm.anthropic_api_key } : {}),
        ...(aiForm.gemini_api_key ? { gemini_api_key: aiForm.gemini_api_key } : {}),
      }),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["ai-settings"] }),
  });

  function toggleEpp(epp: string) {
    setNewProfile((p) => ({
      ...p,
      required_epp: p.required_epp.includes(epp)
        ? p.required_epp.filter((x) => x !== epp)
        : [...p.required_epp, epp],
    }));
  }

  return (
    <div>
      <h1>Configuración</h1>

      <div className="card">
        <h2>Dispositivos</h2>
        <form
          onSubmit={(e: FormEvent) => {
            e.preventDefault();
            createDevice.mutate();
          }}
          style={{ display: "flex", gap: "0.5rem", flexWrap: "wrap" }}
        >
          <input
            placeholder="Nombre del dispositivo"
            value={newDeviceName}
            onChange={(e) => setNewDeviceName(e.target.value)}
            style={{ flex: 1, minWidth: 200 }}
          />
          <button type="submit" disabled={!newDeviceName}>
            Registrar Pi
          </button>
        </form>
        {devices?.map((d: Device) => (
          <div key={d.id} style={{ marginTop: "0.75rem", fontSize: "0.9rem" }}>
            <strong>{d.name}</strong> · ID: <code>{d.id}</code> · Token:{" "}
            <code>{d.api_token}</code>
            {d.last_seen_at && (
              <> · Última conexión: {new Date(d.last_seen_at).toLocaleString()}</>
            )}
          </div>
        ))}
      </div>

      {deviceId && (
        <>
          <div className="card grid-2">
            <div>
              <h2>Horario de captura</h2>
              <label>Dispositivo</label>
              <select value={deviceId} onChange={(e) => setDeviceId(e.target.value)}>
                {devices?.map((d) => (
                  <option key={d.id} value={d.id}>
                    {d.name}
                  </option>
                ))}
              </select>
              <label>Inicio</label>
              <input
                type="time"
                value={schedule.start_time.slice(0, 5)}
                onChange={(e) =>
                  setSchedule((s) => ({ ...s, start_time: e.target.value + ":00" }))
                }
              />
              <label>Fin</label>
              <input
                type="time"
                value={schedule.end_time.slice(0, 5)}
                onChange={(e) =>
                  setSchedule((s) => ({ ...s, end_time: e.target.value + ":00" }))
                }
              />
              <label>Intervalo</label>
              <input
                type="number"
                min={1}
                value={schedule.interval_value}
                onChange={(e) =>
                  setSchedule((s) => ({ ...s, interval_value: Number(e.target.value) }))
                }
              />
              <label>Unidad</label>
              <select
                value={schedule.interval_unit}
                onChange={(e) => setSchedule((s) => ({ ...s, interval_unit: e.target.value }))}
              >
                <option value="seconds">Segundos</option>
                <option value="minutes">Minutos</option>
              </select>
              <label>Días (0=lun … 6=dom)</label>
              <input
                value={schedule.enabled_days}
                onChange={(e) => setSchedule((s) => ({ ...s, enabled_days: e.target.value }))}
              />
              <button type="button" onClick={() => saveSchedule.mutate()}>
                Guardar horario
              </button>
            </div>

            <div>
              <h2>Calidad de imagen</h2>
              <label>Ancho</label>
              <input
                type="number"
                value={image.width}
                onChange={(e) => setImage((i) => ({ ...i, width: Number(e.target.value) }))}
              />
              <label>Alto</label>
              <input
                type="number"
                value={image.height}
                onChange={(e) => setImage((i) => ({ ...i, height: Number(e.target.value) }))}
              />
              <label>Calidad JPEG</label>
              <input
                type="number"
                value={image.jpeg_quality}
                onChange={(e) =>
                  setImage((i) => ({ ...i, jpeg_quality: Number(e.target.value) }))
                }
              />
              <label>Máx. KB</label>
              <input
                type="number"
                value={image.max_kb}
                onChange={(e) => setImage((i) => ({ ...i, max_kb: Number(e.target.value) }))}
              />
              <button type="button" onClick={() => saveImage.mutate()}>
                Guardar imagen
              </button>
            </div>
          </div>

          <div className="card">
            <h2>Perfil de operario</h2>
            <select value={profileId} onChange={(e) => setProfileId(e.target.value)}>
              <option value="">Seleccionar perfil</option>
              {profiles?.map((p) => (
                <option key={p.id} value={p.id}>
                  {p.name}
                </option>
              ))}
            </select>
            <button
              type="button"
              disabled={!profileId}
              onClick={() => assignProfile.mutate()}
            >
              Asignar al dispositivo
            </button>
          </div>
        </>
      )}

      <div className="card">
        <h2>Nuevo perfil EPP</h2>
        <label>Nombre</label>
        <input
          value={newProfile.name}
          onChange={(e) => setNewProfile((p) => ({ ...p, name: e.target.value }))}
        />
        <label>Descripción</label>
        <textarea
          value={newProfile.description}
          onChange={(e) => setNewProfile((p) => ({ ...p, description: e.target.value }))}
        />
        <div className="checkbox-grid">
          {EPP_TYPES.map((epp) => (
            <label key={epp.id}>
              <input
                type="checkbox"
                checked={newProfile.required_epp.includes(epp.id)}
                onChange={() => toggleEpp(epp.id)}
              />
              {epp.label}
            </label>
          ))}
        </div>
        <button type="button" onClick={() => createProfile.mutate()}>
          Crear perfil
        </button>
        {profiles && profiles.length > 0 && (
          <ul style={{ marginTop: "1rem" }}>
            {profiles.map((p) => (
              <li key={p.id}>
                <strong>{p.name}</strong>: {p.required_epp.join(", ") || "sin EPP"}
              </li>
            ))}
          </ul>
        )}
      </div>

      <div className="card">
        <h2>Proveedor de IA</h2>
        <label>Proveedor</label>
        <select
          value={aiForm.active_provider}
          onChange={(e) => setAiForm((f) => ({ ...f, active_provider: e.target.value }))}
        >
          <option value="openai">OpenAI</option>
          <option value="anthropic">Anthropic Claude</option>
          <option value="gemini">Google Gemini</option>
        </select>
        <label>Modelo</label>
        <input
          value={aiForm.model_name}
          onChange={(e) => setAiForm((f) => ({ ...f, model_name: e.target.value }))}
        />
        <label>OpenAI API Key {aiSettings?.openai_api_key_set ? "(configurada)" : ""}</label>
        <input
          type="password"
          placeholder="Dejar vacío para no cambiar"
          value={aiForm.openai_api_key}
          onChange={(e) => setAiForm((f) => ({ ...f, openai_api_key: e.target.value }))}
        />
        <label>Anthropic API Key {aiSettings?.anthropic_api_key_set ? "(configurada)" : ""}</label>
        <input
          type="password"
          placeholder="Dejar vacío para no cambiar"
          value={aiForm.anthropic_api_key}
          onChange={(e) => setAiForm((f) => ({ ...f, anthropic_api_key: e.target.value }))}
        />
        <label>Gemini API Key {aiSettings?.gemini_api_key_set ? "(configurada)" : ""}</label>
        <input
          type="password"
          placeholder="Dejar vacío para no cambiar"
          value={aiForm.gemini_api_key}
          onChange={(e) => setAiForm((f) => ({ ...f, gemini_api_key: e.target.value }))}
        />
        <button type="button" onClick={() => saveAi.mutate()}>
          Guardar IA
        </button>
      </div>
    </div>
  );
}
