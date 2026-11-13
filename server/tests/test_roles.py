from fastapi import APIRouter, Depends

from app.main import app
from app.security import require_role
from tests.conftest import JUDGE_PASSWORDS, login


def test_student_s2_cannot_open_s1_attendance_403_no_data(api, users):
    s1_id, s2_id = users
    login(api, "s2", JUDGE_PASSWORDS["s2"])

    r = api.get(f"/api/students/{s1_id}/attendance")
    assert r.status_code == 403
    assert "attendance" not in r.text  # no data leaks

    own = api.get(f"/api/students/{s2_id}/attendance")
    assert own.status_code == 200
    assert own.json()["attendance"] == []  # empty until Level 7


def test_guardian_cannot_open_unlinked_child_403(api, users):
    s1_id, s2_id = users
    login(api, "guardian", JUDGE_PASSWORDS["guardian"])

    assert api.get(f"/api/students/{s1_id}/attendance").status_code == 200  # linked child
    assert api.get(f"/api/students/{s2_id}/attendance").status_code == 403


def test_teacher_only_page_refused_for_student(api, users):
    router = APIRouter()

    @router.get("/api/teacher-only-test")
    def teacher_only(user=Depends(require_role("teacher", "admin"))):
        return {"ok": True}

    app.include_router(router)
    try:
        login(api, "s2", JUDGE_PASSWORDS["s2"])
        assert api.get("/api/teacher-only-test").status_code == 403
        login(api, "teacher", JUDGE_PASSWORDS["teacher"])
        assert api.get("/api/teacher-only-test").status_code == 200
    finally:
        app.router.routes = [
            route for route in app.router.routes if getattr(route, "path", None) != "/api/teacher-only-test"
        ]


def test_students_list_scoped_by_role(api, users):
    s1_id, s2_id = users
    login(api, "s1", JUDGE_PASSWORDS["s1"])
    assert [s["id"] for s in api.get("/api/students").json()] == [s1_id]

    login(api, "guardian", JUDGE_PASSWORDS["guardian"])
    assert [s["id"] for s in api.get("/api/students").json()] == [s1_id]

    login(api, "admin", JUDGE_PASSWORDS["admin"])
    assert sorted(s["id"] for s in api.get("/api/students").json()) == sorted([s1_id, s2_id])

    names = {s["name"] for s in api.get("/api/students").json()}
    assert names == {"Student 1", "Student 2"}


def test_students_list_needs_login(api, users):
    api.cookies.clear()
    assert api.get("/api/students").status_code == 401
