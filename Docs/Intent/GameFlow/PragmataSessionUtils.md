# PragmataSessionUtils 의도 카드

## 대상

`PragmataSessionUtils`

## 종류

Listen session room code와 travel URL helper

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataSessionUtils.h`
- `Source/Pragmata_2P/Game/PragmataSessionUtils.cpp`
- `Source/Pragmata_2P/Game/PragmataSessionUtilsTests.cpp`

## 이 코드가 존재하는 이유

room code, role option, lobby/game travel URL 문자열을 여러 GameMode와 SessionSubsystem에 흩어 두지 않고 한곳에서 같은 규칙으로 만들기 위해 존재한다.

## 사용해야 하는 상황

- host가 listen lobby를 열 때
- join client가 room code와 preferred role을 travel option으로 넘길 때
- lobby에서 gameplay map으로 `ServerTravel`할 때
- room code를 normalize하거나 validation할 때

## 사용하면 안 되는 상황

- UI 표시 문자열만 만들 때
- map asset 선택 자체를 결정할 때
- runtime에서 role ownership을 직접 확정할 때

## 기본 흐름

room code 생성/정규화 -> session setting/search key로 사용 -> host는 `OpenLevel` listen option 생성 -> join은 connect URL option 생성 -> lobby는 gameplay `ServerTravel` URL 생성

## 이 코드를 호출하는 쪽

- `UPragmataMultiplayerSessionSubsystem`
- `APragmataLobbyGameMode`
- `APragmataGameMode`
- automation test

## 이 코드가 호출하는 쪽

- Unreal string/path API
- `EPragmataPlayerRole` option formatting

## 서버/클라이언트 조건

host 첫 listen 전환은 아직 listen NetDriver가 없으므로 `OpenLevel` option을 만든다. lobby에서 gameplay로 넘어갈 때는 server가 `BuildGameTravelURL` 결과로 `ServerTravel`한다.

## Blueprint 연결 여부

C++ namespace helper라 Blueprint 직접 호출 경로는 없다.

## 헷갈리기 쉬운 코드

- `APragmataTitlePlayerController::TravelToLobby`: title에서 local lobby map으로 이동
- `APragmataLobbyGameMode::RequestStartGame`: lobby server가 gameplay map으로 이동
- `UPragmataGameFlowConfig`: 어떤 map/class를 쓸지 제공하는 config

## 관련 문서

- `Docs/Systems/Server/server.md`
- `Docs/KNOW_HOW.md`
- `Docs/Intent/GameFlow/PragmataGameFlowConfig.md`

## 관련 커밋

- `8fa8e56f feat: 6자리 listen session 계약 보강`
- `9a4fed8e fix: listen server role replication 보강`
- `ebd46c4b fix: packaged LAN join 포트 광고 수정`

## 확실한 근거

- `PragmataSessionUtils.cpp`에 room code alphabet, normalize, travel URL helper가 모여 있음
- `PragmataMultiplayerSessionSubsystem.cpp`가 host listen open level option과 join travel URL을 이 helper로 생성
- `PragmataSessionUtilsTests.cpp`가 room code와 travel URL contract를 검증

## 추정한 의도

LAN/session 테스트 중 role, room code, map travel option이 call site마다 달라져 생기는 join 실패를 줄이려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Steam/EOS 같은 외부 online subsystem으로 바꿀 때도 같은 URL option contract를 유지할지 여부.

## 검증 방법

```powershell
rg -n "PragmataSessionUtils|BuildLobbyListen|BuildGameTravelURL|RoomCode" Source Docs
```

## 다음 수정 시 주의점

host 최초 listen open과 lobby server travel을 같은 API로 합치지 말 것.
