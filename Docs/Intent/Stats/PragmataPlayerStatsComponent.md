# UPragmataPlayerStatsComponent 의도 카드

## 대상

`UPragmataPlayerStatsComponent`

## 종류

Shared Hugh player stat replication component

## 정의 위치

- `Source/Pragmata_2P/Components/PragmataPlayerStatsComponent.h`
- `Source/Pragmata_2P/Components/PragmataPlayerStatsComponent.cpp`
- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh의 재화, damage multiplier, repair cartridge처럼 Hugh와 Diana HUD가 같이 봐야 하는 player stat을 replicated component 하나로 묶기 위해 존재한다.

## 사용해야 하는 상황

- 양쪽 플레이어 HUD에 같은 Hugh resource를 보여야 할 때
- repair cartridge 사용/획득을 서버에서 확정해야 할 때
- Hugh weapon damage multiplier를 shared stat으로 적용해야 할 때
- UI가 Lunafilament 총액과 획득 연출 delta를 분리해서 받아야 할 때

## 사용하면 안 되는 상황

- enemy health나 enemy-specific stat
- Diana 전용 OverDrive처럼 Diana component가 이미 원본인 값
- local HUD 임시 표시값 저장소
- UI Widget 내부의 currency 원본 변수

## 기본 흐름

server stat 변경 -> replicated property update -> `OnRep_*` 또는 server broadcast -> HUD ViewModel bind -> Hugh/Diana root HUD 동시 갱신

Lunafilament는 신호를 둘로 나눈다.

- `OnLunafilamentChanged(NewTotal)`: authoritative total 변경. 초기 표시, 소비/감소, 강제 보정에 사용
- `OnLunafilamentGained(Delta, OldTotal, NewTotal)`: 획득 연출용. UI의 `+N` merge/animation trigger에 사용

## 이 코드를 호출하는 쪽

- `AHughCharacter`
- `AWeaponBase`
- `UPragmataHUDViewModel`
- gameplay pickup/repair flow

## 이 코드가 호출하는 쪽

- `UPragmataHealthComponent::RecoverHP`
- replicated delegates: `OnLunafilamentChanged`, `OnLunafilamentGained`, `OnDamageMultiplierChanged`, `OnRepairCartridgeChanged`

## 서버/클라이언트 조건

stat mutation은 owner actor authority가 있는 server에서만 수행한다. client는 replicated value와 delegate로 UI를 갱신한다.

## Blueprint 연결 여부

`BlueprintSpawnableComponent`, `BlueprintPure`, `BlueprintCallable`, `BlueprintAssignable`로 Blueprint에서 읽고 호출 가능하다.

## 헷갈리기 쉬운 코드

- `UPragmataHealthComponent`: HP 원본
- `UOverDriveComponent`: Diana OverDrive 원본
- `UPragmataHUDViewModel`: HUD binding target
- UI 검증용 `0` 키 임시 trigger: gameplay reward source가 아니라 수동 테스트 진입점

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Systems/Server/server.md`

## 관련 커밋

- `b37dda22 feat: Hugh 공유 stats 추가`

## 확실한 근거

- `PragmataPlayerStatsComponent.cpp`가 모든 mutation 함수에서 `HasServerAuthority()`를 확인
- `ConsumeRepairCartridge`는 `RecoverHP` 성공 시에만 cartridge를 차감
- `Docs/KNOW_HOW.md`가 Hugh/Diana HUD shared stat source를 GameState role pointer 기준으로 기록
- `AddLunafilament`는 서버에서만 증가하고, overflow는 `MAX_int32`로 clamp
- `OnRep_Lunafilament`는 이전 복제값보다 증가했을 때 client에서도 `OnLunafilamentGained`를 복원

## 추정한 의도

Diana player 화면에서도 Hugh의 생존/자원 상태를 같은 값으로 보여야 하는 2인 협동 UI 요구를 충족하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Lunafilament 획득/소비 gameplay source가 어디까지 확정됐는지 여부. 현재 `0` 키 trigger는 UI 수동 검증용 임시 경로이며 최종 보상 source가 아니다.

## 검증 방법

```powershell
rg -n "PragmataPlayerStatsComponent|RepairCartridge|PlayerDamageMultiplier|Lunafilament" Source Docs
```

## 다음 수정 시 주의점

client convenience에서 stat을 직접 바꾸지 말고 server mutation 함수만 통과시킨다.
UI는 `UPragmataHUDViewModel::Lunafilament`를 animated count에 직접 묶지 않는다.
획득 연출은 `UPragmataHUDViewModel::OnLunafilamentGainedForUI`와 1초 merge window를 사용하고, 표시용 총액은 WBP presentation cache로만 보유한다.
