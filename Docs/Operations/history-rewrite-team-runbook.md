# History Rewrite Team Runbook

## 목적

- `develop` history에서 map prop mesh, texture, animation asset을 제거해 repository size를 최종적으로 줄인다.
- 즉시 모든 remote branch를 정리하지 않는다.
- 팀원은 기존 branch에서 계속 작업하고 push할 수 있다.
- 단, `develop` 병합 전에는 반드시 purged `origin/develop` 위로 rebase하고 같은 remote branch를 `--force-with-lease`로 갱신한다.

## 팀원 규칙

- old-history branch를 `develop`에 merge하지 않는다.
- purged `origin/develop`을 기존 branch에 merge해서 sync하지 않는다.
- `develop` 병합 또는 PR 전에는 rebase를 사용한다.
- rebase 후 remote branch를 force-with-lease로 갱신한다.
- `main` history rewrite 후 개인 작업 branch는 rebase하지 않는다.
- 로컬 `main`만 새 `origin/main`으로 hard reset한다.

## main history rewrite 후 로컬 main 동기화

`origin/main`은 `origin/develop` commit으로 강제 이동될 수 있다.
이 경우 개인 작업 branch는 그대로 둔다.
각 팀원은 자기 컴퓨터의 local `main`만 아래 명령으로 맞춘다.

```powershell
git fetch --prune origin
git switch main
git reset --hard origin/main
git status --short --branch
```

로컬 `main`에 임시 변경이 있으면 먼저 stash한다.

```powershell
git switch main
git stash push -u -m "pre-main-history-rewrite"
git fetch --prune origin
git reset --hard origin/main
git status --short --branch
```

stash 복원은 필요한 파일만 확인해서 적용한다.

```powershell
git stash list
git stash show --stat stash@{0}
git stash apply stash@{0}
```

주의:

- `git reset --hard origin/main`은 local `main`의 미커밋 변경을 삭제한다.
- 개인 작업 branch에서 실행하지 않는다.
- 개인 작업 branch는 평소처럼 `develop` 기준으로 PR 전 rebase한다.

## 병합 전 절차

```powershell
git fetch origin --prune
git rebase origin/develop
powershell -ExecutionPolicy Bypass -File Tools/PurgeBranchCheck.ps1
git push --force-with-lease
```

`PurgeBranchCheck.ps1`가 실패하면 출력된 이유를 먼저 해결한다.

## purge branch 재현 절차

아래 명령은 PowerShell 기준이다. `Jisung` 대신 자기 branch 이름을 넣는다.

```powershell
$SourceBranch = "Jisung"
$PurgeBranch = "purge/Jisung"
```

JunHyun 작업자는 두 값만 바꾼다.

```powershell
$SourceBranch = "JunHyun"
$PurgeBranch = "purge/JunHyun"
```

### 1. 최신 develop에서 시작

```powershell
git fetch origin --prune
git switch develop
git pull --ff-only origin develop
git status --short --branch
```

`git status`는 clean이어야 한다.

### 2. 원본 branch 확인

```powershell
git log --oneline origin/develop..origin/$SourceBranch
git diff --name-status origin/develop..origin/$SourceBranch
```

원본 branch는 수정하지 않는다.

### 3. purge branch 생성

```powershell
git switch -c $PurgeBranch origin/develop
```

이미 같은 이름의 local branch가 있으면 먼저 담당자에게 확인한다.

### 4. 필요한 commit만 반영

```powershell
git cherry-pick <commit>
```

오래된 map asset commit 전체를 그대로 옮기지 않는다. 기능 작업 commit만 선별한다.

여러 commit이면 오래된 것부터 순서대로 적용한다.

```powershell
git cherry-pick <oldest-commit>
git cherry-pick <next-commit>
git cherry-pick <latest-commit>
```

### 5. 충돌 확인

```powershell
git status --short
git diff -- <file>
```

애매한 conflict는 여기서 멈추고 담당자에게 확인한다.

### 6. 충돌 처리 명령

