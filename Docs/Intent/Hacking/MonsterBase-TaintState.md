# AMonsterBase 오염 state 의도 카드

## 대상

`AMonsterBase::bIsTainted`, `AMonsterBase::CleanseTaint`, `AMonsterBase::OnRep_Tainted`

## 종류

Persistent enemy visual state

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 이 코드가 존재하는 이유

enemy가 기본적으로 오염된 visual 상태로 시작하고, Diana 정화 스킬을 맞았을 때만 그 상태를 해제하기 위해 존재한다. `bHacked`는 timed hack/combat presentation이고, `bIsTainted`는 정화 스킬 전용 persistent 오염 visual state다. server가 enemy hack grid를 만들 때도 이 값만 보고 Corrupted(보라색) node를 배치한다.

## 사용해야 하는 상황

- 오염된 enemy visual을 Blueprint/AnimBP에서 읽을 때
- Diana 정화 스킬 cone 안의 enemy를 정화할 때
- server에서 정화 결과를 client presentation에 복제할 때

## 사용하면 안 되는 상황

- enemy hack 성공 여부나 hacked damage window를 표현하는 용도
- hack duration timer를 대체하는 용도
- 일반 damage, death, AI combat state를 판정하는 용도

## 기본 흐름

spawn/default: `bIsTainted=true` -> Diana 정화 스킬 hit -> `CleanseTaint()` server authority 확인 -> `bIsTainted=false` -> `HandleTaintStateChanged()` -> client `OnRep_Tainted()`

`SM_Sentinel`, `SM_Walker_Tainted`, `SM_Executor_Tainted`는 서버 `BeginPlay()`에서 Blueprint default override가 있어도 `bIsTainted=true`로 시작한다. 이 초기화 뒤 false로 전환하는 경로는 `CleanseTaint()`뿐이다.

`SM_Sentinel`, `SM_Walker_Tainted`, `SM_Executor_Tainted`는 false 전환에서 지정된 taint material slot을 `M_Invisible`로 교체한다. 이후 hack material 갱신도 taint visibility를 다시 적용하므로 정화된 외형이 복구되지 않는다.

## 이 코드를 호출하는 쪽

- `UDianaCleanseComponent::ApplyCleanseToTarget`
- Blueprint/AnimBP의 `IsTainted()` 조회

## 이 코드가 호출하는 쪽

- `HandleTaintStateChanged`
- `ReceiveTaintStateChanged`
- `ForceNetUpdate`

## 서버/클라이언트 조건

`bIsTainted` 변경은 server authority에서만 수행한다. client는 replicated value와 `OnRep_Tainted()`를 통해 visual presentation만 갱신한다.

## Blueprint 연결 여부

`bIsTainted`는 `BlueprintReadOnly` replicated state이고 `IsTainted()`는 Blueprint/AnimBP 조회용이다. `ReceiveTaintStateChanged()`는 monster별 material, mesh, VFX 갱신을 Blueprint에서 연결하는 event다.

## 헷갈리기 쉬운 코드

- `bHacked`는 timer로 false로 돌아갈 수 있지만 `bIsTainted`는 정화 스킬로 false가 된 뒤 자동 복구하지 않는다.
- Corrupted node 생성은 Blueprint별 별도 bool이 아니라 grid 생성 시점의 `bIsTainted`를 사용한다. 정화 뒤 시작하는 puzzle에는 보라색 node가 없다.
- `CleanseTaint()`는 죽은 enemy에는 적용하지 않는다.
- 오염 해제는 damage 처리가 아니므로 `IPragmataDamageable` 경로를 타지 않는다.

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Hacking/DianaCleanseComponent-RequestCleanse.md`
- `Docs/Intent/Hacking/MonsterBase-HackState.md`

## 관련 커밋

미확인. `git log -S"bIsTainted" -- Source Docs`로 추적.

## 확실한 근거

- `AMonsterBase`가 `bIsTainted`를 `ReplicatedUsing=OnRep_Tainted`로 선언하고, false 전환에서 taint material slot을 `M_Invisible`로 바꾼다.
- `GetLifetimeReplicatedProps()`가 `bIsTainted`를 복제 목록에 포함한다.
- `UDianaCleanseComponent`가 오염된 alive monster만 정화 스킬 대상으로 수집한다.
- `UEnemyHackingPuzzleComponent::InitGrid()`가 owner `IsTainted()`를 확인해 Corrupted node 생성을 결정한다.

## 추정한 의도

오염 state는 enemy의 기본 오염 외형을 표현하고, Diana 정화 스킬이 그 외형을 영구적으로 걷어내는 전용 상호작용이 되게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

monster별 오염된/정화된 material 또는 animation Blueprint 연결 방식은 asset 작업에서 확정해야 한다.

## 검증 방법

```powershell
rg -n "bIsTainted|IsTainted|CleanseTaint|OnRep_Tainted|ReceiveTaintStateChanged" Source Docs
```

## 다음 수정 시 주의점

hack visual을 수정할 때 `bHacked`와 `bIsTainted`를 같은 상태로 합치지 않는다. 두 상태는 duration, reset 조건, 호출 경로가 다르다.
