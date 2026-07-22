# FPragmataDamageSpec 의도 카드

## 대상

`FPragmataDamageSpec`

## 종류

Damage request data

## 정의 위치

- `Source/Pragmata_2P/Data/PragmataTypes.h`

## 이 코드가 존재하는 이유

Unreal 기본 damage float보다 많은 gameplay 정보를 source에서 receiver로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- damage amount 외 instigator, causer, hit result, damage kind, hit direction 같은 정보가 필요한 경우
- hit react와 death animation이 damage context를 필요로 하는 경우

## 사용하면 안 되는 상황

- 단순 UI 표시용 숫자
- 저장 데이터나 network protocol로 장기 보존해야 하는 별도 data

## 기본 흐름

source가 `FPragmataDamageSpec` 생성 -> delivery gate로 전달 -> receiver가 actor별 규칙 적용 -> health component가 최종 적용

## 이 코드를 호출하는 쪽

- weapon damage
- melee notify
- overlap notify
- hack damage

## 이 코드가 호출하는 쪽

없음. data struct다.

## 서버/클라이언트 조건

server damage 처리에서 기준 data로 사용한다.

## Blueprint 연결 여부

미확인.

## 헷갈리기 쉬운 코드

- `FDamageEvent`: Unreal engine damage event. 현재 gameplay damage의 주 경로가 아니다.
- `DamageAmount` 단일 float: `FPragmataDamageSpec`보다 정보가 부족하다.

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/Create/20260703-161329-receive-damage-only-hitreact-refactor.md`

## 관련 커밋

미확인. `git log -SFPragmataDamageSpec -- Source/Pragmata_2P`로 추적.

## 확실한 근거

`Docs/Create/20260703-161329-receive-damage-only-hitreact-refactor.md`에 source가 `FPragmataDamageSpec`을 직접 만든다고 기록.

## 추정한 의도

damage 관련 context를 단일 구조체로 묶어 receiver별 반응이 같은 입력을 보게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

현재 field 전체가 모든 source에서 필요한지 여부.

## 검증 방법

```powershell
rg -n "FPragmataDamageSpec" Source Docs
```

## 다음 수정 시 주의점

field 추가 시 모든 source와 receiver의 의미 변화를 함께 문서화.
