# Blueprint 편집형 튜토리얼 트리거 구현 계획

## 요구사항 요약

- [x] 공통 overlap/network 로직은 한 곳에서 유지하고 Map마다 Level Blueprint 로직을 복사하지 않음
- [x] 각 튜토리얼은 별도 `UTutorialStepData`로 제목, 이미지, 설명, 키 안내, 표시 역할을 설정
- [x] 각 `BP_TutorialTrigger_<Name>` child Blueprint가 서로 다른 `TutorialStepData`를 Class Defaults에서 선택
- [x] Blueprint는 고유 DataAsset reference와 trigger 설정을 편집하고, 제목·이미지·설명 자체는 선택한 DataAsset의 Details에서 편집
- [x] Map에 배치한 개별 trigger 인스턴스에서도 `TutorialStep`, Box 크기, 활성 역할, 1회 발동, 종료 정책을 수정 가능
- [x] trigger 판정과 1회 발동 상태는 server authority에서 처리
- [ ] Hugh host/Diana client와 Diana host/Hugh client 배치에서 같은 결과 보장
- [ ] PIE뿐 아니라 packaged build에서도 popup, DataAsset, preview image, RichText key icon 로드 보장

## 현재 기반

- `UTutorialStepData`가 `StepTitle`, soft `PreviewImage`, `DescriptionText`, `KeyBindings`, `TargetRole` 보유
  - `Source/Pragmata_2P/Data/TutorialStepData.h:10-48`
- `UTutorialManagerSubsystem::ShowTutorialStep()`이 로컬 `AssignedRole`과 `TargetRole`을 비교하고 popup 하나를 재사용
  - `Source/Pragmata_2P/Game/TutorialManagerSubsystem.cpp:10-71`
- `APragmataGameState::Multicast_ShowTutorialStep()`이 서버 요청을 각 peer의 로컬 subsystem으로 전달
  - `Source/Pragmata_2P/Game/PragmataGameState.h:112-117`
  - `Source/Pragmata_2P/Game/PragmataGameState.cpp:337-356`
- 기존 server-authoritative Box Actor 구현 기준
  - `Source/Pragmata_2P/Props/PlayerTeleportTrigger.h:10-39`
  - `Source/Pragmata_2P/Props/PlayerTeleportTrigger.cpp:6-63`
- popup class는 `Config/DefaultGame.ini:9-10`의 runtime soft class reference
- `PreviewImage`는 `UTutorialStepViewModel::InitFromStepData()`에서 동기 soft load
  - `Source/Pragmata_2P/UI/TutorialStepViewModel.cpp:7-26`

## 설계 결정

- [x] `ATutorialTriggerActor` C++ base에 collision, authority, 역할 판정, one-shot, show/hide lifecycle 구현
- [x] `/Game/Blueprints/Props/Tutorial/BP_TutorialTriggerBase`는 C++ base의 공용 Blueprint 표현만 소유
- [x] 목적별 `BP_TutorialTrigger_<Name>` child는 Event Graph 복사 없이 고유 `TutorialStep`과 Box 크기만 기본값으로 설정
- [x] `TutorialStep`은 `EditAnywhere, BlueprintReadOnly`로 노출해 child Class Defaults와 배치 인스턴스 양쪽에서 교체 가능하게 구성
- [x] 콘텐츠 값은 Actor에 중복 저장하지 않고 `DA_Tutorial_<Name>`이 단일 소유
- [x] `TutorialStep.TargetRole`은 popup을 볼 역할, trigger의 `ActivationRole`은 공간에 진입해 발동할 역할로 분리
- [x] `ActivationRole=None`은 어느 player pawn도 발동 가능, 역할 지정 시 다른 역할 overlap은 one-shot을 소비하지 않음
- [x] 기본 종료 정책은 `UntilReplaced`, 선택 정책으로 `Timed` 제공
- [x] `OnEndOverlap` 자동 종료는 두 pawn 동시 overlap과 one-shot collision disable 충돌 때문에 기본 정책에서 제외
- [x] 이전 timed trigger가 나중에 표시된 tutorial을 닫지 않도록 표시 요청별 고유 `PresentationSerial` 기반 hide 사용
- [x] 같은 `TutorialStepData`가 연속으로 다시 표시되어도 이전 timer는 최신 presentation을 닫지 않음

## 구현 TODO

### 1. C++ trigger base 추가