```powershell
git checkout --ours -- <file>
git checkout --theirs -- <file>
git add <file>
git cherry-pick --continue
```

잘못 판단했으면 중단한다.

```powershell
git cherry-pick --abort
```

기준:

- purge asset 재등장: 삭제 유지.
- 독립적인 include, enum, module, input 추가: 양쪽 보존 가능.
- `.uasset`, `.umap`, `.ini`, `.uproject`, 같은 함수 로직 충돌: 자동 처리 금지.

### 7. purge 검증

```powershell
powershell -ExecutionPolicy Bypass -File Tools/PurgeBranchCheck.ps1
```

purge asset은 되살리지 않는다. 실패하면 출력된 경로를 먼저 해결한다.

### 8. 빌드 검증

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Pragmata_2PEditor Win64 Development -Project="$PWD\Pragmata_2P.uproject" -WaitMutex -NoHotReload
```

`Result: Succeeded`가 나와야 한다.

### 9. branch별 검증 기록

```powershell
notepad Docs/Operations/history-rewrite-team-runbook.md
```

`purge/<branch>` 안에 자기 branch의 `해결됨` / `수동 검증 필요`만 적는다. 다른 branch 체크리스트는 넣지 않는다.

### 10. push

```powershell
git status --short --branch
git push -u origin $PurgeBranch
```

push 전에 generated file, build output, purge asset이 staged 되지 않았는지 확인한다.

## purge/minwon 검증 상태

### 해결됨

- `origin/minwon`은 변경하지 않고 `origin/purge/minwon` 별도 branch 생성.
- `purge/minwon` 최초 생성 기준: `origin/develop` `65960c25`.
- develop 병합 전 검증 commit: `origin/purge/minwon` `ffb6d1ce`.
- local `purge/minwon`과 `origin/purge/minwon` 일치.
- worktree clean.
- tracked purge target: `0`.
- Gate SM purge asset 8개 tracked: `0`.
- Gate SM purge asset 8개는 `.gitignore`의 `Content/Map/Props/**/SM/` 규칙으로 ignore.
- minwon 최신 5개 작업 commit의 34개 경로 대조 완료.
- 25개 경로는 `origin/minwon` 최종본과 동일.
- 8개 차이는 의도적으로 purged 된 Gate SM asset.
- `Source/Pragmata_2P/Props/PoweredResponBox.h` 차이는 공백/빈 줄 차이만 존재.
- `PoweredResponBox.h`는 `--ignore-space-at-eol --ignore-blank-lines` 기준 동일.
- Editor 대상 빌드 성공.

### 수동 검증 필요

- `Content/Map/LunarMap.umap`에서 워커 스폰 박스 배치 확인.
- 워커 스포너 동작 확인.
- 임시 전원 공급까지 가는 길 확인.
- 문이 계속 열려 있는 오류 수정 상태 확인.
- 비상구 조명 배치와 표시 확인.
- 비상구 조명 머티리얼과 발광 표시 확인.
- `BP_Floor_29*`, `BP_Floor_49*` 바닥 Blueprint의 표시와 콜리전 확인.
- `PoweredResponBox` 상호작용 동작 확인.
- 제거 대상 Map mesh가 필요한 시각 요소는 Google Drive 복원 asset이 없는 환경에서 누락 참조처럼 보일 수 있음.

## 선택 백업

걱정되는 local 작업이 있으면 rebase 전에 직접 백업한다.

```powershell
git branch backup/pre-purge-work
git stash push -u -m "pre-purge-work"
```

stash를 만든 경우 rebase 후 필요할 때만 적용한다.

```powershell
git stash apply
```

## 최종 repo size

- active remote branch가 old map asset blob을 가리키는 동안 GitHub repository size는 바로 줄지 않을 수 있다.
- 모든 병합 대상 branch가 clean history로 바뀐 뒤 fresh clone size를 확인한다.
- GitHub 표시 size가 늦게 줄면 server-side GC 또는 GitHub Support가 필요할 수 있다.
