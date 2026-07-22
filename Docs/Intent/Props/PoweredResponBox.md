# APoweredResponBox

## 목적

전원 상태와 player overlap을 함께 확인해 열리는 map response box다. 필요할 때만 열림 애니메이션 완료 뒤 일정 시간 후 actor를 제거한다.

## Open Cleanup 설정

- `Remove After Opening`: 체크하면 열림 애니메이션 완료 후 제거 예약
- `Removal Delay After Opening`: 완료 뒤 제거까지의 대기 시간. `0`이면 애니메이션 완료 직후 제거
- 제거 결정과 `Destroy()`는 server만 실행. 복제 actor 삭제로 client도 함께 사라짐

## 규칙

- 닫힘 애니메이션 완료나 열림 중 전원 차단으로 닫힌 경우에는 제거하지 않음
- 옵션을 끄면 기존처럼 제거하지 않고 전원·근접 상태에 따라 동작
- Territory 활성화는 기존 `OpenDoor()` 시점에 유지하며, 제거 예약과 분리

## 관련 코드

- `Source/Pragmata_2P/Props/PoweredResponBox.h`
- `Source/Pragmata_2P/Props/PoweredResponBox.cpp`
- `Source/Pragmata_2P/Props/DoorBase.cpp`
