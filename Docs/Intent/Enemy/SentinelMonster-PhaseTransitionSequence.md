# Sentinel phase transition sequence

## 목적

`ASentinelMonster`가 반피 HitReact를 정상 종료한 뒤 선택적으로 보스 전용 `ULevelSequence`를 재생한다.

## 설정

- `bPlayPhaseTransitionSequence`: 기능 사용 여부
- `PhaseTransitionSequence`: `BP_Sentinel` Class Defaults에 지정할 Level Sequence asset
- `PhaseTransitionSequenceBossBindingTag`: 현재 Sentinel actor로 교체할 sequence binding tag. 기본값 `Boss`

Level Sequence asset은 BP가 참조하지만, Sequence Player는 각 peer의 현재 World에서 runtime 생성한다. 맵에 LevelSequenceActor를 미리 배치하지 않는다.

## 동작 흐름

```text
server half-health HitReact 종료
-> Sentinel이 sequence 옵션과 asset 확인
-> NetMulticast로 각 peer에 Sequence Player 생성
-> binding tag를 현재 Sentinel actor로 교체
-> sequence 종료 전 HitReact 공격 잠금 유지
-> server OnFinished
-> 공통 HitReact 종료 처리와 AI 공격 가능 상태 복구
```

## 구분할 코드

- `AMonsterBase::HandleHalfHealthHitReactMontageEnded`: 모든 몬스터의 기본 HitReact 종료·공격 재개 처리
- `ASentinelMonster::HandleHalfHealthHitReactMontageEnded`: Sentinel만 sequence 재생을 먼저 시도하고, 재생하지 않으면 부모 처리로 fallback
- `AMonsterTerritoryActor::FirstEntrySequence`: Territory 진입 연출. 보스 반피 연출과 소유·시작 조건이 다름

## 수정 주의

- sequence가 actor track으로 Sentinel을 제어하면 해당 binding에 tag를 반드시 지정
- sequence를 재생하는 동안 `bHalfHealthHitReactPlaying`을 유지하므로 AI 공격은 시작하지 않음
- sequence asset이 비어 있거나 runtime player 생성에 실패하면 기본 HitReact 종료 흐름으로 즉시 복귀
- 사망하면 진행 중 sequence를 모든 peer에서 중단

## 검증 방법

1. `BP_Sentinel` Class Defaults에서 `Play Phase Transition Sequence`를 켜고 asset과 `Boss` binding tag를 지정
2. 2-player PIE에서 Sentinel 체력을 50% 아래로 내려 HitReact 종료 뒤 두 peer 모두 sequence 재생 확인
3. sequence 중 `bCanAttack`이 false이고, 종료 뒤 다시 공격을 선택하는지 확인
4. sequence 중 Sentinel을 사망시켜 sequence 중단과 boss HUD 해제를 확인
