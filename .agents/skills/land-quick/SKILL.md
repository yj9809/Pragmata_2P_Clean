---
name: land-quick
description: "[주의: build-fix 없이 PR을 즉시 병합] Pragmata_2P 변경을 commit, push, develop 동기화, ready PR 생성, check 확인, 즉시 merge, branch 정렬까지 빠르게 완료. 사용자가 quick merge, land quick, 빠른 병합처럼 build-fix 생략을 명시하면 변경 범위 판단은 사용자 승인으로 간주하고 build-fix를 실행하지 않는다."
---

# Land Quick

현재 named branch 변경을 `develop`에 PR로 빠르게 통합한다. 이 스킬은 `land`와 같은 Git/PR 안전장치를 사용하지만, `build-fix`를 실행하지 않는다.

> 주의: 모든 gate를 통과하면 별도 확인 없이 PR을 즉시 merge한다. 사용자가 현재 요청에서 빠른 병합과 즉시 merge까지 명시한 경우에만 계속한다.

## Build-Skip Policy

이 스킬은 `build-fix`를 실행하지 않는다. 사용자가 `$land-quick`, `land quick`, `quick merge`, `빠른 병합`처럼 이 스킬을 명시적으로 호출하면 변경 범위와 build 생략 여부를 사용자가 판단한 것으로 간주한다.

에이전트는 변경 범위를 확인하고 리스크를 기록하지만, 다음 이유만으로 중단하지 않는다.

- `Source/**`, `Config/**`, `Content/**` 변경 포함
- `.uasset`, `.umap`, runtime asset, cook/package 영향 가능 변경 포함
- Blueprint-facing API, gameplay, networking, UI runtime behavior 변경 포함

대신 PR body와 최종 보고에 build-fix 생략 사실, 포함된 고위험 변경 범위, 수행한 대체 검증을 명시한다. 사용자가 명시적으로 `land-quick`을 취소하거나 일반 `land`로 전환하라고 말한 경우에만 build gate가 필요한 workflow로 전환한다.

## Workflow

1. `AGENTS.md`와 적용되는 directive를 읽는다. 최소한 다음 파일을 다시 읽는다.
   - `.agents/directives/01-behavior.md`
   - `.agents/directives/03-workflow.md`
   - `.agents/directives/10-session-coordination.md`
2. `git status --short --branch`, active session manifest, `git remote -v`를 확인한다.
   - detached HEAD이면 중단한다.
   - 현재 branch가 `main`, `develop`, `snapshot/*`이면 중단하고 사용자 확인을 받는다.
   - `origin` URL이 의도한 repository인지, original branch upstream이 예상한 remote branch인지 확인한다.
   - unrelated dirty file이나 다른 active session과 겹치는 파일을 stage하지 않는다.
   - PR에 포함할 intended change와 제외할 pre-existing dirty path를 각각 기록한다.
3. `git diff --name-status`와 `git diff --cached --name-status`로 변경 범위를 확인하고 build 생략 리스크를 기록한다.
   - staged change가 있으면 intended scope인지 확인한다.
   - 사용자 요청에 포함되지 않은 dirty path는 제외 대상으로 기록한다.
   - `Source/**`, `Config/**`, `Content/**`, `.uasset`, `.umap` 등 build/cook/package 영향 가능 path가 있으면 중단하지 말고 PR body와 최종 보고에 기록한다.
4. `.agents/skills/ship/SKILL.md`를 전부 읽고 `$ship` workflow를 수행한다.
   - project directive가 nested skill과 충돌하면 project directive를 우선한다. 특히 tooling은 `build`, intentional removal은 `delete` commit type을 사용한다.
   - 최근 human commit style을 확인한다.
   - 서로 다른 관심사는 hunk 단위까지 분리해 별도 commit으로 만든다.
   - 명시적인 path만 stage하고 generated/ignored/session scratch file을 제외한다.
   - 모든 commit message에서 `Co-Authored-By`가 없는지 확인한다.
   - commit 후 intended change가 모두 commit됐는지 확인한다.
   - pre-existing excluded dirty path 외의 변경이 남으면 sync하지 말고 중단한다.
5. `git remote -v`로 원격을 재확인하고 original named branch를 먼저 push한다.
   - push 전에 `git fetch origin develop <original>`을 수행한다.
   - `git log origin/develop..<original>`과 `git diff --name-status origin/develop...<original>`으로 게시할 모든 commit/path가 intended scope인지 확인한다.
   - 예상하지 않은 선행 commit이나 path가 있으면 push하지 않고 사용자에게 범위를 보고한다.
6. `.agents/skills/sync/SKILL.md`를 전부 읽고 original branch를 최신 `develop`과 동기화한다.
   - dirty change가 있을 때만 이 실행에서 새 stash를 만든다.
   - `develop`에서 `git pull --ff-only origin develop`을 수행한다.
   - original branch로 돌아와 `develop`을 merge하고 충돌이 있으면 아래 `Merge Conflict Policy`를 즉시 적용한다.
   - merge로 original branch HEAD가 바뀐 경우에만 original branch를 다시 push한다.
   - 이 실행에서 만든 stash만 pop한다.
