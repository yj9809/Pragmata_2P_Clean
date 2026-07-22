# Diana Title facial blend fix

## 결론

`Title_Diana_Showcase`의 얼굴 animation이 보이지 않았던 직접 원인은 `Layered Blend per Bone` 설정이 Hugh의 정상 동작 패턴과 달랐기 때문.

최종 해결값:

| 항목 | 값 |
| --- | --- |
| Asset | `/Game/Blueprints/Player/ABP_Diana_Title` |
| Base pose | `/Game/Character/Diana/Animations/Title/AS_Diana_Title_Loop_VerB` |
| Blend pose | `/Game/Character/Diana/Animations/Facial/Minidemo/AS_Diana_Face_MiniDemo_md09000_Title_Loop_VerB` |
| Branch root | `Neck_0` |
| Blend Depth | `0` |
| Blend Weight | `1.0` |
| Curve Blend Option | `Override` |
| Mesh Space Rotation Blend | `false` |
| Mesh Space Scale Blend | `false` |
| 이후 처리 | `Local To Component Space` → `RigidBody` hair physics → `Component To Local Space` |

## 필요한 개념

`Layered Blend per Bone`은 특정 bone branch에 다른 pose를 덮는 node다. 이 node는 pose pin이 연결됐다는 이유만으로 항상 100% 적용되지 않는다.

적용 여부는 최소 세 조건이 같이 맞아야 한다.

1. `BlendPoses_0`에 실제 pose가 연결
2. `BlendWeights_0` 또는 내부 `BlendWeights[0]`가 `1.0`
3. `LayerSetup[0].BranchFilters[0]`가 실제 skeleton branch에 충분한 weight를 부여

이번 문제는 pose 연결과 facial animation asset 자체가 정상인데, branch filter와 blend policy가 Hugh의 검증된 방식과 달라 facial pose가 충분히 적용되지 않은 상태였다.

## 잘못된 중간 설정

초기 수정 과정에서 다음 값이 들어갔다.

```text
BranchFilter BoneName = neck_0
BlendDepth = 255
BlendWeights[0] = 1.0
CurveBlendOption = BlendByWeight
Mesh Space Rotation Blend = true
Mesh Space Scale Blend = true
```

이 설정은 "하위 전체 포함"처럼 보이지만, UE 5.8의 실제 weight 계산과 맞지 않는다.

엔진 계산 흐름:

```text
IncreaseWeightPerDepth = (BlendDepth != 0) ? 1 / BlendDepth : 1
BlendWeight at descendant = Clamp((Depth + 1) * IncreaseWeightPerDepth, 0, 1)
```

따라서 `BlendDepth=255`는 `Neck_0` root에서 약 `0.0039`, 바로 아래 bone에서 약 `0.0078`만 적용한다. 얼굴 branch 전체를 선택한다는 의도와 달리 대부분의 face/head motion이 base pose에 거의 묻힌다.

`BlendDepth=0`은 특수하게 `IncreaseWeightPerDepth = 1`이므로 root와 descendants가 즉시 weight `1.0`을 받는다. Hugh의 정상 동작 blend도 이 방식이다.

## Gold standard 비교

기준 asset:

```text
/Game/Blueprints/Player/ABP_HughCharacter
```

Hugh의 정상 blend node:

```text
BranchFilter BoneName = Spine_0
BlendDepth = 0
BlendWeights[0] = 1.0
CurveBlendOption = Override
Mesh Space Rotation Blend = false
Mesh Space Scale Blend = false
Root Space Rotation Blend = false
```

Diana title에 적용한 동일 policy:

```text
BranchFilter BoneName = Neck_0
BlendDepth = 0
BlendWeights[0] = 1.0
CurveBlendOption = Override
Mesh Space Rotation Blend = false
Mesh Space Scale Blend = false
Root Space Rotation Blend = false
```

차이는 branch root뿐이다. Hugh는 upper-body montage용이라 `Spine_0`, Diana title은 neck/head/face override라 `Neck_0`.

## 원인이 아니었던 것

- Facial animation asset 누락 아님
  - `AS_Diana_Face_MiniDemo_md09000_Title_Loop_VerB`에는 `neck_0`, `head`, `l_eye`, `r_eye`, `c_jaw` 등 543개 track 존재
- Title actor assignment 문제 아님
  - `Title_Diana_Showcase`는 `Animation Blueprint` mode와 `ABP_Diana_Title_C` 사용 확인
- Hair physics 문제 아님
  - hair `RigidBody`는 final blended pose 뒤에 있으며 별도 정상 동작 확인
- `Neck_0` 대소문자만의 문제로 보기 어려움
  - UE `FName`/skeleton lookup 특성상 대소문자만으로 실패한다고 단정하기 어렵고, 최종값은 skeleton display name과 Hugh 패턴에 맞춰 `Neck_0` 사용

## 검증 증거

Animation data 확인:

```text
title body track_count = 543
title facial track_count = 543
facial asset wanted tracks:
  neck_0, neck_1, head, facialdef_face, l_eye, r_eye, c_jaw
```

frame 700 기준 body animation과 facial animation 차이:

```text
neck_0 descendant changed bones = 199
대표 변화:
  l_d_eyelid4
  l_u_eyelid3
  r_u_eyelid2
  l_eye
  r_eye
  c_jaw
  mouth/lip/eyebrow 주변 bones
```

계층 확인:

```text
l_eye -> FacialDef_Face -> Head -> Neck_1 -> Neck_0 -> Spine_2 -> ...
r_eye -> FacialDef_Face -> Head -> Neck_1 -> Neck_0 -> Spine_2 -> ...
c_jaw -> FacialDef_Face -> Head -> Neck_1 -> Neck_0 -> Spine_2 -> ...
```

즉 `Neck_0` branch root는 눈, 턱, 입, 얼굴 def bone을 포함한다.

최종 node 검증:

```text
ABP_Diana_Title LayeredBoneBlend:
  filter[0] bone_name = Neck_0
  filter[0] blend_depth = 0
  blend_weights = [1.0]
  curve_blend_option = Override
  mesh_space_rotation_blend = false
  mesh_space_scale_blend = false
```

Editor 검증:

- `ABP_Diana_Title` compile 성공
- `ABP_Diana_Title` save 성공
- `Title_Diana_Showcase`에서 facial animation visible 확인

## 이후 animation blend 작업 규칙

Animation blend가 보이지 않을 때 먼저 볼 항목:

1. 같은 프로젝트의 작동하는 blend node를 gold standard로 inspect
2. `BlendWeights_*` pin default와 내부 `blend_weights` 둘 다 `1.0`인지 확인
3. full branch override가 목적이면 `BlendDepth=0` 우선 사용
4. `BlendDepth`를 큰 값으로 "하위 전체 포함" 의미로 쓰지 않기
5. `CurveBlendOption`은 작동 기준 asset이 `Override`면 그대로 맞추기
6. `Mesh Space Rotation/Scale Blend`는 기준 asset이 쓰지 않으면 켜지 않기
7. branch root가 실제 target bones의 ancestor인지 `find_bone_path_to_root` 또는 Skeleton Tree로 검증
8. compile/save 후 commandlet 또는 UnreelMCP inspect로 저장값 재확인

## 관련 파일

- `Content/Blueprints/Player/ABP_Diana_Title.uasset`
- `Content/Blueprints/Player/ABP_HughCharacter.uasset`
- `Content/Map/Title.umap`
- `.agents/directives/09-unreelmcp-operation-recipes.md`
