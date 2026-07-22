# UBTTask_ChangeMoveSpeed

## 목적

Behavior Tree에서 monster 이동 상태의 기본 속도를 적용한다. 노드별 연출이나 특수 이동이 필요하면 `Override Move Speed`를 체크해 해당 실행 한 번에만 `Custom Move Speed`를 사용한다.

## 규칙

- 체크 해제: `Move Speed Mode`의 기존 기본값 사용
- 체크: 선택한 상태 기본값을 적용한 뒤 `Custom Move Speed`로 덮어씀
- monster 공용 이동 속도 데이터와 다른 BT 노드는 변경하지 않음

## 관련 코드

- `Source/Pragmata_2P/AI/BTTask_ChangeMoveSpeed.h`
- `Source/Pragmata_2P/AI/BTTask_ChangeMoveSpeed.cpp`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`
