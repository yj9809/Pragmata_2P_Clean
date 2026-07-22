# Pending Cook 제외 목록

## Demo map

- 대상: `/Game/Map/Demo`
- 상태: Editor 전용 테스트 map
- 현재 처리: `MapsToCook`에서 제외
- `ServerDefaultMap`: `/Game/Map/LunarMap_T`로 변경
- 제외 이유: `WBP_UpgradeMenuWidget` WIP Blueprint를 직접 참조하며 package cook을 중단
- 확인 결과: Demo를 제외해도 WIP Upgrade Widget Blueprint compile 오류는 cook 중 계속 발생
- 재포함 조건:
  - Upgrade Widget Blueprint compile 성공
  - C++ `UUpgradeItemRowWidget`, `UPragmataUpgradeMenuWidget` Blueprint 노출/타입 경계 검증
  - `MapsToCook`에 Demo 재추가 후 BuildCookRun 성공
- 추가 처리: `/Game/UI/Widget/Upgrade`를 `DirectoriesToNeverCook`으로 제외
- 최근 검증: `LANPlaytest-Win64-20260710-201229` BuildCookRun 성공
- 주의: 다른 runtime map이 Upgrade Widget을 참조하면 package cook 전에 그 reference를 제거하거나 Widget을 수정해야 함.
- package 재개 조건: Upgrade Widget compile 오류를 수정하고 `DirectoriesToNeverCook` 항목을 제거한 뒤 BuildCookRun 성공
