# LunarMap 튜토리얼 Trigger 복구와 WeaponUse 목표 알림 계획

## 목표

- `LunarMap_LS`의 여섯 tutorial trigger를 실제로 발동하는 목적별 Blueprint로 복구
- `BP_TutorialTrigger_WeaponUse`에서 유효 player pawn이 처음으로 나간 뒤 Hugh·Diana 양쪽에 `발전소 전력을 복구한다`를 10초 표시
- 검증한 trigger 설정을 최종 맵 `LunarMap_T`로 이관

## 확인된 원인

- 여섯 actor가 목적별 child가 아닌 `/Game/Blueprints/Props/Tutorial/BP_TutorialTriggerBase` 인스턴스다.
- PIE log에서 `BP_TutorialTrigger_LockedDoor`는 `TutorialId`가 `None`이라 authority overlap 뒤 즉시 반환한다.
- 여섯 Base 인스턴스는 `TutorialStep`은 연결했지만 `TutorialId`가 모두 `None`이다.
- `ATutorialTriggerActor`는 BeginOverlap만 bind한다. EndOverlap callback과 objective alert 요청이 없다.
- 양쪽 알림 경로는 이미 존재한다. 서버 `APragmataGameState::Multicast_ShowObjectiveAlert()`가 각 peer의 local `APragmataPlayerController`로 전달하고, root HUD의 `WBP_ObjectiveAlertWidget`이 지정 시간 뒤 숨긴다.
- `PragmataTutorialProgress` SaveGame이 PIE를 넘어 유지돼, 같은 tutorial은 다음 PIE에서 조용히 차단된다.

## 동작 결정

- WeaponUse objective alert는 서버가 **첫 유효 player pawn의 종료 overlap**을 확인했을 때 한 번만 발행한다. 두 플레이어가 모두 범위를 나갈 때까지 기다리지 않는다.
- alert는 tutorial popup의 SaveGame 1회 표시 결과와 독립이다. 이미 tutorial을 본 peer라도 WeaponUse trigger의 이번 map-session exit alert는 표시한다.
- 재진입과 두 pawn의 연속 종료 overlap은 actor의 서버 transient gate로 중복 표시하지 않는다.
- `TutorialId`는 SaveGame 영구 키다. 기존 이름을 재사용·변경하지 않는다.
- PIE는 SaveGame과 분리한 transient 이력을 사용하고, standalone/packaged만 영구 이력을 사용한다.
- `UntilReplaced`를 명시 hide 없이 쓰지 않는다. `LunarMap_T`의 여섯 공간 안내는 모두 `Timed 5초`를 사용한다.

## 구현 TODO

### 1. 기존 설정과 역할 확인

- [x] `DA_Tutorial_WeaponsUse`, `DA_Tutorial_MapHacking`, `DA_Tutorial_Overdrive`, `DA_Tutorial_Checkpoint`, `DA_Tutorial_TaintedEnemies`, `DA_Tutorial_LockedDoor`의 `TargetRole`, 제목, 설명, key binding을 readback
- [x] `LunarMap_LS` 여섯 actor의 transform, `TriggerBox` extent, instance override를 기록
- [ ] 각 trigger의 `ActivationRole`을 DataAsset `TargetRole`과 실제 플레이 동선으로 결정. `TargetRole`과 `ActivationRole`을 같은 값으로 가정하지 않음
- [x] 기존 `PragmataTutorialProgress` SaveGame을 보존하고 PIE만 transient 이력으로 분리

### 2. 목적별 trigger Blueprint 복구

- [x] `/Game/Blueprints/Props/Tutorial/`에 `BP_TutorialTrigger_WeaponUse`, `BP_TutorialTrigger_MapHacking`, `BP_TutorialTrigger_Overdrive`, `BP_TutorialTrigger_Checkpoint`, `BP_TutorialTrigger_TaintedHacking`, `BP_TutorialTrigger_LockedDoor` child Blueprint 생성
- [x] 각 child Class Defaults에 해당 `TutorialStep`, 안정적인 `TutorialId`, `ActivationRole`, `DismissPolicy`, `TriggerBox` 기본 extent 설정
- [x] Event Graph에는 overlap/UI 로직을 복제하지 않음. 공통 로직은 `ATutorialTriggerActor`에만 유지
- [x] `LunarMap_LS`의 Base 인스턴스 여섯 개를 목적별 child로 교체하고, 위치·회전·box extent 이관
- [x] `LunarMap_LS`의 여섯 인스턴스에 DataAsset과 고유 `TutorialId`를 명시

