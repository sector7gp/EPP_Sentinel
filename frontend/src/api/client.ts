const API = "/api/v1";

function authHeaders(): HeadersInit {
  const token = localStorage.getItem("token");
  return token ? { Authorization: `Bearer ${token}` } : {};
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const res = await fetch(`${API}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...authHeaders(),
      ...(options.headers || {}),
    },
  });
  if (res.status === 401) {
    localStorage.removeItem("token");
    window.location.href = "/login";
    throw new Error("No autorizado");
  }
  if (!res.ok) {
    const err = await res.json().catch(() => ({}));
    throw new Error(err.detail || res.statusText);
  }
  if (res.status === 204) return undefined as T;
  return res.json();
}

export const api = {
  login: (username: string, password: string) =>
    request<{ access_token: string }>("/auth/login", {
      method: "POST",
      body: JSON.stringify({ username, password }),
    }),

  devices: () => request<Device[]>("/devices"),
  createDevice: (name: string) =>
    request<Device>("/devices", { method: "POST", body: JSON.stringify({ name }) }),

  updateSchedule: (deviceId: string, data: Schedule) =>
    request<Schedule>(`/devices/${deviceId}/schedule`, {
      method: "PUT",
      body: JSON.stringify(data),
    }),

  updateImageSettings: (deviceId: string, data: ImageSettings) =>
    request<ImageSettings>(`/devices/${deviceId}/image-settings`, {
      method: "PUT",
      body: JSON.stringify(data),
    }),

  assignProfile: (deviceId: string, profileId: string) =>
    request(`/devices/${deviceId}/profile`, {
      method: "PUT",
      body: JSON.stringify({ profile_id: profileId }),
    }),

  profiles: () => request<Profile[]>("/profiles"),
  createProfile: (data: { name: string; description?: string; required_epp: string[] }) =>
    request<Profile>("/profiles", { method: "POST", body: JSON.stringify(data) }),
  updateProfile: (id: string, data: Partial<{ name: string; description: string; required_epp: string[] }>) =>
    request<Profile>(`/profiles/${id}`, { method: "PUT", body: JSON.stringify(data) }),
  deleteProfile: (id: string) => request(`/profiles/${id}`, { method: "DELETE" }),

  aiSettings: () => request<AISettings>("/ai-settings"),
  updateAiSettings: (data: Partial<AISettingsUpdate>) =>
    request<AISettings>("/ai-settings", { method: "PUT", body: JSON.stringify(data) }),

  analyses: (params: URLSearchParams) =>
    request<AnalysisList>(`/analyses?${params}`),

  exportAnalyses: (params: URLSearchParams) => {
    const token = localStorage.getItem("token");
    return fetch(`${API}/analyses/export?${params}`, {
      headers: token ? { Authorization: `Bearer ${token}` } : {},
    });
  },

  auditLogs: (params?: URLSearchParams) =>
    request<AuditLogList>(`/audit-logs?${params || ""}`),
};

export type Device = {
  id: string;
  name: string;
  api_token: string;
  last_seen_at: string | null;
};

export type Schedule = {
  device_id?: string;
  start_time: string;
  end_time: string;
  interval_value: number;
  interval_unit: string;
  enabled_days: string;
};

export type ImageSettings = {
  device_id?: string;
  width: number;
  height: number;
  jpeg_quality: number;
  max_kb: number;
};

export type Profile = {
  id: string;
  name: string;
  description: string | null;
  required_epp: string[];
};

export type AISettings = {
  active_provider: string;
  model_name: string;
  openai_api_key_set: boolean;
  anthropic_api_key_set: boolean;
  gemini_api_key_set: boolean;
};

export type AISettingsUpdate = {
  active_provider?: string;
  model_name?: string;
  openai_api_key?: string;
  anthropic_api_key?: string;
  gemini_api_key?: string;
};

export type Analysis = {
  id: string;
  capture_id: string;
  device_id: string;
  device_name: string;
  image_url: string;
  captured_at: string;
  analyzed_at: string;
  provider: string;
  cumple_normativa: boolean;
  observaciones: string | null;
  epp_results: Record<string, boolean>;
  status: string;
};

export type AnalysisList = {
  items: Analysis[];
  total: number;
  page: number;
  page_size: number;
};

export type AuditLog = {
  id: string;
  event_type: string;
  message: string;
  device_id: string | null;
  user: string | null;
  created_at: string;
};

export type AuditLogList = {
  items: AuditLog[];
  total: number;
};

export const EPP_TYPES = [
  { id: "casco_seguridad", label: "Casco de seguridad" },
  { id: "gafas_seguridad", label: "Gafas de seguridad" },
  { id: "proteccion_auditiva", label: "Protección auditiva" },
  { id: "guantes_seguridad", label: "Guantes de seguridad" },
  { id: "calzado_seguridad", label: "Calzado de seguridad" },
  { id: "ropa_industrial", label: "Ropa industrial" },
  { id: "proteccion_respiratoria", label: "Protección respiratoria" },
  { id: "chaleco_reflectivo", label: "Chaleco reflectivo" },
];