- [x] `Source/Pragmata_2P/Props/TutorialTriggerActor.h` 추가
- [x] `Source/Pragmata_2P/Props/TutorialTriggerActor.cpp` 추가
- [x] `ATutorialTriggerActor`를 `UCLASS(Blueprintable)` Actor로 선언
- [x] `UBoxComponent* TriggerBox`를 root로 생성하고 Pawn channel만 `Overlap`하도록 설정
- [ ] 다음 값을 `Pragmata|Tutorial Trigger` Category로 Blueprint에 노출
  - [ ] `TutorialStep`: `TObjectPtr<UTutorialStepData>`, `EditAnywhere`, `BlueprintReadOnly`
  - [ ] `ActivationRole`: `EPragmataPlayerRole`, 기본값 `None`
  - [ ] `bEnabled`: 기본값 `true`
  - [ ] `bTriggerOnce`: 기본값 `true`
  - [ ] `DismissPolicy`: `UntilReplaced` 또는 `Timed`
  - [ ] `DisplayDurationSeconds`: `Timed`에서만 편집, `ClampMin=0.1`
- [x] Box extent와 transform을 Blueprint component Details 및 Map instance에서 수정 가능하게 유지
- [x] `BeginPlay()`에서 overlap delegate를 한 번만 연결
- [x] `HandleTriggerBeginOverlap()`에서 `HasAuthority()`를 가장 먼저 확인
- [x] `OtherActor`가 player-controlled Pawn이고 controller가 `APragmataPlayerController`인지 확인
- [x] `ActivationRole`이 지정된 경우 `GetAssignedRole()`이 일치할 때만 발동
- [x] null `TutorialStep`은 actor 이름을 포함한 warning 후 아무 상태도 소비하지 않음
- [x] 유효한 overlap에서 `APragmataGameState::BeginTutorialPresentation(TutorialStep)` 호출
- [ ] 반환된 non-zero `PresentationSerial`을 actor의 latest serial과 timed hide timer에 저장
- [ ] `0` 반환은 authority/null/발급 실패로 취급하고 one-shot 상태와 timer를 소비하지 않음
- [ ] non-zero serial 반환 직후 server-side one-shot 상태를 소비하고 timer를 예약
- [ ] serial 발급과 RPC 발행 뒤 client 부재, widget load 실패, role filter skip이 발생해도 one-shot 요청은 소비된 상태로 유지
- [ ] one-shot trigger는 추가 overlap을 무시하되 역할 불일치 overlap은 소비하지 않음
- [ ] `Timed` 정책은 server timer에서 해당 presentation serial만 닫도록 요청
- [x] actor 종료 또는 Map unload 시 timer clear
- [x] Blueprint scripted event용 `TriggerConfiguredTutorial(AActor* TriggeringActor)`와 `HideConfiguredTutorial()`을 `BlueprintCallable`, `BlueprintAuthorityOnly`로 노출
- [ ] 위 함수는 client 호출을 server로 전달하는 RPC가 아니며 authority graph에서만 호출해야 함을 tooltip과 intent 문서에 명시
- [ ] `HideConfiguredTutorial()`은 이 actor가 마지막으로 발행한 presentation serial만 종료하고 global `HideTutorial()`은 호출하지 않음

### 2. 표시 요청별 presentation lifecycle 보강

