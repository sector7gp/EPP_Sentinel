import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { FormEvent, useEffect, useState } from "react";
import {
  api,
  EPP_TYPES,
  type Device,
  type ImageSettings,
  type Profile,
  type Schedule,
  type StreamInput,
  type VideoStream,
} from "../api/client";

const emptyStream = (profileId: string): StreamInput => ({
  name: "",
  source_type: "usb",
  connection_config: { device: "/dev/video0" },
  profile_id: profileId,
  enabled: true,
});

export default function ConfigPage() {
  const qc = useQueryClient();
  const { data: devices } = useQuery({ queryKey: ["devices"], queryFn: api.devices });
  const { data: profiles } = useQuery({ queryKey: ["profiles"], queryFn: api.profiles });
  const { data: aiSettings } = useQuery({ queryKey: ["ai-settings"], queryFn: api.aiSettings });

  const [deviceId, setDeviceId] = useState("");
  const [newDevice, setNewDevice] = useState({ name: "", location: "" });
  const [editDevice, setEditDevice] = useState({ name: "", location: "" });
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
  const [streamForm, setStreamForm] = useState<StreamInput | null>(null);
  const [editingStreamId, setEditingStreamId] = useState<string | null>(null);
  const [newProfile, setNewProfile] = useState({ name: "", description: "", required_epp: [] as string[] });
  const [editProfileId, setEditProfileId] = useState<string | null>(null);
  const [editProfile, setEditProfile] = useState({ name: "", description: "", required_epp: [] as string[] });
  const [aiForm, setAiForm] = useState({
    active_provider: "openai",
    model_name: "gpt-4o-mini",
    openai_api_key: "",
    anthropic_api_key: "",
    gemini_api_key: "",
  });
  const [error, setError] = useState("");

  const { data: streams, refetch: refetchStreams } = useQuery({
    queryKey: ["streams", deviceId],
    queryFn: () => api.streams(deviceId),
    enabled: !!deviceId,
  });

  const { data: deviceConfig } = useQuery({
    queryKey: ["device-config", deviceId],
    queryFn: () => api.deviceConfig(deviceId),
    enabled: !!deviceId,
  });

  const [scheduleMessage, setScheduleMessage] = useState("");
  const [imageMessage, setImageMessage] = useState("");

  useEffect(() => {
    if (devices?.length && !deviceId) setDeviceId(devices[0].id);
  }, [devices, deviceId]);

  useEffect(() => {
    if (deviceConfig) {
      setSchedule(deviceConfig.schedule);
      setImage(deviceConfig.image_settings);
    }
  }, [deviceConfig]);

  useEffect(() => {
    const d = devices?.find((x) => x.id === deviceId);
    if (d) setEditDevice({ name: d.name, location: d.location || "" });
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

  useEffect(() => {
    if (profiles?.length && !streamForm) {
      setStreamForm(emptyStream(profiles[0].id));
    }
  }, [profiles, streamForm]);

  const saveSchedule = useMutation({
    mutationFn: () => api.updateSchedule(deviceId, schedule),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["devices"] });
      qc.invalidateQueries({ queryKey: ["device-config", deviceId] });
      setScheduleMessage("Horario guardado correctamente.");
      setTimeout(() => setScheduleMessage(""), 3000);
    },
    onError: (e: Error) => setError(e.message),
  });

  const saveImage = useMutation({
    mutationFn: () => api.updateImageSettings(deviceId, image),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["devices"] });
      qc.invalidateQueries({ queryKey: ["device-config", deviceId] });
      setImageMessage("Configuración de imagen guardada correctamente.");
      setTimeout(() => setImageMessage(""), 3000);
    },
    onError: (e: Error) => setError(e.message),
  });

  const createDevice = useMutation({
    mutationFn: () =>
      api.createDevice({
        name: newDevice.name,
        location: newDevice.location || undefined,
      }),
    onSuccess: () => {
      setNewDevice({ name: "", location: "" });
      qc.invalidateQueries({ queryKey: ["devices"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const patchDevice = useMutation({
    mutationFn: () =>
      api.updateDevice(deviceId, {
        name: editDevice.name,
        location: editDevice.location,
      }),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["devices"] }),
    onError: (e: Error) => setError(e.message),
  });

  const removeDevice = useMutation({
    mutationFn: () => api.deleteDevice(deviceId),
    onSuccess: () => {
      setDeviceId("");
      qc.invalidateQueries({ queryKey: ["devices"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const saveStream = useMutation({
    mutationFn: () => {
      if (!streamForm) throw new Error("Formulario incompleto");
      const payload = {
        ...streamForm,
        connection_config:
          streamForm.source_type === "usb"
            ? { device: streamForm.connection_config.device || "/dev/video0" }
            : { url: streamForm.connection_config.url || "" },
      };
      return editingStreamId
        ? api.updateStream(deviceId, editingStreamId, payload)
        : api.createStream(deviceId, payload);
    },
    onSuccess: () => {
      setStreamForm(emptyStream(profiles?.[0]?.id || ""));
      setEditingStreamId(null);
      refetchStreams();
      qc.invalidateQueries({ queryKey: ["devices"] });
      qc.invalidateQueries({ queryKey: ["dashboard-streams"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const deleteStreamMut = useMutation({
    mutationFn: (streamId: string) => api.deleteStream(deviceId, streamId),
    onSuccess: () => {
      refetchStreams();
      qc.invalidateQueries({ queryKey: ["devices"] });
      qc.invalidateQueries({ queryKey: ["dashboard-streams"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const createProfile = useMutation({
    mutationFn: () => api.createProfile(newProfile),
    onSuccess: () => {
      setNewProfile({ name: "", description: "", required_epp: [] });
      qc.invalidateQueries({ queryKey: ["profiles"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const updateProfileMut = useMutation({
    mutationFn: () => {
      if (!editProfileId) throw new Error("Perfil no seleccionado");
      return api.updateProfile(editProfileId, editProfile);
    },
    onSuccess: () => {
      setEditProfileId(null);
      qc.invalidateQueries({ queryKey: ["profiles"] });
      qc.invalidateQueries({ queryKey: ["dashboard-streams"] });
    },
    onError: (e: Error) => setError(e.message),
  });

  const deleteProfileMut = useMutation({
    mutationFn: (id: string) => api.deleteProfile(id),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["profiles"] }),
    onError: (e: Error) => setError(e.message),
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

  function toggleEpp(list: string[], epp: string, setter: (v: string[]) => void) {
    setter(list.includes(epp) ? list.filter((x) => x !== epp) : [...list, epp]);
  }

  function startEditStream(stream: VideoStream) {
    setEditingStreamId(stream.id);
    setStreamForm({
      name: stream.name,
      source_type: stream.source_type,
      connection_config: { ...stream.connection_config },
      profile_id: stream.profile_id,
      enabled: stream.enabled,
    });
  }

  function startEditProfile(p: Profile) {
    setEditProfileId(p.id);
    setEditProfile({
      name: p.name,
      description: p.description || "",
      required_epp: [...p.required_epp],
    });
  }

  const selectedDevice = devices?.find((d) => d.id === deviceId);

  return (
    <div>
      <h1>Configuración</h1>
      {error && <p className="error">{error}</p>}

      <div className="card">
        <h2>Nodos Raspberry Pi</h2>
        <form
          onSubmit={(e: FormEvent) => {
            e.preventDefault();
            setError("");
            createDevice.mutate();
          }}
          className="inline-form"
        >
          <input
            placeholder="Nombre del nodo"
            value={newDevice.name}
            onChange={(e) => setNewDevice((d) => ({ ...d, name: e.target.value }))}
            required
          />
          <input
            placeholder="Ubicación (opcional)"
            value={newDevice.location}
            onChange={(e) => setNewDevice((d) => ({ ...d, location: e.target.value }))}
          />
          <button type="submit" disabled={!newDevice.name}>
            Registrar nodo
          </button>
        </form>

        {devices?.map((d: Device) => (
          <div key={d.id} className={`device-row ${deviceId === d.id ? "selected" : ""}`}>
            <button type="button" className="linkish" onClick={() => setDeviceId(d.id)}>
              <strong>{d.name}</strong>
            </button>
            {d.location && <span> · {d.location}</span>}
            <span className={`badge ${d.online ? "ok" : "fail"}`}>
              {d.online ? "Online" : "Offline"}
            </span>
            <span className="muted"> · {d.stream_count} stream(s)</span>
            <br />
            <code className="small">ID: {d.id}</code> · <code className="small">Token: {d.api_token}</code>
            {d.last_seen_at && (
              <span className="muted"> · Última conexión: {new Date(d.last_seen_at).toLocaleString()}</span>
            )}
          </div>
        ))}
      </div>

      {selectedDevice && (
        <>
          <div className="card">
            <h2>Editar nodo seleccionado</h2>
            <label>Nombre</label>
            <input
              value={editDevice.name}
              onChange={(e) => setEditDevice((d) => ({ ...d, name: e.target.value }))}
            />
            <label>Ubicación</label>
            <input
              value={editDevice.location}
              onChange={(e) => setEditDevice((d) => ({ ...d, location: e.target.value }))}
            />
            <div className="btn-row">
              <button
                type="button"
                onClick={() => {
                  setError("");
                  patchDevice.mutate();
                }}
              >
                Guardar nodo
              </button>
              <button
                type="button"
                className="secondary danger"
                onClick={() => {
                  if (confirm(`¿Eliminar nodo "${selectedDevice.name}" y todos sus streams?`)) {
                    setError("");
                    removeDevice.mutate();
                  }
                }}
              >
                Eliminar nodo
              </button>
            </div>
          </div>

          <div className="card">
            <h2>Streams ({streams?.length ?? 0}/4)</h2>
            {streams?.map((s) => (
              <div key={s.id} className="stream-row">
                <strong>{s.name}</strong>
                <span className={`badge ${s.enabled ? "ok" : "fail"}`}>
                  {s.enabled ? "Activo" : "Inactivo"}
                </span>
                <span className="muted">
                  {" "}
                  · {s.source_type.toUpperCase()} · Perfil: {s.profile_name}
                </span>
                {s.source_type === "usb" && (
                  <span className="muted"> · {s.connection_config.device}</span>
                )}
                {s.source_type === "rtsp" && (
                  <span className="muted"> · {s.connection_config.url}</span>
                )}
                <div className="btn-row">
                  <button type="button" className="secondary" onClick={() => startEditStream(s)}>
                    Editar
                  </button>
                  <button
                    type="button"
                    className="secondary danger"
                    onClick={() => {
                      if (confirm(`¿Eliminar stream "${s.name}"?`)) deleteStreamMut.mutate(s.id);
                    }}
                  >
                    Eliminar
                  </button>
                </div>
              </div>
            ))}

            {streamForm && (streams?.length ?? 0) < 4 && (
              <div className="stream-form">
                <h3>{editingStreamId ? "Editar stream" : "Nuevo stream"}</h3>
                <label>Nombre</label>
                <input
                  value={streamForm.name}
                  onChange={(e) => setStreamForm((s) => s && { ...s, name: e.target.value })}
                  required
                />
                <label>Tipo de origen</label>
                <select
                  value={streamForm.source_type}
                  onChange={(e) =>
                    setStreamForm(
                      (s) =>
                        s && {
                          ...s,
                          source_type: e.target.value as "usb" | "rtsp",
                          connection_config:
                            e.target.value === "usb"
                              ? { device: "/dev/video0" }
                              : { url: "" },
                        }
                    )
                  }
                >
                  <option value="usb">USB</option>
                  <option value="rtsp">RTSP</option>
                </select>
                {streamForm.source_type === "usb" ? (
                  <>
                    <label>Dispositivo USB</label>
                    <input
                      placeholder="/dev/video0"
                      value={streamForm.connection_config.device || ""}
                      onChange={(e) =>
                        setStreamForm(
                          (s) =>
                            s && {
                              ...s,
                              connection_config: { device: e.target.value },
                            }
                        )
                      }
                    />
                  </>
                ) : (
                  <>
                    <label>URL RTSP</label>
                    <input
                      placeholder="rtsp://usuario:pass@host:554/stream"
                      value={streamForm.connection_config.url || ""}
                      onChange={(e) =>
                        setStreamForm(
                          (s) =>
                            s && {
                              ...s,
                              connection_config: { url: e.target.value },
                            }
                        )
                      }
                    />
                  </>
                )}
                <label>Perfil EPP</label>
                <select
                  value={streamForm.profile_id}
                  onChange={(e) =>
                    setStreamForm((s) => s && { ...s, profile_id: e.target.value })
                  }
                >
                  {profiles?.map((p) => (
                    <option key={p.id} value={p.id}>
                      {p.name}
                    </option>
                  ))}
                </select>
                <label className="checkbox-inline">
                  <input
                    type="checkbox"
                    checked={streamForm.enabled}
                    onChange={(e) =>
                      setStreamForm((s) => s && { ...s, enabled: e.target.checked })
                    }
                  />
                  Stream activo
                </label>
                <div className="btn-row">
                  <button
                    type="button"
                    disabled={!streamForm.name || !streamForm.profile_id}
                    onClick={() => {
                      setError("");
                      saveStream.mutate();
                    }}
                  >
                    {editingStreamId ? "Guardar stream" : "Agregar stream"}
                  </button>
                  {editingStreamId && (
                    <button
                      type="button"
                      className="secondary"
                      onClick={() => {
                        setEditingStreamId(null);
                        setStreamForm(emptyStream(profiles?.[0]?.id || ""));
                      }}
                    >
                      Cancelar
                    </button>
                  )}
                </div>
              </div>
            )}
            {(streams?.length ?? 0) >= 4 && !editingStreamId && (
              <p className="muted">Máximo 4 streams por nodo alcanzado.</p>
            )}
          </div>

          <div className="card grid-2">
            <div>
              <h2>Horario de captura</h2>
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
              {scheduleMessage && <p className="success">{scheduleMessage}</p>}
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
              {imageMessage && <p className="success">{imageMessage}</p>}
            </div>
          </div>
        </>
      )}

      <div className="card">
        <h2>Perfiles EPP</h2>
        <h3>Nuevo perfil</h3>
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
                onChange={() =>
                  toggleEpp(newProfile.required_epp, epp.id, (v) =>
                    setNewProfile((p) => ({ ...p, required_epp: v }))
                  )
                }
              />
              {epp.label}
            </label>
          ))}
        </div>
        <button type="button" disabled={!newProfile.name} onClick={() => createProfile.mutate()}>
          Crear perfil
        </button>

        {profiles && profiles.length > 0 && (
          <div style={{ marginTop: "1.5rem" }}>
            <h3>Perfiles existentes</h3>
            {profiles.map((p) => (
              <div key={p.id} className="profile-row">
                {editProfileId === p.id ? (
                  <>
                    <input
                      value={editProfile.name}
                      onChange={(e) => setEditProfile((x) => ({ ...x, name: e.target.value }))}
                    />
                    <textarea
                      value={editProfile.description}
                      onChange={(e) =>
                        setEditProfile((x) => ({ ...x, description: e.target.value }))
                      }
                    />
                    <div className="checkbox-grid">
                      {EPP_TYPES.map((epp) => (
                        <label key={epp.id}>
                          <input
                            type="checkbox"
                            checked={editProfile.required_epp.includes(epp.id)}
                            onChange={() =>
                              toggleEpp(editProfile.required_epp, epp.id, (v) =>
                                setEditProfile((x) => ({ ...x, required_epp: v }))
                              )
                            }
                          />
                          {epp.label}
                        </label>
                      ))}
                    </div>
                    <div className="btn-row">
                      <button type="button" onClick={() => updateProfileMut.mutate()}>
                        Guardar
                      </button>
                      <button type="button" className="secondary" onClick={() => setEditProfileId(null)}>
                        Cancelar
                      </button>
                    </div>
                  </>
                ) : (
                  <>
                    <strong>{p.name}</strong>
                    <span className="muted"> — {p.required_epp.join(", ") || "sin EPP"}</span>
                    <div className="btn-row">
                      <button type="button" className="secondary" onClick={() => startEditProfile(p)}>
                        Editar
                      </button>
                      <button
                        type="button"
                        className="secondary danger"
                        onClick={() => {
                          if (confirm(`¿Eliminar perfil "${p.name}"?`)) deleteProfileMut.mutate(p.id);
                        }}
                      >
                        Eliminar
                      </button>
                    </div>
                  </>
                )}
              </div>
            ))}
          </div>
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
