from app.services.analysis_service import compute_compliance


def test_compliance_all_present():
    result = {
        "casco_seguridad": True,
        "guantes_seguridad": True,
    }
    assert compute_compliance(result, ["casco_seguridad", "guantes_seguridad"]) is True


def test_compliance_missing_item():
    result = {"casco_seguridad": True, "guantes_seguridad": False}
    assert compute_compliance(result, ["casco_seguridad", "guantes_seguridad"]) is False


def test_compliance_empty_required():
    assert compute_compliance({"casco_seguridad": True}, []) is False
