# Pragmata 2P Study Project

이 프로젝트는 **Unreal Engine 5.8**을 사용하여 제작 중인 C++ 기반 학습용 프로젝트입니다.

> **UE5 C++ · 5인 팀 프로젝트 | 윤제영 담당: UI/HUD 및 게임 시스템 이벤트 연동**

[상세 포트폴리오](https://app.notion.com/p/5f08d1fa63aa83a184f681cad1598130) · [UI 소스](Source/Pragmata_2P/UI/)

## 담당 구현 — UI/HUD

- **공유 HUD**: Hugh·Diana의 게임플레이 상태를 `UPragmataHUDViewModel`에 연결하고, `FieldNotify`로 화면에 필요한 값 갱신
- **해킹 UI**: 퍼즐의 시작·진행·초기화·완료 이벤트를 ViewModel의 표시 상태로 변환하고, 대상 변경 시 기존 구독 해제
- **적 HP**: `UEnemyHPWidget`에서 HealthComponent의 체력·사망 이벤트를 직접 구독하고, 대상 교체·위젯 제거 시 구독 해제
- **협업**: 팀원과 UI에 전달할 값, 이벤트 발생 시점, 구독·해제 책임을 정리하고 연결 범위 조율

게임플레이 시스템의 상태를 UI로 전달하는 경계와 화면 표시가 본인 담당 범위. 아래 프로젝트 목적과 구현 목표는 팀 전체의 학습 범위.

| 확인할 구현 | 코드 |
| --- | --- |
| 공유 상태 구독과 ViewModel 갱신 | [`PragmataHUDViewModel.cpp`](Source/Pragmata_2P/UI/PragmataHUDViewModel.cpp) · [`FieldNotify 선언`](Source/Pragmata_2P/UI/PragmataHUDViewModel.h) |
| 해킹 이벤트 연결과 표시 값 변환 | [`HackingPuzzleViewModel.cpp`](Source/Pragmata_2P/UI/HackingPuzzleViewModel.cpp) · [`FieldNotify 선언`](Source/Pragmata_2P/UI/HackingPuzzleViewModel.h) |
| 적 HP 직접 구독·해제 | [`EnemyHPWidget.cpp`](Source/Pragmata_2P/UI/EnemyHPWidget.cpp) — `BindToHealth`, `UnbindFromHealth`, `NativeDestruct` |

## 공개 범위와 실행 조건

이 저장소는 `Pragmata_2P_Clean` 공개본으로, C++ 구현과 문서 검토가 주 용도.

- 공개 기준 커밋 [`9acbe53`](https://github.com/yj9809/Pragmata_2P_Clean/tree/9acbe53d753d52457fed784620bc8398503d04c2)에는 Blueprint·Widget·Material·VFX 등 `.uasset` 185개가 포함되고, 실행 맵인 `.umap`은 없음
- 저장소 이름의 `Clean`은 모든 에셋의 제외나 개별 에셋의 출처·재배포 권한 검증 완료를 뜻하지 않음
- 이 공개본만으로 원래 플레이 화면과 실행 환경을 재현할 수 없음. 실행에는 누락된 맵·리소스·의존성과 각각의 적법한 사용 권한을 별도로 확인·준비해야 함
- 기존 [스테이징 제외 에셋 문서](Docs/Operations/unstaged-assets.md)는 팀 개발 당시의 관리 기록이며, 현재 공개본의 전체 포함 목록이나 공개 배포 권한을 보증하는 문서가 아님

## 프로젝트 목적

본 프로젝트의 목표는 Capcom의 *Pragmata*를 참고한 클론 프로젝트를 기반으로, Hugh와 Diana의 플레이를 분리해 **2인 협동 게임플레이**로 확장해 보는 것입니다.

원작의 SF 분위기, 캐릭터 조합, 전투와 퍼즐이 결합된 흐름을 Unreal Engine에서 연구하되, Hugh와 Diana가 각자 다른 역할과 입력, 상호작용을 담당하는 형태로 우리만의 게임플레이 해석을 더하는 것을 목표로 합니다.

이 프로젝트는 상업적 목적이 아닌 **학습 목적의 클론 시도**입니다. 캐릭터 조작, 2인 로컬/협동 플레이 구조, 상호작용 시스템, 카메라, 전투/퍼즐 연계, AI 및 레벨 흐름을 Unreal Engine C++에서 어떻게 구성할 수 있는지 연구하고 연습하기 위한 5인 팀 교육용 프로젝트입니다.

## 사용 엔진 버전

- Unreal Engine: **5.8**
- 프로젝트 유형: **C++ Unreal Engine 프로젝트**
- 메인 프로젝트 파일: `Pragmata_2P.uproject`
- 메인 소스 모듈: `Source/Pragmata_2P/`

## 구현 목표

주요 학습 및 구현 목표는 다음과 같습니다.

- Hugh와 Diana를 분리한 2인 플레이 구조 구현
- Hugh 중심의 이동, 조준, 전투, 보호/호위 역할 실험
- Diana 중심의 해킹, 퍼즐, 지원, 환경 상호작용 역할 실험
- 두 캐릭터가 서로의 행동을 보완하는 협동 메커니즘 설계
- 로컬 멀티플레이 입력, 플레이어 컨트롤러, 카메라 구성 연구
- 전투와 퍼즐이 함께 흐르는 레벨 진행 구조 실험
- Unreal Engine C++ 기반 게임플레이 프레임워크 이해

## 저작권 및 사용 범위

이 프로젝트는 *Pragmata* 및 해당 캐릭터/콘텐츠의 공식 프로젝트가 아니며, Capcom과 관련이 없습니다.

모든 원작 저작권은 원 권리자에게 있습니다. 본 저장소는 Unreal Engine을 활용한 2인 협동 액션/퍼즐 게임플레이 학습과 기술 연구를 목적으로 합니다.

## 저장소 관리 메모

Unreal Engine에서 자동 생성되는 빌드 산출물, 캐시, 임시 파일은 Git에 포함하지 않습니다. 대표적으로 다음 폴더들은 `.gitignore`를 통해 제외됩니다.

- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- `.vs/`
- `.idea/`
- `.omx/`

공개본에서 추적 중인 주요 파일과 폴더는 다음과 같습니다. `Content/`는 일부 에셋만 포함하며, 위의 공개 범위와 실행 조건을 함께 확인하세요.

- `Config/`
- `Content/` (일부 Blueprint·UI·Material·VFX 등)
- `Source/`
- `Pragmata_2P.uproject`
- `README.md`
- `Docs/`
- `AGENTS.md`
