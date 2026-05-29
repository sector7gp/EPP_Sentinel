from app.ai.prompt_builder import PromptBuilder


def test_prompt_includes_required_epp():
    required = ["casco_seguridad", "guantes_seguridad"]
    prompt = PromptBuilder.build(required, "openai")
    assert "casco_seguridad" in prompt
    assert "guantes_seguridad" in prompt
    assert "Casco de seguridad" in prompt


def test_prompt_json_instruction():
    prompt = PromptBuilder.build(["chaleco_reflectivo"], "gemini")
    assert "JSON" in prompt
    assert "chaleco_reflectivo" in prompt
