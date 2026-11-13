from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from app.db import get_db
from app.models import GuardianLink, Student, User
from app.security import current_user

router = APIRouter(prefix="/api/students", tags=["students"])


@router.get("/{student_id}/attendance")
def attendance(
    student_id: int,
    user: User = Depends(current_user),
    db: Session = Depends(get_db),
):
    """Attendance list. Empty until Level 7; the role and ownership checks run now."""
    student = db.get(Student, student_id)
    if student is None:
        raise HTTPException(404, "no such student")

    if user.role == "student":
        own = db.query(Student).filter(Student.id == student_id, Student.user_id == user.id).one_or_none()
        if own is None:
            raise HTTPException(403, "not allowed")
    elif user.role == "guardian":
        link = (
            db.query(GuardianLink)
            .filter(GuardianLink.guardian_user_id == user.id, GuardianLink.student_id == student_id)
            .one_or_none()
        )
        if link is None:
            raise HTTPException(403, "not allowed")

    return {"student_id": student_id, "attendance": []}
