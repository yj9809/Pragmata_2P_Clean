# AMonsterBase LunafilamentReward 의도 카드

## 대상

`AMonsterBase::SetDead`, `LunafilamentReward`

## 이유

모든 원인의 authoritative monster death가 shared Hugh 재화에 한 번만 보상을 지급하게 한다.

## 규칙

- C++ 기본값 `50`, enemy Blueprint별 `EditDefaultsOnly` override 가능
- `0`은 무보상, 음수는 property clamp로 차단
- `bDeathHandled`를 먼저 설정하므로 death delegate/destroy 중복 지급 없음
- OverDrive timed hack, puzzle 완료, client presentation은 보상 경로가 아님

## 주의

`SetDead`의 server branch 밖에서 재화를 지급하지 않는다.
