# Checkpoint session 의도 카드

## 흐름

Diana가 checkpoint surface를 click -> server camera ray 재검증 -> activate/replenish -> active checkpoint 재상호작용 시 Hugh upgrade session.

## server 경계

- `ACheckPoint`가 replicated active state, stable `CheckpointId`, map-authored `RespawnAnchor`와 latest checkpoint를 `APragmataGameMode`에 등록
- `APragmataGameMode`가 10초 death respawn timer와 shared invincibility를 소유
- Hugh만 열린 checkpoint session에서 upgrade/refund, selection, hold progress, close RPC를 보낼 수 있음
- `UPragmataPlayerStatsComponent::TryUpgrade/TryRefundUpgrade`는 checkpoint session 밖에서 controller RPC로 실행되지 않음

## client 경계

- Hugh: interactive `WBP_UpgradeMenuWidget`, selection과 F/C hold progress를 GameState에 전달
- Diana: read-only mirror, replicated selection과 hold gauge만 표시. focus/upgrade/refund/close 불가
- checkpoint prompt는 viewport widget이 아닌 `ACheckPoint.HoldPromptComponent` screen-space widget이 소유
- Diana camera ray가 visible collision surface에 정확히 hit할 때 local click guide를 표시한다. inactive/active 모두 hold progress ring을 사용하지 않는다.
- `IA_Interact` Started 입력은 checkpoint hold state나 release 취소 RPC 없이 local hit actor와 최신 camera direction을 reliable RPC로 보낸다. server는 client actor를 신뢰하지 않고 그 direction으로 `Visibility` trace, 거리, POI 등록을 재검증한 뒤 actor가 일치할 때만 즉시 `Interact()`를 호출한다.
- `BP_CheckPoint.AnimatedMesh`는 `BlockAll`, `QueryAndPhysics`, `Visibility=Block`을 유지해 player 이동 차단과 Diana camera trace focus를 같은 collision surface로 처리
- close는 양 menu, action lock, invincibility를 함께 정리
- 10초 respawn timer는 checkpoint session 유무와 무관하게 death cue의 Diana action lock을 명시 해제한다.
- 10초 respawn timer는 같은 Hugh/Diana pawn을 checkpoint 또는 PlayerStart의 위치와 회전으로 옮긴 뒤 Hugh death/anim state를 먼저 reset하고 HP·ammo·cartridge를 보급한다. Diana control rotation, aim/operator mode, mount socket도 함께 reset한다.
