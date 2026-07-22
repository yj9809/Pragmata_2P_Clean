# APuzzleInteractable 반복 해킹 제한

## 대상

`APuzzleInteractable::CanInteractWithPuzzle`, `CompletePuzzle`, `ResetPuzzleForNextAttempt`

## 규칙

- 기본값 `Allow Repeated Hacking=false`: 한 번 성공하면 영구 완료
- 체크한 퍼즐만 `Max Hack Count` 횟수까지 재해킹 가능
- 반복 퍼즐도 성공 직후에는 `bIsSolved=true`로 닫힘
- progression actor가 `ResetPuzzleForNextAttempt()`를 호출해야 다음 횟수로 재무장
- 최대 횟수에 도달하면 Reset 호출이 와도 다시 열리지 않음

## Diana 후보 판정

`UDianaNodeInteractionComponent`의 `LocallyCompletedActors`는 일반 일회성 POI cache다. `APuzzleInteractable`은 이 cache보다 복제된 `CanInteractWithPuzzle()` 결과를 우선 사용한다. 따라서 서버의 재무장 상태가 복제되면 Diana client도 같은 노드를 다시 해킹 후보로 찾는다.

## 관련 코드

- `Source/Pragmata_2P/Props/PuzzleInteractable.h`
- `Source/Pragmata_2P/Props/PuzzleInteractable.cpp`
- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp`
- `Docs/Systems/Hacking/map-hacking-integration.md`
