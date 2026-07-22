# Diana POI Scan UI + Blue Scan Effect 계획

## 목표

- Diana `IA_Scan` 입력으로 50m 안 POI의 object-owned screen-space icon을 Hugh/Diana 양쪽 local client에 표시한다.
- icon은 `WBP_ScanIconWidget`과 거리 meter를 표시하며 viewport edge arrow는 만들지 않는다.
- POI는 scan wave 도달 시점에 reveal되고 reveal 뒤 5초 동안 보인다.
- scan 성공 시 POI 개수와 무관하게 Hugh/Diana 양쪽 local client에서 1.5초, 이전 강도의 50% blue fullscreen pulse를 재생한다.
- battle/tense 상태에서는 scan marker와 blue pulse를 실행하지 않는다.

## 구현 범위

- `UDianaScanComponent`
  - 서버 role/action lock/cooldown/enemy combat 검증 유지
  - scan은 `Roaming` + non-combat 상태에서만 허용
  - GameState registry 기반 후보 수집 유지
  - 후보 수, 반경 내 수, 유효 POI 수, 거부 사유 로그 추가
  - scan 성공 시 `Client_ShowPOIScanResults`와 `Client_PlayDianaScanPulse` 호출
- `UPOIScanOverlayWidget`
  - marker pooling, projection, edge clamp, reveal timing 담당
  - 거리 scale은 0m 근처 `1.05`, 50m 기준 `0.65`로 선형 보간
  - `TargetActor` resolve 실패 시 scan 시점 `TargetWorldLocation`으로 표시
  - `/Game/UI/Widget/Scan/WBP_POIScanOverlay` soft class 우선 사용
  - asset 누락 시 native fallback 사용
- `UPOIScanMarkerWidget`
  - `BOX`/`NODE` label, meter text, edge arrow 표시
  - native marker를 overlay `WidgetTree`에 직접 생성
  - 새 texture 없이 native UMG shape/text로 표시
- `APragmataPlayerController`
  - scan pulse client RPC 추가
  - local-only transient post-process volume으로 3초 pulse 재생
  - material blendable 대신 color grading/bloom override로 red/green을 낮추고 blue gain/offset을 올려 씬 표면을 파랗게 밝힘

## 자산

- `/Game/UI/Widget/Scan/WBP_POIScanOverlay`
- `/Game/UI/Widget/Scan/WBP_POIScanMarker`
- scan pulse는 runtime material asset 없이 `APostProcessVolume` 설정으로 처리한다.
- runtime WBP asset은 `PAL_CoreRuntime` cook coverage에 포함해야 한다.

## 검증

- Diana POV에서 `IA_Scan` 입력 로그가 찍힌다.
- `BP_ItemBox`, `BP_PuzzleNode`, `BP_PowerSupplyNode`가 50m 안에서 표시된다.
- 가까운 POI가 먼저 reveal된다.
- 화면 밖 POI marker가 edge에 붙고 방향 arrow와 meter가 보인다.
- opened item box, solved one-shot map node는 재scan 시 표시되지 않는다.
- scan 시작 후 5초에 marker가 사라지고 이후 1초 cooldown 동안 재scan이 거부된다.
- enemy combat active 상태에서는 scan result와 blue pulse가 실행되지 않는다.
- 가까운 POI marker는 크게, 50m 근처 POI marker는 작게 표시된다.
- POI가 0개여도 3초 blue pulse가 재생된다.
- Hugh host / Diana client, Diana host / Hugh client 양쪽 listen server 조합에서 확인한다.
- Editor target build가 통과한다.
