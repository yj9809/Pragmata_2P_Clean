# IPragmataScannable POI 의도 카드

## 대상

`IPragmataScannable::GetPragmataScanResult`, `APropInteractionBase::GetPragmataScanResult_Implementation`

## 종류

POI metadata canonical entry point

## 정의 위치

- `Source/Pragmata_2P/Interfaces/PragmataScannable.h`
- `Source/Pragmata_2P/Props/PropInteractionBase.h`
- `Source/Pragmata_2P/Props/PropInteractionBase.cpp`

## 이 코드가 존재하는 이유

enemy scan과 POI scan이 서로 다른 metadata API로 갈라지는 것을 막고, scan 표시 정보와 Diana POI interaction 거리 정책을 `FPragmataScanResult` 하나로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- scan UI에 표시할 actor id, 종류, 텍스트, 거리 정보를 만들 때
- `ItemBox`, `MapHackNode`, enemy처럼 scan 가능 대상이 자기 표시 정보를 제공할 때
- 완료된 POI를 scan 결과에서 제외해야 할 때
- Diana interaction 후보가 `TargetKind`와 `DianaInteractionDistance`를 읽어 종류별 interaction 거리를 적용할 때

## 사용하면 안 되는 상황

- 실제 상호작용 실행 자체
- puzzle 완료 처리
- item 지급
- enemy hack damage 적용
- string `Actor Tags` 기반 gameplay 분기 추가

## 기본 흐름

scan/interaction 후보 actor -> world-local `APragmataGameState` registry -> `IPragmataScannable::GetPragmataScanResult` -> `FPragmataScanResult.TargetKind`로 HUD marker 종류와 Diana interaction 종류 선택 -> `DianaInteractionDistance`로 종류별 상호작용 거리 적용

## 성능 경계

- POI scan과 Diana POI interaction은 `TActorIterator`나 `GetAllActors`로 월드 전체를 훑지 않음
- registry 순회 비용은 전체 actor 수가 아니라 등록된 POI 수에 비례
- local focus용 client registry와 server authority registry는 같은 등록 규칙을 쓰지만 서로 복제되는 gameplay state가 아님

## 인접 코드와 차이

- `IPropInteractableInterface::Interact`: 상호작용 실행 진입점
- `UPropInteractionComponent::TriggerInteraction`: interaction 호출 helper
- `IPragmataScannable::GetPragmataScanResult`: POI 종류, 표시 metadata, Diana interaction 거리 조회
- `APragmataGameState::RegisteredScannablePOIs`: 복제 상태가 아니라 각 world의 local 후보 cache. client focus와 server RPC 재판정이 각각 자기 world cache를 사용

## 관련 문서

- `Docs/Intent/Damage/ReceiveDamage.md`
- `Docs/Intent/Scan/DianaScanComponent-RequestScan.md`
- `Docs/KNOW_HOW.md`

## 수정 시 주의점

`GetItemBoxScanData`, `GetMapNodePOIData`, string tag 분기 같은 별도 API를 추가하지 않음. 새 POI 종류가 필요하면 `EPragmataScanTargetKind`와 기존 `FPragmataScanResult`를 확장한다.
registry 등록을 server-only로 막으면 Diana client focus가 후보를 찾지 못해 map hack 자동 시작이 끊긴다.
