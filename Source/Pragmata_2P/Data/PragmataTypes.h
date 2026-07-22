#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/SoftObjectPtr.h"
#include "PragmataTypes.generated.h"

class AActor;
class AController;

UENUM(BlueprintType)
enum class EWaistArmState : uint8
{
	Idle = 0,
	Aim_Start,
	Aim_End
};

// 무기가 로봇팔과 Hugh 손 중 어디에 붙는지 복제
UENUM(BlueprintType)
enum class EPragmataWeaponAttachMode : uint8
{
	WaistArm = 0,
	Hand
};

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Primary = 0,
	Attack,
	Tactical,
	Defense,
	Max
};

//for abp
UENUM(BlueprintType)
enum class EActionState : uint8
{
	Locomotion = 0,
	Combat,
	Dash,
	Dodge,
	Death
};

//for hughtcharacter.cpp
UENUM(BlueprintType)
enum class EMovementState : uint8
{
	Idle = 0,
	Walk,
	Jog,
	Run
};

// for MonsterBase
UENUM(BlueprintType)
enum class EMonsterMoveSpeedMode : uint8
{
	Default = 0,
	Patrol,
	Chase,
	Slow,
	Fast
};

UENUM(BlueprintType)
enum class EPragmataFaction : uint8
{
	Neutral,
	Player,
	Monster
};

UENUM(BlueprintType)
enum class EPragmataPlayerRole : uint8
{
	None,
	Hugh,
	Diana
};

UENUM(BlueprintType)
enum class EPragmataUpgradeType : uint8
{
	None,
	Suit,
	Weapon,
	Hacking
};

UENUM(BlueprintType)
enum class EPragmataScanTargetKind : uint8
{
	Unknown,
	Enemy,
	MapHackNode,
	ItemBox,
	Checkpoint
};

UENUM(BlueprintType)
enum class EPragmataLobbyPhase : uint8
{
	Title,
	SessionSetup,
	WaitingForPlayer,
	PlayerJoining,
	CharacterSelect,
	ReadyToStart,
	StartingGame
};

UENUM(BlueprintType)
enum class EPragmataDamageKind : uint8
{
	Generic, // 몬스터 공통
	Weapon, // Hugh 무기
	Hack, // Diana 해킹
	Melee // Hugh 크리티컬
};

UENUM(BlueprintType)
enum class EPragmataHitReactStrength : uint8
{
	None, // fallback
	Small, // 그냥 움찔
	Medium, // 뒷걸음질 칠 정도
	Large, // 나가떨어질 정도
	Death, // S/M/L 무시하고 위 애니메이션 재생
	ForcedPair // Diana 조작을 잠그는 강제 paired 피격
};

UENUM(BlueprintType)
enum class EPragmataHitDirection : uint8
{
	Front,
	Right,
	Back,
	Left
};

UENUM(BlueprintType)
enum class EDianaOperatorMode : uint8
{
	Roaming,
	Combat,
	// Hugh 탑승 감시 pose용 상태. 장소 shelter와 무관
	MountedWatch,
	HitReactLocked
};

UENUM(BlueprintType)
enum class EPairedAnimationCueType : uint8
{
	HitReact,
	Death,
	// Diana mounted watch montage를 강제 동기화 cue로 재생
	MountedWatch
};

// 맵 해킹 퍼즐 입력 방향
UENUM(BlueprintType)
enum class EHackDirection : uint8
{
	Up    UMETA(DisplayName = "Up"),
	Down  UMETA(DisplayName = "Down"),
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	None  UMETA(DisplayName = "None")
};

// 맵 해킹 퍼즐 진행 상태
UENUM(BlueprintType)
enum class EMapHackState : uint8
{
	Idle,
	Active,
	Solved
};

// 적 해킹 퍼즐 - 적 종류 (그리드 크기 결정)
UENUM(BlueprintType)
enum class EEnemyHackType : uint8
{
	Watcher   UMETA(DisplayName = "Watcher"),   // 3x3
	Walker    UMETA(DisplayName = "Walker"),    // 4x4
	Executor  UMETA(DisplayName = "Executor"),  // 5x5
	Sentinel  UMETA(DisplayName = "Sentinel"),  // 8x8
};

