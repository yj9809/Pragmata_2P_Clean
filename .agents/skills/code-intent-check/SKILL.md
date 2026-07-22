---
name: code-intent-check
description: Code intent workflow 점검. Use when asked to verify PLAN-code-intent-visualization.md progress, check missing requirements, validate worktree cleanliness, confirm Unreal non-interference, or audit code-intent generated output.
---

# Code Intent Check

## 절차

1. `AGENTS.md`, `01-behavior.md`, `03-workflow.md`, `10-session-coordination.md`, `11-human-documentation-style.md` 확인.
2. `Docs/Plans/PLAN-code-intent-visualization.md` 읽기.
3. `누락 방지 대조표`, `반려 조건`, `최종 완료 조건`을 실제 파일과 대조.
4. 아래 명령 실행:

```powershell
git status --short
.\Tools\CodeIntent\validate.ps1
```

5. report:
   - 충족
   - 미충족
   - manual 검증 필요
   - 다음 행동

## 금지

- Unreal `Source/`, `Content/`, `Config/`, `.uproject`, `.Build.cs` 수정 금지.
- 재생성용 generated output은 `Saved/CodeIntent/`에 둔다.
- 팀 공유용 핵심 HTML은 `Docs/CodeIntentSite/`에서 Git으로 추적한다.
- Doxygen과 Blueprint scan 보존본은 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`에 둔다.
- `node_modules`, `.venv`, installer, cache를 repo 안에 만들지 말 것.
