---
name: land
description: "[주의: 검증 통과 시 PR을 즉시 병합] Pragmata_2P 변경을 build-fix로 검증한 뒤 commit, push, develop 동기화, ready PR 생성, check 확인, 즉시 merge, branch 정렬까지 완료. 사용자가 PR 생성뿐 아니라 즉시 병합까지 명시적으로 요청할 때만 사용. Merge conflict는 명백한 text Accept Both만 자동 처리하고 그 외에는 즉시 중단·보고. PR 생성·검토만 요청한 경우 사용 금지."
---

# Land

현재 named branch 변경을 `develop`에 PR로 안전하게 통합한다. `build-fix`를 첫 번째 하위 스킬로 실행하고, build가 실패하면 Git publication을 시작하지 않는다.

> 주의: 모든 gate를 통과하면 별도 확인 없이 PR을 즉시 merge한다. 사용자가 현재 요청에서 즉시 merge까지 명시한 경우에만 계속한다.

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
   - build 전 현재 프로젝트 Unreal Editor가 실행 중이었는지 기록한다.
   - Editor가 실행 중이면 Unreal tooling으로 dirty package를 확인한다. 미저장 package가 있거나 확인할 수 없으면 사용자의 discard 승인이 없는 한 build 전에 중단한다.
3. **첫 번째 하위 스킬로 반드시 `build-fix`를 실행한다.**
   - `.agents/skills/build-fix/SKILL.md`를 전부 읽고 해당 workflow를 수행한다.
   - build가 실패하면 최소 수정과 재검증을 끝낼 때까지 commit, push, sync, PR 작업을 시작하지 않는다.
   - build 성공 로그 경로와 기존 warning을 기록한다.
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
7. **PR에 들어갈 최종 조합을 다시 `build-fix`로 검증한다.**
   - sync가 pre-existing excluded dirty path를 복구했다면 final validation 전 path-scoped stash를 새로 만들고 stash object id를 기록한다.
   - clean worktree가 정확히 original branch HEAD를 나타내는 상태에서 build한다.
   - `.agents/skills/build-fix/SKILL.md` workflow를 두 번째로 실행한다.
   - build가 수정 없이 성공해야 PR 단계로 진행한다.
   - build fix가 tracked file을 수정하면 `$ship`부터 sync와 final build까지 반복해 remote head와 검증한 head를 일치시킨다.
8. GitHub CLI 상태를 확인한다.
   - `gh --version`과 `gh auth status`가 실패하면 PR 작업을 중단하고 인증 방법을 안내한다.
   - `gh pr list --head <original> --base develop --state open`으로 기존 open PR을 확인한다.
9. `original branch -> develop` ready PR을 생성하거나 기존 PR을 사용한다.
   - 최초 push 전 확인한 commit/path 범위를 다시 검증해 PR 전체 diff가 intended scope인지 확인한다.
   - 제목은 전체 diff의 주된 의도를 반영한다.
   - body에는 변경 내용, 원인/이유, 사용자 영향, build와 runtime 검증을 기록한다.
   - body용 임시 파일은 `.agents/work/` 아래에 만들고 완료 후 제거한다.
   - 기존 PR이 draft이고 사용자가 merge까지 요청했다면 내용과 check를 확인한 뒤 ready로 전환한다.
   - named branch를 삭제하지 않는다.
10. PR merge 전 `mergeable`, `mergeStateStatus`, required check를 확인한다.
   - check가 pending이면 15초 간격, 최대 10분까지만 poll한다.
   - timeout이면 merge하지 않고 pending check를 보고한다.
   - check 실패, conflict, review block이 있으면 merge하지 않고 정확한 blocker를 보고한다.
   - final build 직후 기록한 `develop` base SHA와 original head SHA를 PR의 현재 base/head SHA와 비교한다.
   - base 또는 head가 바뀌었으면 sync와 final build를 반복하고 check를 다시 확인한다.
   - 반복 sync에서 conflict가 발생해도 동일한 `Merge Conflict Policy`를 적용한다.
   - ready이고 merge 가능하면 merge commit 방식으로 PR을 merge한다.
11. PR merge commit으로 local/remote branch를 정렬한다.
    - local `develop`을 `origin/develop`에 fast-forward한다.
    - original branch를 `develop`에 fast-forward하고 original branch를 push한다.
    - `HEAD`, `origin/develop`, `origin/<original>`이 같은 merge commit인지 확인한다.
    - final validation용 stash를 만들었다면 기록한 stash object만 original branch에 복구하고, conflict가 없을 때 제거한다.
12. build 전에 현재 프로젝트 Unreal Editor가 실행 중이었다면 original branch에서 다시 실행한다.
    - `UNREEL_MCP_ENABLE_PYTHON=1`을 설정한다.
    - 다른 프로젝트의 Editor process는 종료하거나 재시작하지 않는다.
13. 최종 결과를 보고한다.
    - 분리한 commit hash와 제목
    - push와 sync 결과 및 stash 생성/pop 여부
    - PR 번호, URL, check, merge commit
    - 최종 branch/remote hash 일치와 working tree 상태
    - build 결과와 Editor 재실행 여부
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

허용된 Accept Both 처리에서는 양쪽 block을 원래 순서로 그대로 남기고 conflict marker만 제거한다. 해당 path를 stage하기 전에 `git diff --check`와 중복 identifier/key를 확인한다. 이후 final build가 이 자동 해결 path 때문에 실패하면 `build-fix`로 의미 수정하지 말고 즉시 중단해 사용자에게 보고한다.

중단 시 merge를 임의로 abort하거나 다른 strategy로 재시도하지 않는다. `git status --short`, `git diff --name-only --diff-filter=U`, conflict 종류와 각 hunk가 Accept Both 대상이 아닌 이유를 보고하고 사용자의 해결을 기다린다. conflict 상태에서는 push, stash pop, build, PR 생성·merge, Editor 재실행을 진행하지 않는다.

## Stop Conditions

- build 실패가 해결되지 않음
- active session 또는 unrelated dirty change와 stage 범위 충돌
- Accept Both 조건을 만족하지 않는 sync merge conflict 또는 stash pop conflict
- final validation stash 복구 conflict
- remote/`gh` 인증 부재
- PR check 실패, merge conflict, review/branch protection block
- 사용자가 PR merge까지 명시하지 않은 경우: PR 생성 후 merge 전에 중단

`git reset --hard`, direct `develop` merge/push, force-push, branch 삭제, 기존 stash pop을 사용하지 않는다.

final validation stash를 만든 뒤 workflow가 중단되면 original branch가 conflict 없는 안전한 상태일 때 기록한 stash object만 복구한다. 안전하게 복구할 수 없으면 stash를 보존하고 object id를 보고한다.

`build-fix`가 Editor를 닫은 뒤 workflow가 중단되면, original branch로 안전하게 복귀했고 conflict/partial Git operation이 없을 때만 이전 Editor 실행 상태를 복구한다. conflict나 partial merge 상태에서는 Editor를 열지 않고 blocker를 보고한다.