// 적 해킹 퍼즐 - 그리드 셀 노드 타입
UENUM(BlueprintType)
enum class EHackNodeType : uint8
{
	Empty     UMETA(DisplayName = "Empty"),     // 빈 셀 (통과 가능, 노드 없음)
	Normal    UMETA(DisplayName = "Normal"),    // 파란 노드 (랜덤 배치)
	Corrupted UMETA(DisplayName = "Corrupted"), // 보라 노드 (오염된 적 전용)
	Target    UMETA(DisplayName = "Target"),    // 목표 노드
	Decode    UMETA(DisplayName = "Decode"),    // 노란 노드 (적 방어력 감소, 그리드당 1개)
};

// 적 해킹 퍼즐 - 그리드 셀 표시 상태
UENUM(BlueprintType)
enum class EHackCellState : uint8
{
	Default UMETA(DisplayName = "Default"),
	Visited UMETA(DisplayName = "Visited"),  // 회색 경로
	Cursor  UMETA(DisplayName = "Cursor"),   // 현재 커서
};

// 적 해킹 퍼즐 진행 상태
UENUM(BlueprintType)
enum class EEnemyHackState : uint8
{
	Idle,
	Active,
	Completed,
};

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataDamageSpec
{
	GENERATED_BODY()

	// 최종 체력 차감량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	float DamageAmount = 0.f;

	// 피해를 시작한 actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	// 피해 판정이나 projectile을 소유한 actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	TObjectPtr<AActor> DamageCauser = nullptr;

	// kill credit과 AI 반응에 쓰는 controller
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	TObjectPtr<AController> InstigatorController = nullptr;

	// 피해 출처 분류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	EPragmataDamageKind DamageKind = EPragmataDamageKind::Generic;

	// 대미지 값에 따라 자동으로 HitReact 얼마나 강하게 할지 결정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	EPragmataHitReactStrength HitReactStrength = EPragmataHitReactStrength::Small;

	// HitReact 어느 방향으로 할지 결정 (대미지 처리 시 계산 필요)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	EPragmataHitDirection HitDirection = EPragmataHitDirection::Front;

	// impact 정보가 있는 point/sweep 피해인지 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	bool bHasImpact = false;

	// point/sweep 피해 위치와 bone 정보를 전달
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	FHitResult ImpactHit;

	// 공격자가 target을 때린 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Damage")
	FVector ImpactDirection = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataScanResult
{
	GENERATED_BODY()

	// scan 대상이 유효한지 여부
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	bool bIsValid = false;

	// scan으로 찾은 actor
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	TObjectPtr<AActor> TargetActor = nullptr;

	// HUD marker가 actor reference resolve에 실패해도 표시할 수 있는 scan 시점 위치
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	FVector TargetWorldLocation = FVector::ZeroVector;

	// UI와 gameplay에서 쓰는 대상 id
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	FName TargetId = NAME_None;

	// scan UI가 POI와 enemy scan을 구분하는 대상 종류
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	EPragmataScanTargetKind TargetKind = EPragmataScanTargetKind::Unknown;

	// scan UI에 표시할 텍스트
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	FText DisplayText;

	// scanner와 target 사이 거리
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	float Distance = 0.f;

	// area scan wave가 이 target을 표시하기까지 기다리는 시간
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	float RevealDelay = 0.f;

	// Diana가 이 POI를 실제 상호작용할 수 있는 최대 거리
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Scan")
	float DianaInteractionDistance = 0.f;
};

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPairedAnimationCue
{
	GENERATED_BODY()

	// cue 순서 보장용 id
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	uint8 SequenceId = 0;

	// paired animation cue 종류
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EPairedAnimationCueType CueType = EPairedAnimationCueType::HitReact;

	// hit react 세기
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EPragmataHitReactStrength Strength = EPragmataHitReactStrength::Small;

	// hit react 방향
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EPragmataHitDirection Direction = EPragmataHitDirection::Front;

	// 서버 기준 montage 시작 시간
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	float ServerStartTime = 0.f;
};

// Hugh 애니메이션 상태를 원격 프록시에 전달
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FHughReplicatedAnimState
{
	GENERATED_BODY()

	// 상체 전투/이동 상태 machine을 원격 프록시에 맞춤
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EActionState ActionState = EActionState::Locomotion;

	// walk/jog/run blend가 서버 movement 상태와 어긋나지 않게 전달
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EMovementState MovementState = EMovementState::Idle;

	// aim pose와 weapon hand attach 표시를 원격 클라이언트에 재현
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bIsAiming = false;

	// sprint animation과 MaxWalkSpeed 의도를 simulated proxy에 노출
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bIsRunning = false;

	// jump hold 기반 자동 hover 판정을 animation graph와 공유
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bIsJumpInput = false;

	// 공중 running pose 대신 hover loop를 고르게 하는 서버 확정 상태
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bIsHovering = false;

	// server 승인 dash가 진행 중인지 remote presentation에 전달
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bIsDashing = false;

	// 0 ground, 1 aim, 2 midair montage 선택
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	uint8 DashPresentationMode = 0;

	// aim dash montage section
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	FName DashSectionName = NAME_None;

	// 연속 dash event를 OnRep에서 구분
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	int32 DashActionSerial = 0;

	// server 승인 weapon change presentation 대상
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EWeaponType WeaponChangeTargetType = EWeaponType::Primary;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EWeaponType WeaponChangePreviousType = EWeaponType::Primary;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	bool bWeaponChangeActive = false;

	// 연속 weapon change를 OnRep에서 구분
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	int32 WeaponChangeActionSerial = 0;

	// server에서 탄약 고갈이 확정된 non-primary weapon
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	EWeaponType WeaponDepletionType = EWeaponType::Primary;

	// 연속 weapon depletion montage를 OnRep에서 구분
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Animation")
	int32 WeaponDepletionActionSerial = 0;
};

// 튜토리얼 키 입력 한 줄 (행동 설명 + RichText 이미지 데코레이터 row name)
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FTutorialKeyBinding
{
	GENERATED_BODY()

	// 키 입력에 대응하는 행동 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial")
	FText ActionLabel;

	// RichTextBlockImageDecorator DataTable의 row name과 매칭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial")
	FName IconStyleRowName;
};

// 대사 한 줄. 표시 타이밍은 게임 로직이 소유하고 UI는 전달받은 내용만 그린다
// 초상화/음성 확장은 필드 추가로 처리해 진입점 시그니처를 유지
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FDialogueLine
{
	GENERATED_BODY()

	// 화자 이름. 비우면 나레이션으로 보고 UI에서 화자 라인을 숨김
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Dialogue")
	FText Speaker;

	// 현지화/줄바꿈 대응용 대사 본문
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Dialogue")
	FText Line;
};

// Diana가 행동 결과에 따라 재생할 음성 카테고리. 대사 선택과 cooldown은 DianaVoiceComponent가 server에서 소유한다.
UENUM(BlueprintType)
enum class EDianaVoiceAction : uint8
{
	Caution,
	Cleanse,
	DecoyGeneratorObtain,
	Hacking,
	HackingAim,
	HackingFail,
	ImminentVictory,
	ItemBoxOpened,
	OnDeath,
	OverDriveFire,
	OverDriveReady,
	Scan,
	ShockwaveGunObtain,
	StasisNetObtain,
	Victory
};

// 한 음성 파일과 공용 Dialogue UI에 표시할 자막 정보
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FDianaVoiceLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice")
	TSoftObjectPtr<class USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice", meta=(MultiLine="true"))
	FText Subtitle;

	// 0이면 SoundBase duration을 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice", meta=(ClampMin="0.0", Units="s"))
	float DisplaySeconds = 0.f;
};

// 같은 행동에서 임의 선택할 음성 묶음과 행동별 반복 방지 시간
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FDianaVoiceActionPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice")
	EDianaVoiceAction Action = EDianaVoiceAction::Caution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice")
	TArray<FDianaVoiceLine> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice", meta=(ClampMin="0.0", Units="s"))
	float CooldownSeconds = 10.f;
};
