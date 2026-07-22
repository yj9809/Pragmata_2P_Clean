# 세션 기반 의도 복원 색인

이 문서는 local Codex session, repo `.omx` context, `.agents/work` 기록에서 복원한 개발 의도 색인이다.
각 항목의 세부 의도는 가능한 경우 symbol별 card로 분리했다.

## 확인한 자료

- Codex Desktop rollout: `session_meta.cwd`가 `Pragmata_2P`인 rollout 133개
- Codex session index: thread title과 update time
- `.omx/context`: Ralph/Autopilot context snapshot
- `.omx/artifacts`: ABP_Diana node scan, fix-hugh-look-input review/plan artifact
- `.agents/work/active-sessions`: 과거 task intent, touched path, notes
- 기존 tracked docs: `Docs/KNOW_HOW.md`, `Docs/Architecture/`, `Docs/Systems/`, `Docs/Create/`

## 복원 원칙

- 개인 session 원문은 복붙하지 않음
- `Docs/Intent/**/*.md`를 사람이 검토할 수 있는 의도 원본으로 사용
- 현재 코드와 기존 Docs로 확인되는 내용만 `확실한 근거`로 기록
- session 기억만 있는 내용은 `추정한 의도` 또는 `아직 모르는 점`으로 분리
- 이미 충분한 card가 있는 항목은 이 색인에서 기존 card로 연결

## 새로 분리한 card

- `Animation/DianaAnimInstance-UpdateIKTargets.md`
- `Animation/PairedAnimationComponent-ForcedDianaInterrupt.md`
- `Enemy/MonsterBase-ResolveDeathAnimation.md`
- `GameFlow/PragmataSessionUtils.md`
- `GameFlow/PragmataTitlePlayerController-RequestStartFromTitle.md`
- `HUD/PragmataHUDWidget-ResolveWeaponWheelRoot.md`
- `Input/HughLookInput-IA_Look.md`
- `Input/PragmataPlayerController-InputDeviceState.md`
- `Lighting/HughCharacter-HeadVisorSpotlight.md`
- `Movement/PragmataHughMovementComponent-SavedMoveIntent.md`
- `Stats/PragmataPlayerStatsComponent.md`
- `Weapon/GripGun-PullTrigger.md`

## 세션에서 복원한 주요 의도