### 3. WeaponUse 종료 objective alert 추가

- [x] `ATutorialTriggerActor`에 선택형 Blueprint Class Defaults 설정 추가
  - [x] `bShowObjectiveAlertOnExit`
  - [x] `ExitObjectiveAlertText`
  - [x] `ExitObjectiveAlertDisplaySeconds`
- [x] `BeginPlay()`에서 `TriggerBox::OnComponentEndOverlap`를 중복 없이 bind
- [x] 종료 callback도 BeginOverlap과 동일하게 server authority, player-controlled pawn, `APragmataPlayerController`, `ActivationRole`을 검증
- [x] 서버 transient `bExitObjectiveAlertShown`으로 첫 유효 exit만 통과시키고, `APragmataGameState::Multicast_ShowObjectiveAlert()` 호출
- [x] WeaponUse child와 LS 인스턴스에 `bShowObjectiveAlertOnExit=true`, `ExitObjectiveAlertText=발전소 전력을 복구한다`, `ExitObjectiveAlertDisplaySeconds=10.0` 설정
- [x] 기존 `WBP_ObjectiveAlertWidget`과 root HUD child 이름을 readback. 새 widget, 새 HUD 경로, Blueprint Event Graph UI 생성은 추가하지 않음
- [ ] UI가 늦게 생성되는 travel 직후 경로와 이미 표시 중인 objective의 재표시 시점을 확인. 필요한 경우 기존 `ShowObjective`의 10초 재설정 계약을 유지

### 4. 문서와 cook 소유권 갱신

- [x] `Docs/Intent/Tutorial/TutorialTriggerActor.md`에 EndOverlap objective alert의 authority·1회 gate·tutorial popup과의 책임 분리를 기록
- [x] `Docs/Intent/HUD/ObjectiveAlertWidget.md`에 WeaponUse가 `Multicast_ShowObjectiveAlert()`을 사용하는 공통 알림 사례를 추가
- [x] `Docs/KNOW_HOW.md`의 기존 tutorial/HUD recipe를 현재 동작으로 갱신
- [x] `Docs/Operations/lunarmap-ls-sync-policy.md`에서 이미 제거된 Cleanse sequence/camera 설명을 현재 non-cinematic acquire 흐름으로 정정하고, tutorial trigger 이관 목록을 추가
- [x] 새 soft reference를 만들지 않는지 확인. 새 runtime soft asset이 필요해질 경우에만 `/Game/CookLabels/PAL_CoreRuntime` coverage를 검증·보강

### 5. LunarMap_T 이관

- [x] `LunarMap_LS`의 여섯 trigger를 `LunarMap_T`로 복사
- [x] T의 여섯 actor class, DataAsset, `TutorialId`, transform, box extent readback 비교
- [x] `LunarMap_T` BGM SoundWave dependency 복구
- [x] GameFlow fallback/BP CDO와 `MapsToCook`를 T로 전환하고 `LunarMap_LS` 삭제

## 검증 TODO

- [x] Editor target build로 EndOverlap delegate signature, C++ property, GameState 호출 compile 확인
- [x] 목적별 child Blueprint compile/save/reopen 후 Class Defaults readback
- [ ] 2-player PIE에서 Hugh host/Diana client, Diana host/Hugh client 각각 여섯 trigger의 begin overlap 확인
- [ ] Output Log에 `TutorialStep이 설정되지 않음`, `TutorialId가 설정되지 않음`, HUD child 누락 warning이 없는지 확인
- [ ] WeaponUse에서 한 player가 나갈 때 양쪽 viewport에 정확히 `발전소 전력을 복구한다`가 표시되고 10초 뒤 숨는지 확인
- [ ] 두 pawn이 순서대로 나가거나 같은 pawn이 재진입·재이탈해도 objective alert가 map-session에 한 번만 표시되는지 확인
- [ ] 이미 tutorial SaveGame 이력이 있는 peer와 새 이력이 있는 peer에서 alert가 SaveGame에 의해 차단되지 않는지 확인
- [ ] `LunarMap_T` 이관 뒤 같은 2-player matrix 재실행
- [ ] `git diff --check`, `git status --short`, comment coverage, Intent/KNOW_HOW 갱신, 불필요 파일 미변경 확인

## 범위 제외

- [ ] tutorial queue·다중 popup 설계
- [ ] objective 진행 상태 저장 또는 퀘스트 완료 조건
- [ ] `WBP_ObjectiveAlertWidget`의 시각 디자인 변경
