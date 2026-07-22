# LunarMap_T 이관 완료

- 최종 gameplay map: `LunarMap_T`
- 삭제한 debug map: `LunarMap_LS`
- GameFlow fallback, `BP_GameFlow_Default`, `MapsToCook` 모두 `LunarMap_T` 사용

## 이관 확인

- 여섯 `BP_TutorialTrigger_*`를 동일 transform·TriggerBox extent·DataAsset·`TutorialId`로 이관
- WeaponUse 종료 objective alert 설정 이관
- `BP_DeadFilament_Box`, `BP_BossRouteElevator`, `BP_PuzzleNode_Elevator0`는 T에 이미 같은 class와 transform으로 존재
- `CleanseAcquisitionSequenceCamera`는 현재 non-cinematic Cleanse 획득 경로에서 참조하지 않아 이관하지 않음

## 삭제 전 확인 기준

- `LunarMap_LS` 최초 snapshot 뒤 tracked map 변경은 Cleanse와 tutorial trigger 작업뿐임
- LS-only actor 목록 중 tutorial trigger 여섯 개와 legacy camera만 현재 Hojin 변경 범위
- `LunarMap_T` load, tutorial trigger readback, GameFlow CDO, `MapsToCook`를 확인한 뒤 LS 삭제
