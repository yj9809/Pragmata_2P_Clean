# 브랜치 규칙

이 저장소의 branch 운용 가이드.

AI 실행 지침: `.agents/directives/03-workflow.md`

## 기본 브랜치 구조

유지 브랜치:

- `main`: 안정적인 기준 브랜치
- `develop`: 개발 내용이 모이는 통합 브랜치
- (예시) `hojin`: `호진` 이름을 영문으로 표기한 개인/작업 브랜치
- `snapshot/<YYMMDD>-<설명>`: devlog, screenshot, troubleshooting 회고용 저장소 상태 보존 브랜치

초기 구조:

```text
main
└── develop
    └── hojin
```

초기에는 세 브랜치가 같은 커밋을 가리킬 수 있음.

## 원격 저장소

원격 저장소는 문서에서 고정하지 않음.

GitHub 저장소 확정 후 `origin` remote 설정. Push 전 remote 확인:

```bash
git remote -v
```

## 브랜치 생성 방식

초기 설정 순서:

1. `main`에서 초기 프로젝트 커밋 생성
2. `main`에서 `develop` 생성
3. `develop`에서 `hojin` 생성

예시:

```bash
git switch main
git switch -c develop
git switch -c hojin
```

## 작업 방식

일반 작업 위치: `hojin`

- `main`: 안정 기준
- `develop`: 통합 브랜치
- `hojin`: 개인/작업 브랜치
- `snapshot/*`: 일반 개발용 아님. 특정 시점 보존용

기능 개발, 문서 수정, 실험 작업은 `hojin`에서 진행 후 `develop`으로 병합.

## Snapshot 브랜치

용도: devlog, screenshot, troubleshooting 회고용 저장소 상태 보존.

생성 조건: 사용자의 "archive the current state", "현재 상태 archive", "snapshot 생성" 등 명시 요청.

브랜치 이름 형식:

```text
snapshot/<YYMMDD>-<short-kebab-description>
```

예시:

```text
snapshot/260620-server-error
```

규칙:

- 기본 동작: 현재 `HEAD`를 가리키는 브랜치 생성
- Uncommitted work: 사용자 명시 요청 없으면 snapshot에 미포함
- 생성 전 확인: `git status --short --branch`
- 설명: 짧은 kebab-case
- 설명 불명확 시 짧은 label 확인
- 권장 명령: 현재 작업 브랜치를 바꾸지 않는 `git branch snapshot/<name> HEAD`
- 병합 금지: `snapshot/*` -> `main`, `develop`, 개인 작업 브랜치
- 삭제 금지: 사용자 명시 요청 없을 때
- 원격 push: 사용자의 publish 또는 push 요청이 있을 때만

## 동시 세션 조율

여러 AI 세션이 같은 저장소에서 작업 가능.

작업 전 확인:

- `.agents/work/active-sessions/*.md` 확인
- `done`이 아닌 manifest의 편집 예정 파일 확인
- 내 작업 파일과 겹치면 편집 중단
- 충돌 세션, 작업 요약, 경로 확인 후 대기

주의:

- `.agents/work/active-sessions/` manifest는 gitignored scratch 상태
- manifest stage/commit 금지
- 오래된 manifest처럼 보여도 임의 무시 금지
- binary Unreal asset 중복 편집 금지

## 병합 방식

`hojin` 변경사항은 Pull Request로만 `develop`에 반영.

기본 순서:

```bash
git switch hojin
git fetch origin
git merge origin/develop
git push origin hojin
```

이후 GitHub에서 `hojin` -> `develop` Pull Request 생성 후 PR에서 병합.

중요 규칙:

- `develop`에 직접 로컬 merge 후 push 금지
- 모든 Git 병합은 Pull Request를 통해 진행
- PR 병합 후 `hojin` 삭제 금지
- `develop` 반영 후에도 `hojin` 유지
- 명시 요청 없는 `main`, `develop`, `hojin`, `snapshot/*` 삭제 금지

## 초기 push 예시

초기 설정 후 세 브랜치 push:

```bash
git push -u origin main
git push -u origin develop
git push -u origin hojin
```

## 주의사항

- 일반 작업의 `main` 직접 커밋 금지
- 브랜치 삭제 또는 force push 전 의도 확인
- force push 필요 시 `--force` 대신 `--force-with-lease`
- Unreal Engine 생성 파일인 `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` 커밋 금지
- 세션 조율 manifest인 `.agents/work/active-sessions/` 파일 커밋 금지
- 커밋 메시지 기준: [`Docs/Guides/commit-rules.md`](commit-rules.md)
