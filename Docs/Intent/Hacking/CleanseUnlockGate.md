# Cleanse unlock gate 의도 카드

## 대상

`APragmataPlayerState::bCleanseUnlocked`, `UDianaCleanseComponent::CanStartCleanse`

## 규칙

- Diana cleanse는 PlayerState unlock 전 모든 mode/preview/RPC 경로에서 거부
- unlock은 server가 acquisition prop 완료 시 한 번만 설정
- PlayerState replication으로 death/checkpoint respawn 뒤에도 유지

## 획득 연출 경계

`ACleanseSkillAcquisitionProp`가 `BP_DeadFilament_Box`의 native parent다. 서버가 Diana 상호작용을 검증하면 `bAcquired`를 먼저 확정하고 Diana PlayerState의 `UnlockCleanse()`를 즉시 호출한다. 이어 Diana owning client에 `UTutorialBlueprintLibrary::ShowTutorialOnce(..., "CleanseAcquisition", ...)`를 요청하고, `AAnimatedPropInteraction::Interact_Implementation()` 경로로 기계 상호작용 애니메이션을 재생한다.

정화 획득에는 Level Sequence, Montage, 별도 camera, possession 변경, Hugh 이동이 없다. `LS_CleanseAcquisition`은 향후 참고용 `.uasset`으로만 보존하며 런타임 획득 흐름에서 로드하거나 재생하지 않는다.

- 획득 prop은 scan POI가 아니며 local controller가 Hugh 위치에서 2000cm 이내일 때만 prop-owned `WBP_ScanIconWidget`을 표시한다.
- 상호작용 후보는 `Visibility` camera ray의 실제 surface hit로만 생성한다. `ACleanseSkillAcquisitionProp::BeginPlay()`는 Blueprint의 `NoCollision` 저장값이 이 계약을 끊지 않도록 `AnimatedMesh`를 `QueryOnly`, `Visibility=Block`으로 보정한다.
- tutorial은 unlock 직후 Diana owning controller RPC에서 `ShowTutorialOnce`로만 표시한다.
- Diana HUD의 `OverDriveGaugeWidget` purification panel은 같은 PlayerState unlock 복제값으로 unlock 전 `Collapsed`, unlock 후 표시한다.
- prop은 `bAcquired`를 복제해 재상호작용과 근접 아이콘을 막고, late join client에서도 focus prompt를 숨긴다.
- 기계 애니메이션은 일반 ItemBox reward 경로를 건너뛰고 `AAnimatedPropInteraction` 구현을 직접 호출하므로 Lunafilament를 지급하지 않는다.
