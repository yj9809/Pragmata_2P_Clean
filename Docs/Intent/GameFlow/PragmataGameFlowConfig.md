# UPragmataGameFlowConfig 의도 카드

## 대상

`UPragmataGameFlowConfig`

## 종류

Project game flow soft reference config

## 정의 위치

- `Source/Pragmata_2P/Data/PragmataGameFlowConfig.h`
- `Source/Pragmata_2P/Data/PragmataGameFlowConfig.cpp`

## 이 코드가 존재하는 이유

title, lobby, gameplay map과 mode class 흐름을 Blueprint class default와 project setting에서 조정 가능하게 하기 위해 존재한다.

## 사용해야 하는 상황

- title 이후 map travel 흐름을 바꾸는 경우
- packaged build에서 soft-loaded map/mode ownership을 확인해야 하는 경우

## 사용하면 안 되는 상황

- 임시 PIE map 전환만을 위해 hard-coded path를 추가하는 경우
- runtime asset cook 문제를 코드 hard load로 우회하는 경우

## 기본 흐름

project setting의 GameFlow class 확인 -> config object에서 map/mode soft path 읽기 -> title/lobby/server flow에서 travel 또는 class load

## 이 코드를 호출하는 쪽

- `APragmataLobbyGameMode`
- title/lobby 관련 flow

## 이 코드가 호출하는 쪽

- soft path getter와 Unreal asset loading 경로

## 서버/클라이언트 조건

server travel과 packaged runtime asset ownership에 영향.

## Blueprint 연결 여부

`BP_GameFlow_Default` class default가 핵심 연결점.

현재 gameplay map은 `LunarMap_T`다. `BP_GameFlow_Default`, C++ fallback `LobbyGameMap`, `MapsToCook`가 모두 같은 map을 가리켜야 한다.

## 헷갈리기 쉬운 코드

- `UPragmataGameFlowSettings`: project setting에서 GameFlow class를 고르는 설정
- map `WorldSettings`: 현재 열린 map의 local 설정

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Operations/packaging-guide.md`
- `Docs/Systems/Server/server.md`

## 관련 커밋

- `9754a074 fix: DirectMap GameFlow 제거`
- `b9a419fa feat: Blueprint 편집 가능한 GameFlow 추가`
- `588d2287 fix: packaged soft load cook 보장 추가`

## 확실한 근거

- `Docs/KNOW_HOW.md` Title GameFlow recipe에 class default와 Project Settings 경로 기록
- packaging 관련 docs에 `PAL_CoreRuntime` cook owner 기록

## 추정한 의도

map flow를 코드 상수 대신 Editor에서 바꿀 수 있게 하되, packaged build cook ownership을 명시적으로 관리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

팀이 최종 release에서 GameFlow config를 어느 asset으로 고정할지.

## 검증 방법

```powershell
rg -n "UPragmataGameFlowConfig|GameFlowClass|BP_GameFlow_Default|MapsToCook|PAL_CoreRuntime" Source Config Content Docs
```

## 다음 수정 시 주의점

GameFlow gameplay map을 바꾸면 `BP_GameFlow_Default`, C++ fallback, `MapsToCook`를 함께 바꾸고 이전 map의 referencer가 없는지 확인. 새 runtime asset soft reference를 추가하면 `PAL_CoreRuntime` 또는 `MapsToCook` coverage를 같이 검증.
