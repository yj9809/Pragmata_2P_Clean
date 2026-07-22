# Unreal 빌드 Warning 정리

## 기준

- 빌드 명령
  - `Build.bat Pragmata_2PEditor Win64 Development -Project=Pragmata_2P.uproject -WaitMutex -NoHotReload`
- 마지막 확인일
  - 2026-06-18
- 결과
  - 빌드 성공
  - warning 1개 남음

## Warning 목록

| 파일 | Warning | 의미 |
| --- | --- | --- |
| `Source/Pragmata_2P/Animation/HughAnimInstance.cpp` | `UAnimInstance::CalculateDirection` deprecated | UE가 `UKismetAnimationLibrary::CalculateDirection` 사용을 요구 |

## 왜 고쳐야 하나

- 다음 Engine upgrade에서 compile error로 바뀔 수 있음
- 현재는 warning이지만 UE 메시지가 `before upgrading to the next release`를 명시
- 실제 빌드 실패와 오래된 API warning이 섞이면 원인 파악이 느려짐
- Animation code는 매 프레임 실행되므로 Engine 권장 API로 유지하는 편이 안전

## 수정 방법

현재 코드:

```cpp
Angle = CalculateDirection(HughCharacter->GetVelocity() ,HughCharacter->GetActorRotation());
```

권장 코드:

```cpp
Angle = UKismetAnimationLibrary::CalculateDirection(
	HughCharacter->GetVelocity(),
	HughCharacter->GetActorRotation());
```

확인 사항:

- `Source/Pragmata_2P/Animation/HughAnimInstance.cpp`
  - `#include "KismetAnimationLibrary.h"` 유지
- `Source/Pragmata_2P/Pragmata_2P.Build.cs`
  - `AnimGraphRuntime` dependency 유지
- 수정 후 Editor target build 재실행

검증 기준:

- `HughAnimInstance.cpp`에서 C4996 warning 없음
- `Pragmata_2PEditor Win64 Development` 빌드 성공
