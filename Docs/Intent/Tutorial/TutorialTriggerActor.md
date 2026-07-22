# ATutorialTriggerActor 의도 카드

## 목적

맵에 배치한 범위 진입으로 튜토리얼을 표시하되, overlap 판정은 서버가 소유하고 영구 최초 1회 판정은 각 로컬 SaveGame이 소유한다. 튜토리얼별 제목, 이미지, 설명, 키 안내는 `UTutorialStepData`가 소유하고 목적별 child Blueprint는 해당 DataAsset과 범위 설정만 선택한다.

## 주요 심볼

- `ATutorialTriggerActor::TriggerConfiguredTutorial`
- `ATutorialTriggerActor::HandleTriggerEndOverlap`
- `UTutorialBlueprintLibrary::ShowTutorialOnce`
- `UTutorialManagerSubsystem::ShowTutorialOnce`
- `UTutorialManagerSubsystem::IsRunningInPIE`
- `APragmataGameState::BeginTutorialPresentation`
- `UTutorialManagerSubsystem::ShowTutorialPresentation`
- `UTutorialManagerSubsystem::HideTutorialPresentation`

## 책임 경계

- C++ actor는 authority, player/controller 확인, `ActivationRole`, timed hide와 선택형 종료 objective alert를 처리한다.
- `ActivationRole`은 누가 범위를 밟아 발동할 수 있는지 결정한다.
- `UTutorialStepData::TargetRole`은 어느 로컬 viewport가 popup을 표시하는지 결정한다.
- 현재 role mapping은 `DA_Tutorial_WeaponsUse=Hugh`, `DA_Tutorial_Overdrive=Diana`, `DA_CleanseAcquisitionTutorial=None(양쪽)`이다.
- `BP_TutorialTrigger_<Name>`은 Event Graph 로직을 복제하지 않는다. Class Defaults에서 `TutorialStep`, `TriggerBox`, 역할, 종료 정책만 설정한다.
- 제목, 이미지, 설명, 키 표시는 actor에 복사하지 않고 `DA_Tutorial_<Name>`에서 편집한다.
- `TutorialId`는 로컬 SaveGame 이력 키다. 배포 후 rename하거나 다른 의미의 tutorial에 재사용하지 않는다.

## 전역 1회 호출

공간 trigger가 아닌 획득, 전투, 퍼즐 로직은 `UTutorialBlueprintLibrary::ShowTutorialOnce(WorldContextObject, TutorialId, StepData)`를 호출한다. server 호출은 `GameState` multicast로 각 peer에 전달한다. client 호출은 해당 로컬 viewport만 처리한다.

packaged/standalone `TutorialManagerSubsystem`은 전용 `PragmataTutorialProgress` SaveGame slot을 읽는다. PIE는 같은 slot을 읽거나 쓰지 않는 transient 이력을 사용해 매 PIE 실행에서 tutorial을 다시 검증한다. 저장되지 않은 ID가 역할 필터와 MVVM 초기화를 통과한 뒤에만 ID를 기록한다. 역할 불일치, null data, widget load 실패, MVVM 실패는 ID를 소비하지 않는다.

## 표시 수명

서버 `GameState`는 표시 요청마다 0이 아닌 `PresentationSerial`을 발급한다. `UTutorialManagerSubsystem`은 trigger, 획득, prop, Blueprint 직접 호출을 가리지 않고 성공 표시마다 DataAsset의 `DisplayDurationSeconds` timer를 같은 serial에 연결한다. timed hide는 자신이 발급한 serial과 로컬 subsystem의 활성 serial이 일치할 때만 popup을 닫는다. 이 계약으로 이전 timer가 더 최근 popup 또는 같은 DataAsset을 다시 표시한 popup을 닫지 못한다.

역할 불일치나 위젯 초기화 실패 client는 활성 serial을 바꾸지 않는다. 직접 `ShowTutorialStep()` 또는 client-local `ShowTutorialOnce()` 성공은 높은 local serial을 발급해 server presentation과 충돌하지 않는다.