| 기간 | 근거 | 복원한 의도 | 문서화 상태 |
| --- | --- | --- | --- |
| 2026-06-08 | `Design CharacterBase hierarchy`, `.agents/work/codex-260608-vertical-slice-architecture.md` | Hugh는 실제 조작 body, Diana는 별도 operator pawn, attached Diana mesh는 표현 avatar로 분리 | `Docs/KNOW_HOW.md`, 기존 HUD/Hacking card |
| 2026-06-09 | `Update Diana socket bindings`, paired montage session notes | paired animation은 Hugh/Diana ID pair를 DataAsset로 묶고, 손 IK target socket은 mount pose와 side-handle pose를 분리 | `Animation/DianaAnimInstance-UpdateIKTargets.md` |
| 2026-06-09 | `.omx/context/fix-hugh-look-input-20260609T125404Z.md` | `IA_Look`은 mouse 2D 입력을 `FVector2D`로 전달해야 하므로 Boolean이 아니라 Axis2D | `Input/HughLookInput-IA_Look.md` |
| 2026-06-11~23 | ABP_Diana IK guide/session notes, `.omx/context/abp-diana-cpp-port-20260623T094500Z.md` | socket transform 계산은 C++로 옮기고, AnimGraph skeletal-control node는 Blueprint에 남김 | `Animation/DianaAnimInstance-UpdateIKTargets.md` |
| 2026-06-23 | `Fix attachment template mismatch` | WaistArm/Hugh mesh socket이 없을 때 잘못된 root fallback으로 테스트를 속이지 않게 fallback과 로그를 분리 | 기존 `Movement/WaistArmComponent-*` card |
| 2026-06-27~28 | missing comments/docs Ralph context | AI가 만든 코드라도 왜 이 경계가 필요한지 source comment와 tracked Docs에 남겨야 함 | `Docs/Intent` 전체 원칙 |
| 2026-06-28~29 | `.omx/context/diana-node-interaction-*`, `Add node interaction debug flow` | Diana node interaction은 Roaming/Aim에서 server-authoritative로 node를 완료하고 기존 prop `Interact_Implementation()` 흐름을 재사용 | 기존 `Hacking/DianaNodeInteractionComponent-*` card |
| 2026-06-29 | `Fix BP_Light_Demo alwaysOn` | 세션에는 `BP_Light_Demo`가 나오지만 현재 `Content/`에서는 확인되지 않음. 현재 코드 기준 의도는 light actor가 hardcoded reference 대신 `APragmataGameState::OnPowerStateChanged`를 구독하는 구조 | `Docs/KNOW_HOW.md` |
| 2026-06-29 | `Attach spotlight to HeadVisor socket`, runtime yaw fix session | Hugh visor light는 `HeadVisor_LightSocket`에 붙고, socket 누락은 fallback attach와 warning으로 드러내며, on/off는 replicated bool로 공유 | `Lighting/HughCharacter-HeadVisorSpotlight.md` |
| 2026-06-29 | `HUD responsibility plan` | Hugh/Diana별 root HUD는 다르지만 공유 stat source는 GameState role pointer로 통일 | 기존 HUD card, `Stats/PragmataPlayerStatsComponent.md` |
| 2026-06-30 | `Show ApplyDamage example`, damage review, `.omx/context/damage-flow-hit-window-*` | Unreal `TakeDamage` 대신 `FPragmataDamageSpec` 기반 `ReceiveDamage`로 gameplay damage context를 보존 | 기존 Damage card |
| 2026-06-30~07-01 | enemy hack sessions, `.omx/context/enemy-hack-aim-ui-*`, `.omx/context/combat-hack-*` | enemy hack은 `IA_Hack` 버튼이 아니라 aim 상태와 active target을 기준으로 자동 시작하고, 방향키는 열린 hack UI에만 전달 | 기존 Hacking/HUD card |
| 2026-07-01 | shared Hugh stat session | HP, stamina, repair cartridge, OverDrive처럼 양쪽 플레이어가 봐야 하는 값은 local pawn이 아니라 replicated shared source에서 bind | `Stats/PragmataPlayerStatsComponent.md` |
| 2026-07-01 | Diana hacking animation sessions | Diana combat animation은 aim/hack success serial을 C++ state로 노출하고, hacking pose에서는 오른손 IK가 animation을 덮지 않게 차단 | `Animation/DianaAnimInstance-UpdateIKTargets.md` |
| 2026-06-22~30 | Diana ABP/ForcedPair session notes, `codex-260630-forcedpair-comments-and-asset` | hard hit/death/shelter 같은 forced paired cue 중 Diana scan/hack/camera action이 montage를 덮지 않도록 GameState action lock을 둠 | `Animation/PairedAnimationComponent-ForcedDianaInterrupt.md` |
| 2026-07-02 | map transition/session flow sessions | Title -> MatchLobby -> Demo 흐름은 title controller, lobby game mode, session utils로 역할을 나눔 | `GameFlow/PragmataTitlePlayerController-RequestStartFromTitle.md`, `GameFlow/PragmataSessionUtils.md` |
| 2026-07-02 | server guide, packaging, LAN test sessions | first listen lobby open은 `ServerTravel`이 아니라 `OpenLevel(..., listen options)`로 시작하고, gameplay 전환만 lobby server travel | `GameFlow/PragmataSessionUtils.md` |
| 2026-07-02 | `Create local build-fix skill` | Unreal Editor target build는 에디터 잠금 문제를 먼저 제거하고 compile/UHT/link 오류만 최소 수정하는 반복 skill로 처리 | `.agents/skills/build-fix/SKILL.md`, `Docs/KNOW_HOW.md` |
| 2026-07-02~03 | PAL/package sessions | runtime soft load는 PAL explicit asset, map은 `MapsToCook`, generated/package output은 tracked source와 분리 | `Docs/KNOW_HOW.md`, `GameFlow/PragmataGameFlowConfig.md` |
| 2026-06-18~27 | asset unstage, purge, Google Drive bundle sessions | Git이 감당하기 어려운 large asset은 `Docs/Operations/unstaged-assets.md`에 추적 기준을 남기고 `gd-upload` bundle로 별도 보존 | `Docs/Operations/unstaged-assets.md`, `.agents/skills/gen-gd/SKILL.md`, `.agents/skills/unstage/SKILL.md` |
| 2026-07-03 | `gripgun-authority-20260703T095239Z.md` | GripGun ammo/reload/trace/damage는 server-authoritative helper boundary 안에서만 확정 | `Weapon/GripGun-PullTrigger.md` |
| 2026-07-03 | structural ambiguity scan sessions | `WantsToAim`, `WantsToRun`, `WantsToHover` 계열은 gameplay state가 아니라 movement prediction intent이며, state 확정은 `AHughCharacter`가 담당 | `Movement/PragmataHughMovementComponent-SavedMoveIntent.md` |
| 2026-07-03 | WeaponWheel sessions | `WBP_HughHUD` child 이름을 바로 바꾸지 않고 C++ fallback으로 기존 asset 이름을 수용 | `HUD/PragmataHUDWidget-ResolveWeaponWheelRoot.md` |
| 2026-07-03 | monster death/damage sessions | monster death animation은 health mutation 전 `DamageSpec`으로 확정해야 OnDeath 이후에도 같은 animation을 재생 | `Enemy/MonsterBase-ResolveDeathAnimation.md` |
| 2026-07-04 | Diana death attach prompt | death/forced pair 중 Diana 독립 조작과 IK가 연출을 덮지 않게 action lock/forced layer를 우선해야 함. attach 자체의 최신 asset 상태는 재검증 필요 | `Animation/PairedAnimationComponent-ForcedDianaInterrupt.md` |
| 2026-07-04~05 | Code Intent plan sessions | intent 원본은 `Docs/Intent`, generated site는 읽기용 snapshot, heavy Doxygen/Blueprint output은 Git 밖 보존 | `Docs/Guides/code-intent-usage.md`, directive 13 |
| 2026-07-07 | DualSense input prompt state | UI prompt 전환은 local PlayerController boolean으로 처리하고, Windows DualSense 입력 backend는 deprecated RawInput 대신 GameInputWindows를 사용 | `Input/PragmataPlayerController-InputDeviceState.md` |

## 검토 필요 항목

- Diana death animation 중 Hugh attach/IK 해제 흐름: action lock intent는 card로 복원했지만 최신 Blueprint asset의 실제 attach/IK state는 UnreelMCP 재검증 필요
- `BP_Light_Demo`: 과거 세션명과 `.omx` context에는 남아 있지만 현재 `Content/` 파일 목록에 없음. 현재 사실로는 `ALightBase`와 `Content/Map/Props/Lights/BP_Light_*` 계열만 사용
- 일부 오래된 rollout은 session index title이 비어 있음: cwd가 repo로 확인됐지만 title 기반 분류가 약함
- Blueprint asset 안 직접 호출 관계: 새 card의 Blueprint 연결 여부는 tracked scan 또는 UnreelMCP 재검증이 필요