7. sync 후 `git diff --name-status origin/develop...HEAD`로 PR 최종 범위를 다시 확인한다.
   - build 생략 리스크 기록을 최신 diff 기준으로 갱신한다.
   - sync merge로 예상 밖 path가 PR diff에 들어오면 PR 생성 전에 중단한다.
8. GitHub CLI 상태를 확인한다.
   - `gh --version`과 `gh auth status`가 실패하면 PR 작업을 중단하고 인증 방법을 안내한다.
   - `gh pr list --head <original> --base develop --state open`으로 기존 open PR을 확인한다.
9. `original branch -> develop` ready PR을 생성하거나 기존 PR을 사용한다.
   - 최초 push 전 확인한 commit/path 범위를 다시 검증해 PR 전체 diff가 intended scope인지 확인한다.
   - 제목은 전체 diff의 주된 의도를 반영한다.
   - body에는 변경 내용, 원인/이유, 사용자 영향, build 생략 사유, 수행한 대체 검증을 기록한다.
   - body용 임시 파일은 `.agents/work/` 아래에 만들고 완료 후 제거한다.
   - 기존 PR이 draft이고 사용자가 merge까지 요청했다면 내용과 check를 확인한 뒤 ready로 전환한다.
   - named branch를 삭제하지 않는다.
10. PR merge 전 `mergeable`, `mergeStateStatus`, required check를 확인한다.
    - check가 pending이면 15초 간격, 최대 10분까지만 poll한다.
    - timeout이면 merge하지 않고 pending check를 보고한다.
    - check 실패, conflict, review block이 있으면 merge하지 않고 정확한 blocker를 보고한다.
    - 기록한 `develop` base SHA와 original head SHA를 PR의 현재 base/head SHA와 비교한다.
    - base 또는 head가 바뀌었으면 sync와 최종 diff 재검증을 반복하고 check를 다시 확인한다.
    - 반복 sync에서 conflict가 발생해도 동일한 `Merge Conflict Policy`를 적용한다.
    - ready이고 merge 가능하면 merge commit 방식으로 PR을 merge한다.
11. PR merge commit으로 local/remote branch를 정렬한다.
    - local `develop`을 `origin/develop`에 fast-forward한다.
    - original branch를 `develop`에 fast-forward하고 original branch를 push한다.
    - `HEAD`, `origin/develop`, `origin/<original>`이 같은 merge commit인지 확인한다.
12. 최종 결과를 보고한다.
    - 분리한 commit hash와 제목
    - push와 sync 결과 및 stash 생성/pop 여부
    - PR 번호, URL, check, merge commit
    - 최종 branch/remote hash 일치와 working tree 상태
    - build-fix를 의도적으로 생략한 사유와 대체 검증
    - host app이 요구하면 성공한 stage/commit/push/PR metadata directive

## Merge Conflict Policy

기본 동작은 **즉시 중단 후 사용자 보고**다. conflict를 직접 해석하거나 임의로 고치지 않는다.

자동 해결은 다음 조건을 모두 만족하는 경우에만 허용한다.

- 모든 conflicted path가 UTF-8 등 Git이 text로 처리하는 일반 text file
- 각 conflict hunk가 서로 독립적인 추가 내용이며 literal `Accept Both`로 양쪽 block을 그대로 보존 가능
- marker 제거 외에 값 선택, 재작성, 이동, 정렬, 중복 제거, 이름 변경이 불필요
- 양쪽 block을 순서대로 보존해도 동일 identifier, 함수, key, include, list entry가 충돌하지 않음

다음 중 하나라도 해당하면 자동 해결하지 않고 즉시 중단한다.

- `.uasset`, `.umap` 또는 다른 binary file
- delete/modify, rename/rename, rename/modify, file mode conflict
- 같은 scalar 값, property, JSON/YAML key, 함수 body, 선언 또는 조건 분기를 양쪽이 다르게 수정
- Accept Both 후 compile 의미, 실행 순서, ownership, networking, asset reference 판단이 필요
- conflict marker 제거 외의 편집이 필요하거나 안전성을 확신할 수 없음

허용된 Accept Both 처리에서는 양쪽 block을 원래 순서로 그대로 남기고 conflict marker만 제거한다. 해당 path를 stage하기 전에 `git diff --check`와 중복 identifier/key를 확인한다.

중단 시 merge를 임의로 abort하거나 다른 strategy로 재시도하지 않는다. `git status --short`, `git diff --name-only --diff-filter=U`, conflict 종류와 각 hunk가 Accept Both 대상이 아닌 이유를 보고하고 사용자의 해결을 기다린다. conflict 상태에서는 push, stash pop, PR 생성·merge를 진행하지 않는다.

## Stop Conditions

- active session 또는 unrelated dirty change와 stage 범위 충돌
- Accept Both 조건을 만족하지 않는 sync merge conflict 또는 stash pop conflict
- remote/`gh` 인증 부재
- PR check 실패, merge conflict, review/branch protection block
- 사용자가 PR merge까지 명시하지 않은 경우: PR 생성 후 merge 전에 중단

`build-fix`, `git reset --hard`, direct `develop` merge/push, force-push, branch 삭제, 기존 stash pop을 사용하지 않는다.