`UTutorialManagerSubsystem`은 GameInstance 수명이므로 world cleanup에서 popup과 serial을 초기화한다. 새 world의 `GameState`가 serial을 다시 발급해도 이전 world presentation과 충돌하지 않는다.

actor-local 1회 상태는 두지 않는다. trigger 재진입은 같은 `TutorialId`로 다시 요청할 수 있지만, 각 peer는 popup 표시에 성공해 저장한 ID만 영구 차단한다. 역할 불일치나 UI 실패 peer는 이후 재진입에서 다시 시도할 수 있다.

`bShowObjectiveAlertOnExit`이 켜진 trigger는 첫 유효 player pawn의 EndOverlap에서만 `APragmataGameState::Multicast_ShowObjectiveAlert()`을 호출한다. 이 gate는 map-session transient 값이며 tutorial SaveGame 이력이나 objective 진행 상태를 저장하지 않는다.

`ACleanseSkillAcquisitionProp`은 Diana의 상호작용 권한만 검증한 뒤 `ShowTutorialOnce()` server 경로를 호출한다. `DA_CleanseAcquisitionTutorial.TargetRole=None`이므로 Hugh와 Diana가 같은 1회 tutorial과 5초 timer를 각각 받는다.

`UntilReplaced`는 trigger actor의 서버 종료 정책일 뿐, popup의 전역 표시 수명을 연장하지 않는다. 모든 tutorial DataAsset은 기본 `DisplayDurationSeconds=5.0`이며, 현재 `LunarMap_T`의 여섯 공간 안내도 `Timed`, `5.0초`를 사용한다.

## Blueprint 사용

1. `BP_TutorialTriggerBase`를 parent로 목적별 child Blueprint를 만든다.
2. child Class Defaults의 `TutorialStep`과 고유하고 안정된 `TutorialId`를 지정한다.
3. `TriggerBox` 크기와 transform, `ActivationRole`, `DismissPolicy`를 설정한다.
4. 맵에는 목적별 child를 배치하고 위치와 인스턴스 override만 조정한다.
5. WeaponUse child는 `bShowObjectiveAlertOnExit=true`, `ExitObjectiveAlertText=발전소 전력을 복구한다`, `ExitObjectiveAlertDisplaySeconds=10.0`으로 설정한다.
6. 모든 DataAsset의 `DisplayDurationSeconds`는 기본 `5.0초`를 유지한다. trigger child는 별도 server hide도 요청하도록 `DismissPolicy=Timed`를 사용한다.

`TriggerConfiguredTutorial()`과 `HideConfiguredTutorial()`은 client 호출을 server RPC로 전달하지 않는다. Blueprint에서 직접 호출할 때는 authority graph에서만 사용한다.

## Cook 소유권

맵에 배치한 child Blueprint와 그 hard DataAsset은 cooked map dependency가 소유한다. Config soft class인 `WBP_TutorialPopup`과 DataAsset의 soft `PreviewImage`는 `/Game/CookLabels/PAL_CoreRuntime`의 explicit asset으로 포함한다. 동적 spawn 또는 soft reference로 사용하는 child/DataAsset도 같은 PAL에 명시한다.

## 검증 기준

- 역할 불일치 overlap은 영구 표시 이력을 소비하지 않는다.
- 같은 `TutorialId`는 게임 재실행 뒤에도 다시 표시되지 않는다.
- 표시 실패 peer에는 `TutorialId`가 저장되지 않는다.
- trigger 재진입은 표시를 다시 요청할 수 있지만 저장된 peer에서는 같은 `TutorialId`를 다시 표시하지 않는다.
- 이전 timed hide가 최신 또는 legacy popup을 닫지 않는다.
- Blueprint Class Defaults와 맵 인스턴스에서 tutorial 및 Box 설정을 편집할 수 있다.
- packaged build에서 popup, preview image, RichText key icon이 로드된다.
- PIE 재시작 뒤에는 같은 `TutorialId`가 다시 표시되고, packaged/standalone 재시작 뒤에는 저장된 ID가 계속 차단된다.
- WeaponUse 종료 뒤 objective alert는 양쪽 local HUD에 한 번만 10초 표시된다.
