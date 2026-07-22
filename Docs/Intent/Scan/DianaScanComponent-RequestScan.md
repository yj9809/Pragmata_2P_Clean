# UDianaScanComponent::RequestScan 의도 카드

## 대상

`UDianaScanComponent::RequestScan`, `Server_RequestScan`, `CollectAreaScanResults`

## 종류

Diana POI area scan entry point

## 정의 위치

- `Source/Pragmata_2P/Components/DianaScanComponent.h`
- `Source/Pragmata_2P/Components/DianaScanComponent.cpp`

## 이 코드가 존재하는 이유

Diana가 주변 50m 안의 POI를 찾고, Hugh와 Diana 양쪽 local client에서 POI 자체의 `WBP_ScanIconWidget`을 일정 시간 보여주기 위해 존재한다. `APropInteractionBase`가 공통 `ScanIconComponent`와 Diana local key prompt 소유권을 제공하므로 ItemBox, MapHackNode, Checkpoint는 Blueprint별 별도 HUD marker 설정 없이 같은 object UI 경로를 쓴다.

## 사용해야 하는 상황

- `IA_Scan` 입력으로 주변 `ItemBox`, `MapHackNode`, `Checkpoint` marker를 표시할 때
- scan duration과 `1초` cooldown을 서버 기준으로 제한해야 할 때
- 가까운 POI부터 늦게 드러나는 wave reveal timing이 필요할 때

## 사용하면 안 되는 상황

- enemy hack target 판정
- map hack node 상호작용 시작
- item box open
- damage, inventory, puzzle 완료 같은 gameplay 상태 변경

## 기본 흐름

`ADianaOperatorPawn::RequestScan` -> `UDianaScanComponent::RequestScan` -> server RPC -> `APragmataGameState` POI registry 조회 -> `IPragmataScannable::GetPragmataScanResult` -> Hugh/Diana `APragmataPlayerController::Client_ShowPOIScanResults` + `Multicast_PlayScanAnimation(AS_Diana_General2_2000_Scan_VerA)` -> `RevealDelay` 뒤 target `UWidgetComponent`의 `UScanIconWidget` 점등 -> 마지막 reveal 기준 공통 hide timer로 정리

reveal마다 icon은 5초 표시하고 rescan은 기존 reveal/hide timer를 갱신한다. scan이 성공하면 POI 개수와 무관하게 Hugh/Diana owning client에 `Client_PlayDianaScanPulse(ScanOrigin, ScanRadius, 1.5초)`도 보낸다. pulse는 local `PostProcessVolume`의 color grading/bloom override로 씬 표면을 파랗게 밝히는 시각 효과 전용이며 전체 blend weight는 이전의 50%다.

`BP_DeadFilament_Box`는 `bCanBeScanned=false`, `TargetKind=Unknown`이며 scan registry 결과에 포함하지 않는다. 20m Hugh proximity icon은 scan과 독립된 prop-owned 표시 경로다.

## 서버/클라이언트 조건

서버가 Diana role, action lock, cooldown, enemy combat state를 검증한다. scan은 `Roaming` + non-combat 전용이며 battle/tense 상태에서는 표시 결과와 local scan pulse 요청을 보내지 않는다.

진단 로그는 입력 도달, 서버 거부 사유, enemy combat 거부, registry 후보 수, 반경 내 후보 수, 유효 POI 수를 구분한다. 반경 안 scannable 0개 상태는 local screen/log PrintString으로 표시한다.
scan 결과에는 actor reference와 scan 시점 world location을 함께 담아 HUD가 replication resolve 타이밍에 덜 흔들리게 한다.

## 헷갈리기 쉬운 코드

- `UDianaNodeInteractionComponent`: `IA_Interact` 입력으로 item box나 map hack node를 실제 상호작용하는 경로
- `UDianaHackComponent`: enemy hack puzzle 시작과 damage 경로
- `IPragmataScannable`: POI metadata 단일 진입점. POI 종류별 별도 `GetPOIData` API나 string tag 분기를 만들지 않음
- `UScanIconWidget`: object-owned Screen-space icon 표시와 거리 text 담당. scan 결과를 interaction 판정으로 재사용하지 않음
- `ScanIconComponent` 자체는 render 가능 상태를 유지한다. 초기/scan/hide 상태는 `UScanIconWidget::SetScanIconVisible()`만 변경하며 component visibility로 숨기지 않음
- `APropInteractionBase::ShowInteractionPromptWidget`: Diana가 유효하게 조준한 prop의 local key prompt 표시. scan icon과 같은 visibility를 공유하지 않음

## 관련 문서

- `Docs/KNOW_HOW.md` - `Diana Scan, Hack, and Node Interaction`
- `Docs/Intent/Scan/PragmataScannable-POI.md`

## 수정 시 주의점

월드 전체 순회로 POI를 찾지 않음. 새 POI는 `IPragmataScannable` 결과와 GameState registry에 연결해 중복 scan/interaction API를 만들지 않음.
