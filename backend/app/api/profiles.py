from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from app.constants import ALL_EPP_TYPES, AuditEventType
from app.database import get_db
from app.models import OperatorProfile, ProfileEPPRequirement
from app.schemas.schemas import ProfileCreate, ProfileResponse, ProfileUpdate
from app.services.audit import log_event
from app.utils.auth import get_current_admin

router = APIRouter(prefix="/profiles", tags=["profiles"])


def _to_response(profile: OperatorProfile) -> ProfileResponse:
    return ProfileResponse(
        id=profile.id,
        name=profile.name,
        description=profile.description,
        required_epp=[r.epp_type for r in profile.epp_requirements],
    )


@router.get("", response_model=list[ProfileResponse])
def list_profiles(db: Session = Depends(get_db), _: str = Depends(get_current_admin)):
    profiles = db.query(OperatorProfile).all()
    return [_to_response(p) for p in profiles]


@router.post("", response_model=ProfileResponse)
def create_profile(
    body: ProfileCreate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    for epp in body.required_epp:
        if epp not in ALL_EPP_TYPES:
            raise HTTPException(status_code=400, detail=f"EPP inválido: {epp}")
    profile = OperatorProfile(name=body.name, description=body.description)
    db.add(profile)
    db.flush()
    for epp in body.required_epp:
        db.add(ProfileEPPRequirement(profile_id=profile.id, epp_type=epp))
    db.commit()
    db.refresh(profile)
    log_event(db, AuditEventType.CONFIG_CHANGE.value, f"Perfil creado: {profile.name}", user=admin)
    return _to_response(profile)


@router.put("/{profile_id}", response_model=ProfileResponse)
def update_profile(
    profile_id: str,
    body: ProfileUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    profile = db.get(OperatorProfile, profile_id)
    if not profile:
        raise HTTPException(status_code=404, detail="Perfil no encontrado")
    if body.name is not None:
        profile.name = body.name
    if body.description is not None:
        profile.description = body.description
    if body.required_epp is not None:
        for epp in body.required_epp:
            if epp not in ALL_EPP_TYPES:
                raise HTTPException(status_code=400, detail=f"EPP inválido: {epp}")
        db.query(ProfileEPPRequirement).filter_by(profile_id=profile_id).delete()
        for epp in body.required_epp:
            db.add(ProfileEPPRequirement(profile_id=profile_id, epp_type=epp))
    db.commit()
    db.refresh(profile)
    log_event(db, AuditEventType.CONFIG_CHANGE.value, f"Perfil actualizado: {profile.name}", user=admin)
    return _to_response(profile)


@router.delete("/{profile_id}")
def delete_profile(
    profile_id: str,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    profile = db.get(OperatorProfile, profile_id)
    if not profile:
        raise HTTPException(status_code=404, detail="Perfil no encontrado")
    db.delete(profile)
    db.commit()
    log_event(db, AuditEventType.CONFIG_CHANGE.value, f"Perfil eliminado: {profile_id}", user=admin)
    return {"status": "ok"}
