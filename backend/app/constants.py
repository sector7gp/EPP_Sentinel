from enum import Enum


class EPPType(str, Enum):
    CASCO_SEGURIDAD = "casco_seguridad"
    GAFAS_SEGURIDAD = "gafas_seguridad"
    PROTECCION_AUDITIVA = "proteccion_auditiva"
    GUANTES_SEGURIDAD = "guantes_seguridad"
    CALZADO_SEGURIDAD = "calzado_seguridad"
    ROPA_INDUSTRIAL = "ropa_industrial"
    PROTECCION_RESPIRATORIA = "proteccion_respiratoria"
    CHALECO_REFLECTIVO = "chaleco_reflectivo"


EPP_LABELS: dict[str, str] = {
    EPPType.CASCO_SEGURIDAD: "Casco de seguridad",
    EPPType.GAFAS_SEGURIDAD: "Gafas de seguridad",
    EPPType.PROTECCION_AUDITIVA: "Protección auditiva",
    EPPType.GUANTES_SEGURIDAD: "Guantes de seguridad",
    EPPType.CALZADO_SEGURIDAD: "Calzado de seguridad",
    EPPType.ROPA_INDUSTRIAL: "Ropa industrial",
    EPPType.PROTECCION_RESPIRATORIA: "Protección respiratoria",
    EPPType.CHALECO_REFLECTIVO: "Chaleco reflectivo",
}

ALL_EPP_TYPES = [e.value for e in EPPType]


class AIProviderName(str, Enum):
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    GEMINI = "gemini"


class AuditEventType(str, Enum):
    CAPTURE_RECEIVED = "capture_received"
    CAPTURE_FAILED = "capture_failed"
    UPLOAD_SUCCESS = "upload_success"
    CONNECTION_ERROR = "connection_error"
    AI_RESPONSE = "ai_response"
    AI_ERROR = "ai_error"
    CONFIG_CHANGE = "config_change"
