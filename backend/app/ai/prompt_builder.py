import json

from app.constants import EPP_LABELS


class PromptBuilder:
    @staticmethod
    def build(required_epp: list[str], provider: str) -> str:
        labels = [f"- {EPP_LABELS.get(e, e)} (`{e}`): indicar true si visible y correctamente usado, false si ausente o incorrecto" for e in required_epp]
        required_block = "\n".join(labels) if labels else "- Evaluar todos los EPP visibles en la imagen"

        schema_fields = {e: "boolean" for e in required_epp}
        for e in (
            "casco_seguridad",
            "gafas_seguridad",
            "proteccion_auditiva",
            "guantes_seguridad",
            "calzado_seguridad",
            "ropa_industrial",
            "proteccion_respiratoria",
            "chaleco_reflectivo",
        ):
            if e not in schema_fields:
                schema_fields[e] = "boolean"
        schema_fields["cumple_normativa"] = "boolean"
        schema_fields["observaciones"] = "string"

        return f"""Eres un inspector de seguridad industrial. Analiza la imagen de un operario y determina el uso de Elementos de Protección Personal (EPP).

EPP obligatorios para este perfil:
{required_block}

Responde ÚNICAMENTE con un objeto JSON válido (sin markdown) con estas claves:
{json.dumps(list(schema_fields.keys()), ensure_ascii=False)}

Reglas:
- Evalúa solo lo que puedas observar con certeza razonable.
- `cumple_normativa` debe ser true solo si TODOS los EPP obligatorios listados arriba están presentes y correctamente usados.
- `observaciones`: breve explicación en español si hay incumplimiento o dudas.

Proveedor destino: {provider}
"""