- [x] `Source/Pragmata_2P/UI/TutorialPopupWidget.h/.cpp`의 초기화 경로가 MVVM View/ViewModel 주입 성공 여부를 `bool`로 반환하도록 보강
- [x] `Source/Pragmata_2P/Game/TutorialManagerSubsystem.h/.cpp`에 local 표시 성공 여부를 반환하는 내부 `TryShowTutorialStep()` helper 추가
- [ ] helper는 null, widget class, local controller, role filter, widget 생성, MVVM 초기화를 모두 통과한 경우에만 `true` 반환
- [ ] 새 widget의 MVVM 초기화가 실패하면 blank popup을 남기지 않고 viewport 상태를 정리
- [x] `Source/Pragmata_2P/Game/PragmataGameState.h/.cpp`에 authority-only `BeginTutorialPresentation(UTutorialStepData* StepData)` 추가
- [x] `GameState`가 world 단위 monotonic `uint32 TutorialPresentationSerial`을 증가시키고 `0`은 legacy/direct show 예약값으로 유지
- [ ] serial overflow 시 `0`을 건너뛰어 유효 presentation이 legacy 값과 충돌하지 않게 처리
- [ ] `BeginTutorialPresentation()`이 새 serial로 multicast show를 실행하고 trigger가 timer에 보관할 serial을 반환
- [ ] `Multicast_ShowTutorialPresentation(UTutorialStepData* StepData, uint32 PresentationSerial)` Reliable RPC 추가
- [ ] `Multicast_HideTutorialPresentation(uint32 ExpectedPresentationSerial)` Reliable RPC 추가
- [x] `Source/Pragmata_2P/Game/TutorialManagerSubsystem.h/.cpp`에 현재 `ActivePresentationSerial` transient 값 저장
- [ ] presentation show가 해당 local peer의 role filter와 popup 초기화를 실제 통과한 경우에만 `ActivePresentationSerial`을 새 serial로 갱신
- [ ] role mismatch 또는 local 표시 실패 peer는 기존 popup과 기존 active serial을 그대로 유지
- [ ] 기존 public `ShowTutorialStep()`은 같은 `TryShowTutorialStep()` helper를 사용하고 실제 표시 성공 시 `ActivePresentationSerial=0`으로 갱신
- [ ] `HideTutorialPresentation(uint32 ExpectedPresentationSerial)`은 expected serial이 active serial과 일치할 때만 popup 제거
- [ ] 기존 `ShowTutorialStep()`/`HideTutorial()`과 `Multicast_ShowTutorialStep()`/`Multicast_HideTutorial()`은 기존 cleanse 및 legacy caller 호환을 위해 유지
- [ ] 기존 global `HideTutorial()` 실행 시 active serial도 초기화
- [ ] timed trigger A 뒤 trigger B가 표시된 경우 A timer가 B를 닫지 않는지 검증
- [ ] trigger A와 C가 같은 DataAsset을 사용해도 A timer가 최신 C presentation을 닫지 않는지 검증

### 3. Blueprint base와 목적별 child 제작

- [x] C++ Editor target build 후 `/Game/Blueprints/Props/Tutorial/BP_TutorialTriggerBase` 생성
- [x] parent class를 `ATutorialTriggerActor`로 지정
- [x] base Blueprint Event Graph에는 tutorial별 로직을 추가하지 않음
- [x] `TriggerBox`의 기본 크기와 editor 색상을 배치하기 쉬운 값으로 설정
- [ ] tutorial마다 `/Game/Data/Tutorial/DA_Tutorial_<Name>` 생성
- [x] 기존 `/Game/UI/Data/DA_Tutorial_WeaponsUse`와 `/Game/Data/Tutorial/DA_CleanseAcquisitionTutorial`은 중복 생성하지 않고 필요 시 그대로 참조
- [ ] 각 DataAsset에서 다음 값 입력
  - [ ] `StepTitle`
  - [ ] `PreviewImage`
  - [ ] `DescriptionText`
  - [ ] `KeyBindings.ActionLabel`
  - [ ] `KeyBindings.IconStyleRowName`
  - [ ] `TargetRole`
- [ ] `IconStyleRowName`이 `/Game/UI/Data/RichTextData/DT_TutorialKeyIcons`의 실제 row와 일치하는지 확인
- [x] 각 목적별 `/Game/Blueprints/Props/Tutorial/BP_TutorialTrigger_<Name>` child 생성
- [x] child Class Defaults에서 해당 `DA_Tutorial_<Name>`을 `TutorialStep`에 지정
- [ ] child마다 필요한 `TriggerBox` extent, `ActivationRole`, `DismissPolicy`, duration 설정
- [ ] Map에는 목적별 child Blueprint를 배치하고 위치와 Box transform만 조정
- [ ] 동일 child를 다른 내용으로 재사용해야 할 때만 배치 인스턴스의 `TutorialStep` override 사용
- [x] 공간 진입 예제로 `BP_TutorialTrigger_WeaponsUse`가 필요하면 기존 `DA_Tutorial_WeaponsUse`를 참조
- [x] cleanse acquisition tutorial은 현재 `CleanseSkillAcquisitionProp`의 targeted Client RPC 경로를 유지하고 별도 요청 없이 공간 trigger로 이전하지 않음

### 4. 역할과 발동 정책 적용

