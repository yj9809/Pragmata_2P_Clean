# MatchLobby Hugh 포커스 애니메이션 수정 계획

## 목표

Hugh에서 Diana로 포커스를 전환하는 동안 preview skin 갱신이 발생해도 Hugh와 Diana가 Diana 포커스용 start/loop 애니메이션을 유지하도록 수정.

## 확인된 원인

- `APragmataLobbyPlayerController::UpdateLobbyPresentationFocus`는 skin 갱신 뒤 `FocusRole`을 호출한다. `Source/Pragmata_2P/Game/PragmataLobbyPlayerController.cpp:385`
- 수정 전 `ALobbyPresentationDirector::SetPreviewSkin`은 mesh 교체 후 현재 `FocusedRole` 기준 sequence를 다시 재생했다. `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp:180`
- camera 전환 중에는 `FocusedRole`이 이전 role로 남고 `TransitionTargetRole`만 새 role을 가진다. `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp:92`, `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp:309`
- 수정 전에는 같은 target으로 이미 전환 중이면 `FocusRole`이 조기 반환해 stale sequence를 다시 바로잡지 않았다. `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp:119`
- map 배치 `SkeletalMeshActor`의 `Animation to Play`는 focus 전환 원천이 아니다. director가 Single Node start/loop sequence를 직접 지정한다. `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp:368`

## 수정 범위

- `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp`
- `Docs/KNOW_HOW.md`
- `Docs/Intent/Lobby/LobbyPresentationDirector-FocusRole.md`

`Content/Map/MatchLobby.umap`, animation asset, skin DataAsset, PlayerController 호출 순서는 수정하지 않음.

## 완료 조건

- [ ] skin 교체 없는 Hugh -> Diana 전환에서 Hugh preview가 Diana 포커스용 start animation을 1회 재생한 뒤 loop animation 유지
- [ ] Diana -> Hugh 전환도 Hugh 포커스용 start/loop 유지
- [ ] 전환 도중 local 또는 replicated skin 갱신이 발생해도 두 preview mesh가 `TransitionTargetRole` 기준 sequence 유지
- [ ] 같은 skin id가 반복 전달될 때 mesh와 start animation을 재설정하지 않음
- [ ] 실제 skin mesh 교체 직후에만 두 preview에 현재 presentation target의 start sequence와 loop timer를 한 번 재적용
- [ ] 기존 camera 방향, role 선택, skin 선택, ready/start 흐름에 변화 없음
- [ ] `MatchLobby.umap`과 animation asset에 불필요한 변경 없음

## 구현 단계

### 1. 전환 중 presentation role 계산 보정

- [x] `ALobbyPresentationDirector::SetPreviewSkin`에서 mesh 교체 여부를 `bMeshChanged`로 먼저 계산
- [x] 실제 `SetSkeletalMesh` 호출 시 animation dirty 상태 기록
- [x] 모든 role별 skin 갱신 뒤 `FocusRole`에서 현재 target sequence를 두 preview에 한 번 재적용
- [x] 같은 stable role 또는 같은 transition target 조기 반환 전에 dirty 상태 복구
- [x] 같은 mesh 또는 같은 skin id 갱신은 dirty 상태를 만들지 않아 start animation 반복 방지

적용 형태:

```cpp
const bool bMeshChanged = TargetMeshComponent->GetSkeletalMeshAsset() != SkinMesh;
if (bMeshChanged)
{
    TargetMeshComponent->SetSkeletalMesh(SkinMesh);
    bPreviewAnimationInvalidatedBySkinChange = true;
}

CurrentSkinId = SkinId;
```

`FocusRole`은 같은 presentation target으로 조기 반환하기 전에 dirty 상태를 확인하고 `PlayPreviewSequence(NewFocusedRole)`을 한 번 호출한다.

### 2. 코드 의도와 유지보수 규칙 갱신

- [x] `SetPreviewSkin`의 Korean intent comment를 갱신해 `SetSkeletalMesh` 이후 dirty 상태를 기록하는 이유 명시
- [x] `Docs/Intent/Lobby/LobbyPresentationDirector-FocusRole.md`에 모든 skin 갱신 뒤 `FocusRole`이 sequence를 재적용하는 규칙 추가
- [x] `Docs/KNOW_HOW.md`의 기존 Role Lobby recipe에 skin 교체와 focus animation ownership 순서 추가
- [x] map actor의 기본 animation 설정이 runtime focus sequence를 대체하지 않는다는 주의 유지
- [x] comment coverage gate에서 수정된 `FocusRole`, `SetPreviewSkin` intent comment 확인

### 3. 정적 검증

- [x] `git diff --check`
- [x] 변경 파일이 director source와 관련 문서로 제한됐는지 `git status --short` 확인
- [x] `SetPreviewSkin`의 모든 호출 경로가 같은 skin id 반복 호출과 실제 mesh 교체를 모두 통과하는지 검색
- [x] fallback Hugh/Diana focus start/loop asset 경로가 그대로 유지되는지 확인
- [x] runtime soft-load asset을 새로 추가하지 않았으므로 PAL 변경이 불필요한지 확인

### 4. Unreal 검증

- [ ] 사용자 승인 후 최소 Editor target build 수행
- [ ] MatchLobby 2-player PIE에서 두 player가 CharacterSelect phase에 진입
- [ ] Hugh -> Diana -> Hugh를 천천히 전환하고 각 방향에서 start 1회 후 loop 유지 확인
- [ ] skin 교체 없는 Hugh -> Diana -> Hugh 전환에서 각 방향의 start가 1회만 실행된 뒤 loop 유지 확인
- [ ] 한 viewer가 Hugh -> Diana 전환을 시작한 0.65초 안에 다른 player가 Hugh skin을 DelphiSuit <-> ScribbleSuit로 실제 교체
- [ ] 위 교체 직후 두 preview가 Diana target start를 한 번 다시 적용한 뒤 loop로 전환하고 Hugh가 reference/rest pose로 돌아가지 않는지 확인
- [ ] host/client 역할을 바꿔 같은 replicated Hugh skin overwrite 시나리오 반복
- [ ] Diana는 단일 skin이므로 Diana mesh overwrite 대신 일반 Diana -> Hugh role 전환 회귀만 확인
- [ ] Output Log에서 `LobbyPresentationDirector::LoadAnimation` missing warning과 animation/skeleton warning 부재 확인
- [ ] PIE 종료 후 `MatchLobby.umap`과 animation asset이 dirty 상태가 되지 않았는지 확인

## 위험과 대응

| 위험 | 대응 |
| --- | --- |
| replicated skin refresh마다 start animation 재시작 | 실제 mesh 교체 시에만 dirty 상태 기록 |
| 여러 skin write 사이에서 animation 복구 순서 역전 | 모든 skin 갱신 뒤 `FocusRole`에서 두 preview를 한 번 복구 |
| 동일 target 조기 반환으로 복구 누락 | 조기 반환 전에 dirty 상태만 소비 |
| map actor animation 설정으로 우회해 다시 덮어쓰기 발생 | runtime animation ownership을 director로 유지하고 문서화 |
| 수정 과정에서 사용자 작업 중인 `MatchLobby.umap` 변경 | source/docs만 편집하고 PIE 종료 후 asset dirty 상태 확인 |

## 제외 범위

- `ULobbyPreviewAnimationSet` DataAsset 신규 생성
- focus animation asset 교체 또는 retarget
- camera framing 변경
- lobby skin 종류 추가
- `MatchLobby.umap` actor 구성 변경
