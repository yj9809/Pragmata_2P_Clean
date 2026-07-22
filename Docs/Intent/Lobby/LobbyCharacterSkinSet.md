# LobbyCharacterSkinSet 의도 카드

## 대상

`ULobbyCharacterSkinSet`

`FLobbyCharacterSkinDefinition`

## 종류

Lobby character skin 정의와 role별 skin 순환 규칙

## 정의 위치

- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.h`
- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.cpp`

## 이 코드가 존재하는 이유

Lobby UI, preview mesh, Demo gameplay mesh가 같은 skin id와 label 규칙을 쓰게 하기 위해 존재한다.

## 사용해야 하는 상황

- lobby에서 상하 입력으로 skin을 순환할 때
- preview 또는 gameplay mesh를 skin id로 로드할 때
- DataAsset 생성 전에도 packaged-safe fallback 정의가 필요할 때

## 사용하면 안 되는 상황

- gameplay 중 장비 성능이나 upgrade를 판정할 때
- role 선택 충돌을 판정할 때

## 기본 흐름

role + current skin id -> `GetAdjacentSkinId` -> 서버 `LobbySelectedSkinId` 복제 -> UI label/preview/gameplay mesh 적용

## 확실한 근거

- Hugh fallback skin은 `DelphiSuit`, `ScribbleSuit` 2종
- Diana fallback skin은 `DianaDefault` 1종
- `LoadAllMeshes`는 preview/gameplay 진입 전 mesh load 지연을 줄이기 위해 존재

## 다음 수정 시 주의점

Diana skin mesh가 추가되면 DataAsset과 PAL explicit asset을 먼저 갱신하고, fallback 정의도 같은 id/label 규칙으로 맞춘다.