- [ ] 공용 tutorial은 `TargetRole=None`, `ActivationRole=None`으로 설정해 누구의 진입이든 양쪽에 표시
- [ ] Hugh 전용 tutorial은 `TargetRole=Hugh`, 기본 `ActivationRole=Hugh`로 설정
- [ ] Diana 전용 tutorial은 `TargetRole=Diana`, 기본 `ActivationRole=Diana`로 설정
- [ ] 누가 먼저 진입해도 특정 역할에게 알려야 하는 예외만 `ActivationRole=None`과 role-specific `TargetRole` 조합 사용
- [ ] “밟은 connection 한 명에게만 표시”는 현재 multicast 계약과 다른 요구사항이므로 별도 targeted Client RPC 후속 범위로 분리
- [ ] role handoff 상황에서도 `AssignedRole` 기준을 유지할지 실제 플레이 정책과 대조

### 5. cook coverage 보강

- [x] `/Game/CookLabels/PAL_CoreRuntime` saved asset을 읽어 `Cook Rule=Always Cook`, `Is Runtime Label=true` 확인
- [x] Config soft class `/Game/UI/Widget/Tutorial/WBP_TutorialPopup`을 explicit asset에 추가
- [x] `/Game/UI/Widget/Tutorial/BP_TutorialImageDecorator`와 `/Game/UI/Data/RichTextData/DT_TutorialKeyIcons`가 popup의 hard dependency로 cook되는지 saved dependency와 staged manifest에서 확인
- [ ] 배치된 목적별 trigger Blueprint와 hard `TutorialStep` DataAsset은 cooked map dependency를 cook owner로 사용
- [x] 동적 spawn 또는 soft reference로 사용하는 trigger Blueprint/DataAsset만 `PAL_CoreRuntime` explicit asset에 추가
- [x] 각 DataAsset의 soft `PreviewImage` texture를 `PAL_CoreRuntime` explicit asset에 추가
- [x] PAL 저장 후 expected asset 목록을 다시 읽어 누락 여부 확인
- [x] broad `/Game/UI` directory cook 규칙은 추가하지 않음

### 6. 의도와 운용 문서 갱신

- [x] `Docs/Intent/Tutorial/TutorialTriggerActor.md` 생성
  - [ ] server authority와 one-shot 소유권
  - [ ] `TargetRole`과 `ActivationRole` 차이
  - [ ] child Blueprint와 DataAsset 책임 구분
  - [ ] presentation serial 기반 hide가 필요한 이유
- [x] `Docs/KNOW_HOW.md`에서 기존 tutorial recipe를 다시 검색하고 현재 파일에 없으면 `Tutorial Popup and World Trigger` recipe 추가
- [x] recipe에 DataAsset 제작, child Blueprint 제작, Map 배치, network flow, cook 검증 기록
- [x] 새 overlap/RPC/timer/UI lifecycle 함수에 `02-unreal-code.md` 기준 Korean intent comment 적용
- [x] 작업 종료 전 불필요한 Level Blueprint 로직과 중복 Event Graph가 추가되지 않았는지 확인

## 수용 기준

- [ ] `BP_TutorialTrigger_A`와 `BP_TutorialTrigger_B`가 서로 다른 `DA_Tutorial_*`를 Class Defaults에 보유
- [ ] 각 child Blueprint에서 고유 DataAsset을 선택하고 해당 DataAsset Details에서 title, image, description을 독립 편집 가능
- [ ] A 진입 시 A의 title, image, description만 표시
- [ ] B 진입 시 같은 popup instance가 B의 title, image, description으로 교체
- [ ] 배치 인스턴스 Details에서 `TutorialStep`을 바꾸면 재컴파일 없이 다른 tutorial 표시
- [ ] Box extent를 Blueprint와 Map instance 양쪽에서 편집 가능
- [ ] `TargetRole=None`은 Hugh와 Diana 양쪽 viewport에 표시
- [ ] `TargetRole=Hugh` 또는 `Diana`는 지정 역할 viewport에만 표시
- [ ] 역할 제한 trigger를 다른 역할이 밟아도 표시되지 않고 one-shot도 소비되지 않음
- [ ] 같은 역할이 one-shot trigger를 반복 overlap해도 한 번만 표시
- [ ] `bEnabled=false` trigger는 표시 요청과 one-shot 소비를 모두 수행하지 않음
- [ ] GameState를 찾지 못하면 warning을 기록하고 one-shot을 소비하지 않음
- [ ] `ActivationRole=None`, `TargetRole=Diana` 조합은 Hugh 진입으로 발동해 Diana에게만 표시
- [ ] `UntilReplaced` tutorial은 다음 show 또는 explicit hide 전까지 유지
- [ ] `Timed` tutorial은 설정 시간 뒤 닫힘
- [ ] 이전 timer는 더 최근 tutorial popup을 닫지 않음
- [ ] 같은 DataAsset을 다시 표시한 최신 popup도 이전 presentation timer가 닫지 않음
- [ ] timed world trigger 뒤 기존 `ShowTutorialStep()` 경로의 cleanse tutorial이 표시되면 이전 timer가 cleanse popup을 닫지 않음
- [ ] Hugh viewport에 tutorial A가 표시된 동안 Diana-only presentation B를 수신하면 Hugh는 A와 기존 serial을 유지하고 B timer가 A를 닫지 않음
- [ ] null `TutorialStep` trigger는 crash 없이 warning만 기록
- [ ] Hugh host/Diana client와 Diana host/Hugh client에서 표시 대상과 발동 횟수가 동일
- [ ] packaged build에서 popup, preview image, RichText key icon이 모두 표시
- [ ] streaming level unload 또는 trigger `EndPlay` 뒤 timer callback이 실행되지 않음

