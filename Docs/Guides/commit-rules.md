# 커밋 메시지 규칙

이 저장소의 커밋 메시지 작성 가이드.

AI 실행 지침: `AGENTS.md`

## 기본 원칙

- 커밋 단위: 하나의 명확한 관심사
- 메시지 언어: 한국어
- 제목 형식: Conventional Commits
- 공동 작성자 표기: AI, 봇, 에이전트 trailer 금지
- 사람용 문서: 간결한 한국어

## 제목 형식

기본 형식:

```text
<type>: <한국어 설명>
```

예시:

```text
docs: README에 프로젝트 목적 정리
build: Unreal 생성 파일 제외 규칙 정리
feat: Hugh 이동 입력 처리 추가
delete: 사용하지 않는 테스트 에셋 제거
```

### 제목 작성 규칙

- 한국어 사용
- 파일명, 클래스명, Unreal Engine 용어, 기술 식별자는 영어 유지 가능
- `추가`, `수정`, `정리`, `보강`, `반영`, `제거`, `갱신`, `적용` 같은 간결한 표현 사용
- `수정했습니다`, `추가했음`, `작업 중` 같은 문장형, 과거형, 진행형 회피
- 제목 끝 문장 부호 금지

좋은 예:

```text
docs: README에 프로젝트 목적 추가
build: Content 빈 폴더 placeholder 추가
feat: Hugh 이동 입력 처리 추가
delete: 미사용 Blueprint 에셋 제거
```

피할 예:

```text
docs: README를 수정했습니다.
feat: adding movement stuff
build: update files!!!
```

## 커밋 타입

| 타입 | 용도 |
| --- | --- |
| `feat` | 새로운 게임플레이 기능, 시스템, 메커니즘, AI 동작, UI 기능 추가 |
| `fix` | 버그 수정 또는 회귀 수정 |
| `docs` | 문서만 변경 |
| `style` | 동작에 영향 없는 포맷팅 변경 |
| `refactor` | 기능 추가나 버그 수정이 아닌 코드 구조 변경 |
| `perf` | 성능 개선 |
| `test` | 테스트만 추가 또는 수정 |
| `build` | 빌드 설정, 프로젝트 설정, 도구, `.gitignore`, placeholder 등 유지보수 작업 |
| `delete` | 사용하지 않는 코드, 에셋, 문서, 설정, placeholder 등을 의도적으로 제거 |
| `revert` | 이전 커밋 되돌리기 |

### Unreal 프로젝트 기준 예시

- `feat`: 이동 입력, 2인 협동, 해킹 퍼즐, 전투 시스템, 카메라 상태 처리 추가
- `fix`: 공격 판정 오류, 카메라 문제, 입력 바인딩 문제, collision 설정 오류 수정
- `docs`: `README.md`, `Docs/` 문서 변경
- `build`: `.gitignore`, `.uproject`, IDE 설정, `.gitkeep` placeholder 변경
- `refactor`: 동작 변화 없는 `Source/` 클래스 구조 정리
- `delete`: 사용하지 않는 C++ 클래스, Blueprint, map, material, texture, placeholder, 문서 제거

삭제 타입 선택:

- 버그를 직접 고치는 삭제: `fix`
- 구조 변경에 포함된 삭제: `refactor`
- 삭제 자체가 주목적인 변경: `delete`

## 본문 형식

작은 단일 파일 문서 변경은 제목만으로 충분.

여러 파일 변경 또는 설명이 필요한 변경은 파일 기준 bullet 사용.

```text
<type>: <한국어 설명>

- path/to/file:
  변경 요약 추가
- path/to/other-file, path/to/related-file:
  관련 처리 보강
```

예시:

```text
docs: 프로젝트 문서 구조 정리

- README.md:
  학습 목적과 엔진 버전 설명 추가
- Docs/Guides/commit-rules.md, AGENTS.md:
  커밋 메시지 규칙과 에이전트 지침 정리
```

본문 작성 규칙:

- 파일 또는 경로 기준 bullet
- 같은 관심사 파일은 하나의 bullet로 묶기 가능
- `추가`, `정리`, `보강`, `반영`, `제거`, `갱신`, `적용` 같은 짧은 표현 사용
- 긴 배경 설명과 문단형 서술 회피
- 일반 커밋의 `Constraint:`, `Rejected:`, `Tested:` trailer 금지

## 커밋 분리 기준

서로 무관한 변경은 별도 커밋.

권장 분리 예시:

- `.gitignore` 변경 -> `build` 커밋
- `README.md` 변경 -> `docs` 커밋
- `Docs/Architecture/architecture.md` 또는 `Docs/` 구현 기록 변경 -> `docs` 커밋
- `AGENTS.md` 또는 도구 지침 변경 -> `build` 또는 `docs` 커밋
- `Source/` 게임플레이 구현 -> `feat`, `fix`, 또는 `refactor` 커밋
- 미사용 `Source/`, `Content/`, `Docs/` 파일 제거 -> `delete` 커밋
- `Content/`의 `.gitkeep` placeholder 추가 -> `build` 커밋

여러 독립 변경이 동시에 있을 때는 각 변경에 필요한 파일만 stage 후 순서대로 커밋.

구현 변경과 같은 관심사의 `Docs/` 설명 문서는 같은 커밋에 포함 가능. 관련 없는 문서 정리는 별도 `docs` 커밋.

## 커밋 전 확인

상태 확인:

```bash
git status --short
```

- 의도한 파일만 staged 상태인지 확인
- 생성 파일, 캐시, IDE 설정 제외
- `.agents/work/active-sessions/` manifest 제외
- 다른 세션이 같은 파일을 편집 중이면 커밋 중단
- 필요한 경우 최근 커밋 스타일 확인

```bash
git log -2 --format=full
```

기존 커밋 스타일이 없으면 이 문서의 기본 형식 사용.

## AI 에이전트용 세부 지침

AI가 커밋 생성 또는 메시지 재작성 시 함께 확인할 지침:

- `.agents/directives/03-workflow.md`
- `.agents/directives/10-session-coordination.md`
- `.agents/directives/11-human-documentation-style.md`
