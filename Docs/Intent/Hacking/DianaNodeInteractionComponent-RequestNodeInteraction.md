# UDianaNodeInteractionComponent::RequestNodeInteraction 의도 카드

## 대상

`UDianaNodeInteractionComponent::RequestNodeInteraction`

## 종류

Diana POI interaction entry point

## 정의 위치

- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h`
- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp`

## 이 코드가 존재하는 이유

Diana가 조준한 POI 상호작용을 서버 권위 경로로 요청하기 위해 존재한다.
scan marker 표시와 실제 상호작용 실행을 분리하고, `MapHackNode`는 aim-only, `ItemBox` 같은 non-MapHackNode POI는 click-required로 나눈다.

## 사용해야 하는 상황

- Diana가 `MapHackNode`를 aim focus만으로 실행하려는 경우
- Diana가 `IA_Interact` 입력으로 item box 같은 non-MapHackNode POI를 실행하려는 경우
- scan target이 아닌 `BP_DeadFilamentBox`를 실제 조준하고 `IA_Interact`로 Cleanse 획득 연출을 시작하려는 경우
- `Roaming + Aim` 상태에서 화면 중앙의 POI를 서버가 다시 찾아야 하는 경우

## 사용하면 안 되는 상황

- enemy hack target 처리
- 이미 active map hack의 방향 입력 처리
- `IA_Scan` 입력 처리 또는 POI marker 표시
- item box를 조준만으로 자동 open하는 경로

## 기본 흐름

- `RefreshFocusedNodeLog` -> `TryRequestFocusedMapHackNode` -> `RequestNodeInteraction` server RPC -> server world registry에서 `MapHackNode` 재검증 -> map hack/node 완료 처리
- `ADianaOperatorPawn::OnStartInteract` -> `RequestNodeInteraction` -> `Server_RequestNodeInteraction` -> server world registry에서 focused target 재검증 -> `ItemBox`, `Checkpoint` 또는 다른 click 입력 POI 처리
- Cleanse 획득 prop은 scan registry에 등록하지 않는다. 대신 camera ray가 해당 actor를 직접 hit했을 때만 click-required interaction 후보에 추가하고 서버에서 같은 조건을 재검증한다.
- focus 재검증은 camera ray가 실제 candidate 또는 candidate child surface에 hit한 경우만 통과. actor root location cone 판정은 사용하지 않음
- `SetFocusedNodeActor`는 local focus 전환에서 ItemBox 계열의 key prompt를 표시하고 이전 focus prop의 prompt를 숨긴다. 이 UI 경로는 server interaction 권한과 분리된다.

## 이 코드를 호출하는 쪽

- Diana focus refresh 경로의 `MapHackNode` 자동 요청
- Diana `IA_Interact` 입력 처리 경로의 click-required POI 요청. checkpoint도 hold timer가 아닌 이 경로로 즉시 실행

## 이 코드가 호출하는 쪽

- `Server_RequestNodeInteraction`

## 서버/클라이언트 조건

client는 local hit actor와 최신 camera direction을 요청에 포함할 뿐이며, 서버가 그 direction으로 ray trace를 다시 실행해 target 일치, role, action lock, aim 상태, 완료 상태를 판단한다.
client focus registry는 UI 후보 cache이고, 최종 interaction은 server registry에서 다시 찾는다.

## Blueprint 연결 여부

`BlueprintCallable`. Blueprint input binding이나 widget에서 호출 가능성 있음.

## 헷갈리기 쉬운 코드

- `UDianaScanComponent::RequestScan`: `IA_Scan`으로 POI marker만 표시
- `UDianaHackComponent::RequestHack`: enemy hack 시작
- `RequestMapHackDirection`: active map hack 방향 입력
- `TryRequestFocusedMapHackNode`: focus 유지 중 `MapHackNode` 자동 요청과 반복 RPC 억제

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `DianaNodeInteractionComponent.h`에 `BlueprintCallable`로 선언
- `Docs/Systems/Hacking/map-hacking-integration.md`에 map puzzle 시작 흐름 기록

## 추정한 의도

POI 선택, item box open, map node 완료 판정, map hack 시작을 하나의 Diana component에 모으려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

일반 node interaction과 map hack 전환 기준의 모든 asset 사례.

## 검증 방법

```powershell
rg -n "RequestNodeInteraction|Server_RequestNodeInteraction|TryStartMapHack" Source Docs
```

## 다음 수정 시 주의점

enemy hack component와 POI interaction component의 책임을 섞지 않는다.
scan marker 표시를 이 함수의 호출 조건으로 쓰지 않는다.
`MapHackNode` 자동 요청을 복구하더라도 `ItemBox`를 aim-only로 열지 않는다.
부모 Blueprint mesh hit를 child `BP_PuzzleNode_Elevator` target으로 승격하지 않는다. 실제 hit actor의 attach chain 안에 candidate가 있어야 한다. `ABossRouteElevator` child puzzle은 `PuzzleMesh`만 허용하고 `Box`, `PuzzleStateMarker`, parent `ElevatorMesh` hit는 거부한다.
