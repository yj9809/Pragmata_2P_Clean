# Diana 전투 idle animation 복구 계획

## 목표

- enemy combat 중에도 Diana가 tense/combat idle 대신 기존 follow idle pose를 유지
- `EDianaOperatorMode::Combat`의 gameplay 입력·스킬 허용 의미는 유지
- forced pair, death, aim/hack, skill body animation은 현재 우선순위와 동작을 보존

## 확인된 원인

`APragmataGameState::SetEnemyCombatActive()`는 enemy combat 진입 시 Diana의 `OperatorMode`를 `Combat`으로 바꾼다. `UDianaAnimInstance::UpdateLayerMode()`는 `bEnemyCombatActive`를 이미 항상 `false`로 두지만, `OperatorMode == Combat`이면 `CombatIndependent`를 선택한다. `ABP_Diana`가 이 레이어의 combat/tense idle을 재생하므로 전투 상태가 animation에 계속 반영된다.

이는 `Docs/KNOW_HOW.md`의 “combat state는 HUD event로 계속 전달하며, Diana AnimInstance의 `bEnemyCombatActive`는 tense idle 전환에 쓰지 않아 combat 중에도 normal idle을 유지” 규칙과 불일치한다.

## 작업 단계

- [x] `Source/Pragmata_2P/Animation/DianaAnimInstance.cpp`의 `UpdateLayerMode()`를 수정해 `EDianaOperatorMode::Combat`이 `CombatIndependent`를 선택하지 않도록 한다. `LayerMode`는 action lock, `HitReactLocked`, `MountedWatch`일 때만 `ForcedPair`가 되고, 그 외에는 `FollowHugh`를 선택한다.
- [x] `UpdateHandIKTargetMode()`와 `UpdateCombatState()`를 함께 검토해 layer 변경 뒤에도 normal idle 손 IK, aim/hack pose, skill body slot의 오른손 IK 차단이 각각 유지되는지 확인한다.
- [ ] `EDianaAnimLayerMode::CombatIndependent`의 Blueprint 노출 사용처를 확인한다. `ABP_Diana`가 더 이상 이 레이어로 진입하지 않아도 되는 경우에만 C++ enum 및 AnimBP 전이를 최소 범위에서 정리한다. 다른 연출 또는 legacy asset이 참조하면 enum은 남기고 runtime 진입만 차단한다.
- [x] `Docs/Intent/Animation/DianaAnimInstance-UpdateIKTargets.md`와 `Docs/KNOW_HOW.md`의 해당 규칙을 현재 코드와 일치하도록 갱신한다.

## 검증

- [ ] PIE 또는 listen-server 2인 실행에서 combat 진입/해제 시 Diana가 normal follow idle을 유지하는지 확인
- [ ] 같은 실행에서 Diana aim 시작·유지·해제와 enemy hack 성공을 확인해 HackingStart/Loop/End 및 IK 차단이 유지되는지 확인
- [ ] Hugh의 hard hit, death, shelter 등 forced pair 진입 시 `ForcedPair` 레이어와 기존 montage가 normal idle보다 우선하는지 확인
- [ ] `ABP_Diana`의 State Machine에서 `LayerMode` 전이와 exposed enum 사용을 확인하고 compile/save
- [ ] C++ 수정 시 프로젝트 Editor target build 및 `git diff --check` 실행

## 비대상

- `APragmataGameState`의 `bEnemyCombatActive` 복제, HUD 이벤트, `OperatorMode::Combat` 기반 입력·정화 스킬 허용 규칙 변경
- Diana asset, montage, PAL 구성 변경. 새 asset reference가 없으므로 cook ownership 변경 없음
