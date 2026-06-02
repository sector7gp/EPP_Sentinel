from sqlalchemy.orm import Session

from app.models import OperatorProfile, ProfileEPPRequirement


def ensure_default_profile(db: Session) -> OperatorProfile:
    profile = db.query(OperatorProfile).filter(OperatorProfile.name == "Operario de Planta").first()
    if profile:
        return profile
    profile = OperatorProfile(
        name="Operario de Planta",
        description="Perfil por defecto con EPP básicos de planta",
    )
    db.add(profile)
    db.flush()
    for epp in ("casco_seguridad", "chaleco_reflectivo", "calzado_seguridad", "guantes_seguridad"):
        db.add(ProfileEPPRequirement(profile_id=profile.id, epp_type=epp))
    db.commit()
    db.refresh(profile)
    return profile
