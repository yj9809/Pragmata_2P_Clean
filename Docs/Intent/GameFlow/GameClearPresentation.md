# Game Clear Presentation

## 대상

- `AMonsterTerritoryActor::CheckTerritoryGameClear`
- `APragmataGameState::BeginGameClearPresentation`
- `APragmataPlayerController::ShowGameClearPresentation`
- `APragmataPlayerController::RequestReturnToTitleFromGameClear`

## 목적

보스 Territory의 마지막 등록 monster 처치 뒤 Hugh와 Diana가 같은 clear 연출을 보고, 어느 한쪽의 확인 입력으로 둘 다 Title 화면으로 돌아가게 한다.

## 흐름

```text
boss Territory server 사망 감지
-> ActiveMonsters 전멸 확인
-> GameState clear 상태 복제
-> 각 peer의 local PlayerController가 입력 잠금과 fade/widget 표시
-> 아무 입력
-> owning controller Server RPC 검증
-> GameState multicast
-> 각 peer session 정리 후 Title OpenLevel
```

## 경계

- `bTriggerGameClearWhenMonstersDefeated`가 켜진 Territory만 clear를 요청한다. 기본값은 꺼짐이다.
- Combat Lock은 문·엘리베이터 봉쇄용이며 clear 조건으로 사용하지 않는다.
- `GameState`는 server 판정과 peer 공통 신호만 담당한다. illustration과 입력은 각 local `PlayerController`가 처리한다.
- clear widget의 soft class와 Title map은 `UPragmataGameFlowConfig`가 소유한다.
- `WBP_GameClear`와 그 illustration은 `PAL_CoreRuntime` explicit asset에 포함해야 한다. Title map은 `MapsToCook`에 이미 포함되어 있다.

## Blueprint 연결

- Runtime layer rule: `FadeImage` is assigned a solid black brush and forced to Canvas ZOrder `100` by `UPragmataGameClearWidget`, so the exit fade covers both text widgets even when its Blueprint brush is empty.
- `GameClearPresentationDelay` is an instance setting on `AMonsterTerritoryActor`. It waits the configured number of seconds after the final registered boss dies; the Territory stays alive while that timer is pending. Its default is `0` seconds.
- Shared transition timing is `0.6` seconds from gameplay to black, `0.8` seconds from black to the ending UI, and `0.5` seconds from a return input to black/Title. The boss-specific delay remains separate.

- `WBP_GameClear` parent: `UPragmataGameClearWidget`
- 선택 BindWidget 이름: `EndingIllustration`, `FinText`, `PressAnyButtonText`, `FadeImage`
- `FadeImage`는 illustration보다 위에 놓고 `Hit Test Invisible` overlay로 사용한다.

## 검증

1. 보스 Territory 인스턴스에서 `Trigger Game Clear When Monsters Defeated`를 켠다.
2. 2-player listen server에서 마지막 monster 처치 뒤 양쪽 화면이 검은색 -> illustration/`FIN.`으로 전환되는지 확인한다.
3. host 또는 client 어느 쪽에서든 아무 입력을 눌러 양쪽 모두 `Title`로 이동하는지 확인한다.
4. 새 host 세션을 만들 때 이전 room code/session이 남지 않는지 확인한다.
