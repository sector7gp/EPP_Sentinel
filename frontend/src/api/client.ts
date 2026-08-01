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
    const detail = Array.isArray(err.detail)
      ? err.detail.map((d: { msg?: string }) => d.msg).join("; ")
      : err.detail;
    throw new Error(detail || res.statusText);
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
  createDevice: (data: { name: string; location?: string }) =>
    request<Device>("/devices", { method: "POST", body: JSON.stringify(data) }),
  updateDevice: (id: string, data: { name?: string; location?: string }) =>
    request<Device>(`/devices/${id}`, { method: "PATCH", body: JSON.stringify(data) }),
  deleteDevice: (id: string) => request(`/devices/${id}`, { method: "DELETE" }),

  streams: (deviceId: string) => request<VideoStream[]>(`/devices/${deviceId}/streams`),
  createStream: (deviceId: string, data: StreamInput) =>
    request<VideoStream>(`/devices/${deviceId}/streams`, {
      method: "POST",
      body: JSON.stringify(data),
    }),
  updateStream: (deviceId: string, streamId: string, data: Partial<StreamInput>) =>
    request<VideoStream>(`/devices/${deviceId}/streams/${streamId}`, {
      method: "PUT",
      body: JSON.stringify(data),
    }),
  deleteStream: (deviceId: string, streamId: string) =>
    request(`/devices/${deviceId}/streams/${streamId}`, { method: "DELETE" }),

  deviceConfig: (deviceId: string) => request<DeviceConfig>(`/devices/${deviceId}/config`),

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

  profiles: () => request<Profile[]>("/profiles"),
  createProfile: (data: { name: string; description?: string; required_epp: string[] }) =>
    request<Profile>("/profiles", { method: "POST", body: JSON.stringify(data) }),
  updateProfile: (id: string, data: Partial<{ name: string; description: string; required_epp: string[] }>) =>
    request<Profile>(`/profiles/${id}`, { method: "PUT", body: JSON.stringify(data) }),
  deleteProfile: (id: string) => request(`/profiles/${id}`, { method: "DELETE" }),

  aiSettings: () => request<AISettings>("/ai-settings"),
  updateAiSettings: (data: Partial<AISettingsUpdate>) =>
    request<AISettings>("/ai-settings", { method: "PUT", body: JSON.stringify(data) }),

  dashboardStreams: () => request<StreamDashboardItem[]>("/analyses/dashboard/streams"),

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
  location: string | null;
  api_token: string;
  last_seen_at: string | null;
  online: boolean;
  stream_count: number;
};

export type StreamConnectionConfig = {
  device?: string;
  url?: string;
};

export type StreamInput = {
  name: string;
  source_type: "usb" | "rtsp";
  connection_config: StreamConnectionConfig;
  profile_id: string;
  enabled: boolean;
};

export type VideoStream = {
  id: string;
  device_id: string;
  name: string;
  enabled: boolean;
  source_type: "usb" | "rtsp";
  connection_config: StreamConnectionConfig;
  profile_id: string;
  profile_name: string | null;
  required_epp: string[];
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

export type DeviceConfig = {
  device_id: string;
  schedule: Schedule;
  image_settings: ImageSettings;
  config_version: string;
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
  stream_id: string | null;
  stream_name: string | null;
  image_url: string;
  captured_at: string;
  analyzed_at: string;
  provider: string;
  cumple_normativa: boolean;
  observaciones: string | null;
  epp_results: Record<string, boolean>;
  required_epp: string[];
  status: string;
};

export type StreamDashboardItem = {
  stream_id: string;
  stream_name: string;
  device_id: string;
  device_name: string;
  device_location: string | null;
  device_online: boolean;
  profile_id: string;
  profile_name: string;
  required_epp: string[];
  latest_analysis: Analysis | null;
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

export function eppLabel(id: string): string {
  return EPP_TYPES.find((e) => e.id === id)?.label ?? id.replace(/_/g, " ");
}
