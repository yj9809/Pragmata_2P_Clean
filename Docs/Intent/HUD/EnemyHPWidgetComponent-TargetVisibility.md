# EnemyHPWidgetComponent Target 표시 규칙

## 목적

머리 위 HP bar는 체력 수치와 별개로 monster가 현재 전투에 참여했는지를 보여준다. `AMonsterBase::TargetActor`가 유효할 때만 표시한다.

## 흐름

1. 서버에서 `TrySetTargetIfPerceived` 또는 `SetTerritoryTarget`이 `TargetActor`를 설정
2. 서버는 즉시, 클라이언트는 `OnRep_TargetActor`에서 `ApplyTargetHPBarVisibility` 실행
3. `UEnemyHPWidgetComponent`가 저장한 target 표시 상태를 `UEnemyHPWidget::SetEnemyHPVisible`에 전달
4. Target 해제와 사망은 같은 경로로 bar를 숨김

Sentinel은 Target 설정 성공 시 `APragmataGameState::ActiveBossTarget`에도 자신을 등록한다. 이 값은 복제되어 양쪽 HUD의 `UBossHPWidget`에 전달되며, Target 해제와 사망 시 `nullptr`로 되돌린다.

## Territory 연동

Appearance가 끝난 monster와 Runtime Spawn monster는 Territory가 Target을 할당한 뒤 표시한다. First Entry Sequence monster는 sequence 완료 뒤 `StartMonsterAIWithCurrentTarget`에서 Target을 할당하므로 sequence 재생 중에는 표시되지 않는다.

## 구분

`UBossHPWidget`은 HUD에 배치되는 보스 전용 UI이고, `UEnemyHPWidgetComponent`는 머리 위 월드 UI다. Sentinel은 두 UI 모두 같은 Target 설정·해제 기준을 사용한다.