## 검증 TODO

- [x] `rg -n "TutorialTriggerActor|BeginTutorialPresentation|PresentationSerial|Multicast_HideTutorialPresentation" Source Docs`로 선언, 구현, 문서 누락 확인
- [x] Editor target build로 UHT, delegate signature, RPC compile 검증
- [x] `BP_TutorialTriggerBase`와 목적별 child Blueprint compile/save/reopen
- [ ] 두 child Blueprint의 `TutorialStep` Class Default가 저장 후 유지되는지 readback
- [ ] Standalone PIE에서 A/B 순차 진입과 instance override 확인
- [ ] 2-player PIE에서 공용/Hugh 전용/Diana 전용 role matrix 확인
- [ ] 두 pawn 동시 overlap과 빠른 재진입에서 one-shot 중복 호출 여부 확인
- [ ] A timed show 직후 B show 시 A timer 만료 뒤 B가 유지되는지 확인
- [ ] A와 C가 같은 DataAsset을 사용해 순차 표시될 때 A timer 만료 뒤 C가 유지되는지 확인
- [ ] timed world trigger 직후 cleanse acquisition의 targeted client show를 실행해 이전 timer가 cleanse popup을 닫지 않는지 확인
- [ ] Hugh에 A를 표시한 뒤 Diana-only B를 multicast하고 B timer 만료 전후 Hugh의 A와 active serial 유지 확인
- [ ] `bEnabled=false`, GameState 누락, streaming level unload 경로 확인
- [ ] output log에서 null data, widget load, MVVM ViewModel warning이 없는지 확인
- [x] PAL saved asset readback으로 popup과 preview texture explicit entry 확인
- [x] cooked map dependency readback으로 배치 child Blueprint와 hard DataAsset 포함 확인
- [ ] packaged Development build smoke test에서 tutorial asset load 확인
- [x] `git diff --check`와 `git status --short`로 계획 외 파일 및 불필요한 asset 변경 확인

## 위험과 대응

- [ ] 역할 불일치 overlap이 one-shot을 먼저 소비할 위험
  - 대응: actor/controller/role 검증을 `bTriggered` 갱신보다 먼저 실행
- [ ] 이전 timed trigger가 새 tutorial 또는 같은 DataAsset의 새 표시를 닫는 위험
  - 대응: `ActivePresentationSerial`과 expected serial이 일치할 때만 hide
- [ ] 두 pawn이 거의 동시에 진입해 show가 중복 호출될 위험
  - 대응: server authority에서 단일 one-shot 상태 검사와 갱신
- [ ] 목적별 child Blueprint마다 Event Graph가 복제되어 수정 누락이 생길 위험
  - 대응: logic은 C++ base에만 두고 child는 DataAsset과 Box defaults만 소유
- [ ] PIE에서만 soft preview image와 popup이 보이고 package에서 누락될 위험
  - 대응: `PAL_CoreRuntime` explicit coverage와 packaged smoke test를 완료 조건으로 지정
- [ ] 여러 tutorial이 동시에 필요한 요구가 생길 위험
  - 대응: 현재 한 viewport당 popup 하나와 replace 정책을 명시하고 queue/stack은 후속 설계로 분리

## 범위 제외

- [ ] seen tutorial의 SaveGame 영구 저장
- [ ] tutorial queue 또는 여러 popup 동시 표시
- [ ] 특정 network connection 한 명만 지정하는 Blueprint Client RPC
- [ ] popup layout 재설계와 신규 animation
- [ ] input device별 icon 자동 전환
