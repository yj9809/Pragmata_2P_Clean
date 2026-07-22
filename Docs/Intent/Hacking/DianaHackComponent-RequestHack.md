# UDianaHackComponent::RequestHack 의도 카드

## 대상

`UDianaHackComponent::RequestHack`

## 종류

Enemy hack client entry point

## 정의 위치

- `Source/Pragmata_2P/Components/DianaHackComponent.h`
- `Source/Pragmata_2P/Components/DianaHackComponent.cpp`

## 이 코드가 존재하는 이유

Diana 입력이나 reticle 선택을 server RPC로 넘기는 공개 진입점을 제공하기 위해 존재한다.

## 사용해야 하는 상황

- Diana가 enemy hack target을 선택한 경우
- caller가 target actor만 알고 있고 서버 검증은 component에 맡겨야 하는 경우

## 사용하면 안 되는 상황

- map node interaction
- 이미 서버 내부에서 puzzle component를 직접 조작하는 경우

## 기본 흐름

target actor 수신 -> `Server_RequestHack(TargetActor)` 호출 -> 서버에서 target 검증과 puzzle start

## 이 코드를 호출하는 쪽

- `ADianaOperatorPawn::RequestHack`
- reticle 기반 hack 요청 경로

## 이 코드가 호출하는 쪽

- `Server_RequestHack`

## 서버/클라이언트 조건

client는 요청만 보내고 서버가 target, line of sight, distance, active 상태를 판단한다.

## Blueprint 연결 여부

`BlueprintCallable`. Blueprint에서 호출 가능하므로 asset scan 결과와 함께 추적 필요.

## 헷갈리기 쉬운 코드

- `RequestHackDirection`: 이미 열린 enemy hack의 방향 입력
- `UDianaNodeInteractionComponent::RequestNodeInteraction`: map node 상호작용 진입점

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`
- `f25dc7db fix: Diana 해킹 입력을 aim 기반으로 정리`

## 확실한 근거

- `DianaHackComponent.h`에 `BlueprintCallable` 공개 함수로 선언
- `DianaHackComponent.cpp`에서 server RPC 호출

## 추정한 의도

Diana client 입력을 신뢰하지 않고 서버 권위 hack 시작 경로로 통일하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint에서 `RequestHack`을 직접 호출하는 asset 목록.

## 검증 방법

```powershell
rg -n "RequestHack\\(" Source Docs
```

## 다음 수정 시 주의점

target 검증을 client-side convenience로 옮기지 않는다.
