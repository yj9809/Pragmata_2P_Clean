# APragmataPlayerController::EnsureGameplayHUD 의도 카드

## 대상

`APragmataPlayerController::EnsureGameplayHUD`

## 종류

Role-based root HUD creator

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`

## 이 코드가 존재하는 이유

assigned role에 맞는 root HUD widget을 한 번 생성하고, hack state와 shared stats binding의 기준 객체로 보관하기 위해 존재한다.

## 사용해야 하는 상황

- PlayerController가 possession, role 변경, BeginPlay 이후 HUD가 필요한 경우

## 사용하면 안 되는 상황

- 개별 widget이 자체적으로 root HUD를 새로 만들 때
- temporary combat popup이나 one-shot effect widget 생성

## 기본 흐름

role별 HUD class 선택 -> soft class load -> `CreateWidget` -> viewport 추가 -> hack/shared stats bind

## 이 코드를 호출하는 쪽

- PlayerController lifecycle과 role change 처리

## 이 코드가 호출하는 쪽

- `TryGetGameplayHUDClassForAssignedRole`
- `BindGameplayHackState`
- `RebindSharedHUDStats`

## 서버/클라이언트 조건

local PlayerController UI 생성 경로. gameplay state는 server replicated data를 구독한다.

## Blueprint 연결 여부

HUD class는 Blueprint widget class soft reference. `WBP_HughHUD` 등 asset scan으로 확인 가능.

## 헷갈리기 쉬운 코드

- `UPragmataHUDWidget::BindSharedHUDStats`: root HUD 내부 data binding
- `HandleActiveEnemyHackTargetChanged`: 이미 생성된 HUD에 state 전달

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Systems/Server/server.md`

## 관련 커밋

- `b37dda22 feat: Hugh 공유 stats 추가`
- `5fe4d726 feat: 해킹 UI 서버 권위 통합`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 root HUD C++ 기본 경로와 shared stat binding 경로 기록

## 추정한 의도

role별 HUD 생성과 replicated gameplay state 구독을 PlayerController에 집중시키려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

role 전환 중 HUD 재생성 정책 전체.

## 검증 방법

```powershell
rg -n "EnsureGameplayHUD|TryGetGameplayHUDClassForAssignedRole|BindGameplayHackState" Source Docs
```

## 다음 수정 시 주의점

widget Blueprint에서 root HUD를 중복 생성하지 않게 한다.
