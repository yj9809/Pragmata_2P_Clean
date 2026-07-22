# UEnemyHackingPuzzleComponent Decode 노드 의도 카드

## 대상

`EHackNodeType::Decode`, `UEnemyHackingPuzzleComponent::IsDecodeNodeVisited`, `bDecodeNodeVisited`

## 종류

Enemy hack optional defense-break node

## 정의 위치

- `Source/Pragmata_2P/Data/PragmataTypes.h`
- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

enemy hack grid 안에 선택 목표를 하나 더 두고, Diana가 해당 노드를 경로에 포함했는지 서버 기준으로 판정하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack 완료 시 방어력 감소 보너스 여부를 판정하는 경우
- HUD나 Widget Blueprint가 현재 셀을 `Decode` 노드로 표시하는 경우

## 사용하면 안 되는 상황

- 파란 노드 수 기반 hacked duration 계산
- map hack puzzle
- client-only UI 예측으로 gameplay bonus 확정

## 기본 흐름

`InitGrid()`가 시작 커서와 특수 노드를 제외한 빈 칸에 `Decode` 노드 1개 배치 -> `MoveCursor()`가 진입 시 `bDecodeNodeVisited=true` -> 한 칸 백트래킹으로 경로에서 빠지면 false -> 완료 시점 값으로 decode 성공 여부 판정

## 서버/클라이언트 조건

서버가 배치와 방문 상태를 확정하고 `bDecodeNodeVisited`를 복제한다. client와 Widget Blueprint는 복제 상태를 표시하거나 완료 결과를 읽는다.

## Blueprint 연결 여부

`IsDecodeNodeVisited()`는 `BlueprintPure`다. Blueprint는 방어력 감소 판정 자체를 소유하지 않고, 표시나 완료 결과 확인에 사용한다.

## 헷갈리기 쉬운 코드

- `NormalNodesVisited`: duration tier 계산용 파란 노드 수
- `bCorruptedPending`: corrupted node 입력 확인 상태
- `GetProjectedDamage()`: 기본 hack damage 표시

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-ProcessDirectionInput.md`
- `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-CompleteHack.md`

## 검증 방법

```powershell
rg -n "Decode|IsDecodeNodeVisited|bDecodeNodeVisited" Source Docs
```

## 다음 수정 시 주의점

- `Decode` 노드를 시작 커서 `(0,0)`에 배치하지 않는다. 시작 칸은 `MoveCursor()` 진입 이벤트가 없어 live flag가 켜지지 않는다.
- 방어력 감소를 client UI 상태로 확정하지 않는다. 완료 시점의 서버 replicated 값만 gameplay 판정에 사용한다.
