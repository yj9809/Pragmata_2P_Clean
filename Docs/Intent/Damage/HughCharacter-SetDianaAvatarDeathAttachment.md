# AHughCharacter::SetDianaAvatarDeathAttachment 의도 카드

## 대상

`AHughCharacter::SetDianaAvatarDeathAttachment`

## 종류

Diana avatar death/respawn attachment boundary

## 정의 위치

- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh death가 확정된 frame에 Diana avatar를 `DianaMountSocket_root_death`로 옮기고, 이후 respawn reset이 normal socket으로 같은 경계를 통해 복귀하게 하기 위해 존재한다.

## 사용해야 하는 상황

- `HandleDeath`가 death montage 전에 death attachment를 적용하는 경우
- checkpoint respawn reset이 normal attachment를 복구하는 경우

## 사용하면 안 되는 상황

- Diana operator pawn possession 변경
- Diana IK alpha 또는 paired animation cue 제어

## 서버/클라이언트 조건

attachment는 Hugh replicated avatar mesh presentation에 적용한다. server authority와 proxy 모두 같은 socket asset 이름을 사용해야 한다.

## 다음 수정 시 주의점

death socket 누락을 normal socket으로 조용히 대체하지 않는다. 누락 시 log와 mesh-root fallback이 발생하므로 skeletal mesh socket을 수정해 해결한다.
