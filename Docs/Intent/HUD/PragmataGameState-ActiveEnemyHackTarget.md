# APragmataGameState::ActiveEnemyHackTarget 의도 카드

## 대상

`APragmataGameState::ActiveEnemyHackTarget`

## 종류

Replicated enemy hack HUD signal

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`

## 이 코드가 존재하는 이유

현재 enemy hack 대상 actor를 양쪽 클라이언트 HUD에 같은 신호로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack 시작 시 HUD를 target에 bind해야 하는 경우
- enemy hack 종료 또는 hold 후 HUD를 닫아야 하는 경우

## 사용하면 안 되는 상황

- map puzzle HUD 신호
- puzzle 내부 cursor/grid state 저장

## 기본 흐름

server `SetActiveEnemyHackTarget` -> replicated property 변경 -> `OnRep_ActiveEnemyHackTarget` -> PlayerController/HUD bind

## 이 코드를 호출하는 쪽

- `UDianaHackComponent`

## 이 코드가 호출하는 쪽

- `OnActiveEnemyHackTargetChanged` delegate

## 서버/클라이언트 조건

server가 값을 설정하고 client는 OnRep/delegate로 표시한다.

## Blueprint 연결 여부

`BlueprintReadOnly` replicated property. root HUD Blueprint event로 이어짐.

## 헷갈리기 쉬운 코드

- `ActiveMapPuzzleActor`: map hack HUD 신호
- `UDianaHackComponent::ActiveHackTarget`: server component 내부 상태

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/Systems/Server/server.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `5fe4d726 feat: 해킹 UI 서버 권위 통합`
- `f830bae4 fix: enemy hack UI 타깃 갱신 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 `ActiveEnemyHackTarget`은 root HUD C++ 기본 경로가 enemy hacking ViewModel로 연결한다고 기록

## 추정한 의도

HUD가 Diana local state가 아니라 GameState replicated state를 기준으로 열리게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint root HUD event graph의 정확한 node 연결.

## 검증 방법

```powershell
rg -n "ActiveEnemyHackTarget|OnActiveEnemyHackTargetChanged" Source Docs
```

## 다음 수정 시 주의점

완료 animation 때문에 clear 지연이 필요한 흐름을 즉시 null 처리로 깨지 않기.
