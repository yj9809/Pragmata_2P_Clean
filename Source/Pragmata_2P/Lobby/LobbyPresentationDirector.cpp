#include "Lobby/LobbyPresentationDirector.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* HughFocusDianaStartPath = TEXT("/Game/Character/Hugh/Animations/Reinforce/AS_Hugh_Reinforce_FocusDiana_Start_Focus.AS_Hugh_Reinforce_FocusDiana_Start_Focus");
	const TCHAR* HughFocusDianaLoopPath = TEXT("/Game/Character/Hugh/Animations/Reinforce/AS_Hugh_Reinforce_FocusDiana_Loop.AS_Hugh_Reinforce_FocusDiana_Loop");
	const TCHAR* DianaFocusDianaStartPath = TEXT("/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_FocusDiana_Start_VerB.AS_Diana_Reinforce_FocusDiana_Start_VerB");
	const TCHAR* DianaFocusDianaLoopPath = TEXT("/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_FocusDiana_Loop.AS_Diana_Reinforce_FocusDiana_Loop");
	const TCHAR* HughFocusHughStartPath = TEXT("/Game/Character/Hugh/Animations/Reinforce/AS_Hugh_Reinforce_FocusPL_Start.AS_Hugh_Reinforce_FocusPL_Start");
	const TCHAR* HughFocusHughLoopPath = TEXT("/Game/Character/Hugh/Animations/Reinforce/AS_Hugh_Reinforce_FocusPL_Loop.AS_Hugh_Reinforce_FocusPL_Loop");
	const TCHAR* DianaFocusHughStartPath = TEXT("/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_FocusPL_Start_VerB.AS_Diana_Reinforce_FocusPL_Start_VerB");
	const TCHAR* DianaFocusHughLoopPath = TEXT("/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_FocusPL_Loop.AS_Diana_Reinforce_FocusPL_Loop");

	constexpr float HughLegacyScreenRightOffsetThreshold = 120.0f;
	constexpr float DianaLegacyScreenRightOffsetThreshold = 100.0f;

	void ApplyHughFocusFraming(FLobbyCameraFocusSettings& Settings)
	{
		Settings.OrbitAngleDegrees = 16.0f;
		Settings.Radius = 620.0f;
		Settings.Height = 215.0f;
		Settings.FieldOfView = 66.0f;
		Settings.LookAtOffset = FVector(0.0, 0.0, 135.0);
		Settings.ScreenRightOffset = 55.0f;
	}

	void ApplyDianaFocusFraming(FLobbyCameraFocusSettings& Settings)
	{
		Settings.OrbitAngleDegrees = -50.0f;
		Settings.Radius = 520.0f;
		Settings.Height = 165.0f;
		Settings.FieldOfView = 60.0f;
		Settings.LookAtOffset = FVector(0.0, 0.0, 105.0);
		Settings.ScreenRightOffset = 45.0f;
	}
}

// MatchLobby presentation은 level 배치 mesh를 우선하고 C++은 camera/animation 지휘만 담당
ALobbyPresentationDirector::ALobbyPresentationDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->SetFieldOfView(58.0f);

	ApplyHughFocusFraming(HughFocusSettings);
	ApplyDianaFocusFraming(DianaFocusSettings);

	SkinSet = TSoftObjectPtr<ULobbyCharacterSkinSet>(FSoftObjectPath(TEXT("/Game/Data/DA_LobbyCharacterSkinSet.DA_LobbyCharacterSkinSet")));
}

// 배치 actor와 선택 연출 asset을 찾되, 실제 선택 단계 전까지 preview는 숨김
void ALobbyPresentationDirector::BeginPlay()
{
	Super::BeginPlay();

	// 이전 map instance에 저장된 과한 화면 오른쪽 offset을 새 framing 기준으로 보정
	if (HughFocusSettings.ScreenRightOffset > HughLegacyScreenRightOffsetThreshold)
	{
		ApplyHughFocusFraming(HughFocusSettings);
	}
	if (DianaFocusSettings.ScreenRightOffset > DianaLegacyScreenRightOffsetThreshold)
	{
		ApplyDianaFocusFraming(DianaFocusSettings);
	}

	ResolvePreviewMeshes();
	if (!AnimationSet.IsNull())
	{
		LoadedAnimationSet = AnimationSet.LoadSynchronous();
	}
	PreloadSkinMeshes();

	SetPreviewPresentationEnabled(false);
}

// 선택 전환 중 orbit camera 값을 보간
void ALobbyPresentationDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bCameraTransitionActive)
	{
		return;
	}

	TransitionElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(TransitionElapsed / FMath::Max(TransitionDuration, 0.05f), 0.0f, 1.0f);
	const float SmoothedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	CurrentAngle = FMath::Lerp(TransitionStartAngle, TransitionTargetAngle, SmoothedAlpha);
	CurrentRadius = FMath::Lerp(TransitionStartRadius, TransitionTargetRadius, SmoothedAlpha);
	CurrentHeight = FMath::Lerp(TransitionStartHeight, TransitionTargetHeight, SmoothedAlpha);
	CurrentFOV = FMath::Lerp(TransitionStartFOV, TransitionTargetFOV, SmoothedAlpha);
	ApplyCameraPose(CurrentAngle, CurrentRadius, CurrentHeight, CurrentFOV, TransitionTargetRole);

	if (Alpha >= 1.0f)
	{
		bCameraTransitionActive = false;
		FocusedRole = TransitionTargetRole;
	}
}

// local selection 변경을 camera와 두 preview animation에 동시에 반영
void ALobbyPresentationDirector::FocusRole(EPragmataPlayerRole NewFocusedRole, int32 Direction)
{
	if (NewFocusedRole != EPragmataPlayerRole::Hugh && NewFocusedRole != EPragmataPlayerRole::Diana)
	{
		return;
	}

	if (!bPreviewPresentationEnabled)
	{
		SetPreviewPresentationEnabled(true);
	}

	const bool bSamePresentationTarget = (FocusedRole == NewFocusedRole && !bCameraTransitionActive)
		|| (bCameraTransitionActive && TransitionTargetRole == NewFocusedRole);
	if (bSamePresentationTarget)
	{
		// 모든 skin mesh 갱신 뒤 조기 반환 전에 두 preview의 Single Node 상태를 함께 복구
		if (bPreviewAnimationInvalidatedBySkinChange)
		{
			PlayPreviewSequence(NewFocusedRole);
			bPreviewAnimationInvalidatedBySkinChange = false;
		}
		return;
	}

	StartCameraTransition(NewFocusedRole, Direction);
	PlayPreviewSequence(NewFocusedRole);
	bPreviewAnimationInvalidatedBySkinChange = false;
}

void ALobbyPresentationDirector::SetPreviewPresentationEnabled(bool bEnabled)
{
	if (!HughMeshComponent || !DianaMeshComponent)
	{
		ResolvePreviewMeshes();
	}

	bPreviewPresentationEnabled = bEnabled;
	if (bEnabled)
	{
		PreloadSkinMeshes();
	}

	ApplyPreviewMeshVisibility(HughMeshComponent, bEnabled);
	ApplyPreviewMeshVisibility(DianaMeshComponent, bEnabled);

	if (bEnabled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HughLoopTimerHandle);
		World->GetTimerManager().ClearTimer(DianaLoopTimerHandle);
	}

	HughLoopAnimation = nullptr;
	DianaLoopAnimation = nullptr;
	FocusedRole = EPragmataPlayerRole::None;
	TransitionTargetRole = EPragmataPlayerRole::None;
	bCameraInitialized = false;
	bCameraTransitionActive = false;
	bPreviewAnimationInvalidatedBySkinChange = false;
}

// skin mesh 교체로 Single Node 상태가 초기화되면 focus commit 지점에서 함께 복구하도록 기록
void ALobbyPresentationDirector::SetPreviewSkin(EPragmataPlayerRole PreviewRole, FName SkinId)
{
	if (SkinId == NAME_None)
	{
		return;
	}

	USkeletalMeshComponent* TargetMeshComponent = PreviewRole == EPragmataPlayerRole::Diana ? DianaMeshComponent : HughMeshComponent;
	FName& CurrentSkinId = PreviewRole == EPragmataPlayerRole::Diana ? CurrentDianaSkinId : CurrentHughSkinId;
	if (!TargetMeshComponent)
	{
		ResolvePreviewMeshes();
		TargetMeshComponent = PreviewRole == EPragmataPlayerRole::Diana ? DianaMeshComponent : HughMeshComponent;
	}
	if (!TargetMeshComponent || CurrentSkinId == SkinId)
	{
		return;
	}

	ULobbyCharacterSkinSet* LoadedSet = LoadLobbySkinSet();
	USkeletalMesh* SkinMesh = LoadedSet ? LoadedSet->LoadSkinMesh(PreviewRole, SkinId) : nullptr;
	if (!SkinMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPresentationDirector::SetPreviewSkin] Missing preview skin mesh: role=%d skin=%s"), static_cast<int32>(PreviewRole), *SkinId.ToString());
		return;
	}

	const bool bMeshChanged = TargetMeshComponent->GetSkeletalMeshAsset() != SkinMesh;
	if (bMeshChanged)
	{
		TargetMeshComponent->SetSkeletalMesh(SkinMesh);
		bPreviewAnimationInvalidatedBySkinChange = true;
	}
	CurrentSkinId = SkinId;
}

void ALobbyPresentationDirector::ResolvePreviewMeshes()
{
	// level에 배치된 preview actor를 tag로 찾아 camera/animation 대상에 연결
	HughMeshComponent = ResolvePreviewMeshByTag(HughPreviewTag);
	DianaMeshComponent = ResolvePreviewMeshByTag(DianaPreviewTag);

	if (!HughMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPresentationDirector::ResolvePreviewMeshes] Missing Hugh lobby preview mesh with tag %s"), *HughPreviewTag.ToString());
	}
	if (!DianaMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPresentationDirector::ResolvePreviewMeshes] Missing Diana lobby preview mesh with tag %s"), *DianaPreviewTag.ToString());
	}

	ApplyPreviewMeshVisibility(HughMeshComponent, bPreviewPresentationEnabled);
	ApplyPreviewMeshVisibility(DianaMeshComponent, bPreviewPresentationEnabled);
}

void ALobbyPresentationDirector::ApplyPreviewMeshVisibility(USkeletalMeshComponent* MeshComponent, bool bVisible) const
{
	if (!MeshComponent)
	{
		return;
	}

	MeshComponent->SetVisibility(bVisible, true);
	MeshComponent->SetHiddenInGame(!bVisible, true);
	if (!bVisible)
	{
		MeshComponent->Stop();
	}
}

USkeletalMeshComponent* ALobbyPresentationDirector::ResolvePreviewMeshByTag(FName ActorTag) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
	{
		ASkeletalMeshActor* MeshActor = *It;
		if (MeshActor && MeshActor->ActorHasTag(ActorTag))
		{
			return MeshActor->GetSkeletalMeshComponent();
		}
	}

	return nullptr;
}

FLobbyCameraFocusSettings ALobbyPresentationDirector::GetFocusSettings(EPragmataPlayerRole TargetRole) const
{
	return TargetRole == EPragmataPlayerRole::Diana ? DianaFocusSettings : HughFocusSettings;
}

float ALobbyPresentationDirector::ResolveTargetAngle(float StartAngle, float TargetAngle, int32 Direction, EPragmataPlayerRole TargetRole) const
{
	int32 ResolvedDirection = Direction;
	const EPragmataPlayerRole SourceRole = bCameraTransitionActive ? TransitionTargetRole : FocusedRole;
	if (SourceRole == EPragmataPlayerRole::Hugh && TargetRole == EPragmataPlayerRole::Diana)
	{
		ResolvedDirection = -1;
	}
	else if (SourceRole == EPragmataPlayerRole::Diana && TargetRole == EPragmataPlayerRole::Hugh)
	{
		ResolvedDirection = 1;
	}

	if (ResolvedDirection < 0)
	{
		while (TargetAngle >= StartAngle)
		{
			TargetAngle -= 360.0f;
		}
	}
	else if (ResolvedDirection > 0)
	{
		while (TargetAngle <= StartAngle)
		{
			TargetAngle += 360.0f;
		}
	}

	return TargetAngle;
}

void ALobbyPresentationDirector::StartCameraTransition(EPragmataPlayerRole TargetRole, int32 Direction)
{
	const FLobbyCameraFocusSettings TargetSettings = GetFocusSettings(TargetRole);
	const float RawTargetAngle = TargetSettings.OrbitAngleDegrees;

	if (!bCameraInitialized)
	{
		CurrentAngle = RawTargetAngle;
		CurrentRadius = TargetSettings.Radius;
		CurrentHeight = TargetSettings.Height;
		CurrentFOV = TargetSettings.FieldOfView;
		ApplyCameraPose(CurrentAngle, CurrentRadius, CurrentHeight, CurrentFOV, TargetRole);
		FocusedRole = TargetRole;
		bCameraInitialized = true;
		return;
	}

	TransitionTargetRole = TargetRole;
	TransitionElapsed = 0.0f;
	TransitionStartAngle = CurrentAngle;
	TransitionTargetAngle = ResolveTargetAngle(TransitionStartAngle, RawTargetAngle, Direction, TargetRole);
	TransitionStartRadius = CurrentRadius;
	TransitionTargetRadius = TargetSettings.Radius;
	TransitionStartHeight = CurrentHeight;
	TransitionTargetHeight = TargetSettings.Height;
	TransitionStartFOV = CurrentFOV;
	TransitionTargetFOV = TargetSettings.FieldOfView;
	bCameraTransitionActive = true;
}

void ALobbyPresentationDirector::ApplyCameraPose(float AngleDegrees, float Radius, float Height, float FieldOfView, EPragmataPlayerRole TargetRole)
{
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	const FVector CameraLocation = OrbitCenter + FVector(-Radius * FMath::Cos(AngleRadians), Radius * FMath::Sin(AngleRadians), Height);
	const FLobbyCameraFocusSettings FocusSettings = GetFocusSettings(TargetRole);
	FVector LookAtLocation = ResolveLookAtLocation(TargetRole, FocusSettings);
	if (!FMath::IsNearlyZero(FocusSettings.ScreenRightOffset))
	{
		const FRotator BaseRotation = (LookAtLocation - CameraLocation).Rotation();
		LookAtLocation += FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::Y) * FocusSettings.ScreenRightOffset;
	}
	const FRotator CameraRotation = (LookAtLocation - CameraLocation).Rotation();

	SetActorLocation(CameraLocation);
	SetActorRotation(CameraRotation);
	if (CameraComponent)
	{
		CameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->SetFieldOfView(FieldOfView);
	}
}

FVector ALobbyPresentationDirector::ResolveLookAtLocation(EPragmataPlayerRole TargetRole, const FLobbyCameraFocusSettings& FocusSettings) const
{
	const USkeletalMeshComponent* TargetMesh = TargetRole == EPragmataPlayerRole::Diana ? DianaMeshComponent : HughMeshComponent;
	return TargetMesh ? TargetMesh->GetComponentLocation() + FocusSettings.LookAtOffset : OrbitCenter + FocusSettings.LookAtOffset;
}

void ALobbyPresentationDirector::PlayPreviewSequence(EPragmataPlayerRole TargetRole)
{
	PlayPreviewSequenceForMesh(EPragmataPlayerRole::Hugh, TargetRole, HughMeshComponent, HughLoopTimerHandle, HughLoopAnimation);
	PlayPreviewSequenceForMesh(EPragmataPlayerRole::Diana, TargetRole, DianaMeshComponent, DianaLoopTimerHandle, DianaLoopAnimation);
}

void ALobbyPresentationDirector::PlayPreviewSequenceForMesh(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole, USkeletalMeshComponent* MeshComponent, FTimerHandle& LoopTimerHandle, TObjectPtr<UAnimationAsset>& LoopAnimationStorage)
{
	if (!MeshComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(LoopTimerHandle);
	}

	FLobbyPreviewAnimationSequence Sequence;
	if (!ResolveAnimationSequence(PreviewRole, TargetRole, Sequence))
	{
		return;
	}

	UAnimationAsset* StartAnimation = LoadAnimation(Sequence.StartAnimation, TEXT("Lobby focus start animation"));
	UAnimationAsset* LoopAnimation = LoadAnimation(Sequence.LoopAnimation, TEXT("Lobby focus loop animation"));
	if (!StartAnimation || !LoopAnimation)
	{
		return;
	}

	MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComponent->SetAnimation(StartAnimation);
	MeshComponent->Play(false);
	LoopAnimationStorage = LoopAnimation;

	if (World)
	{
		const float StartLength = FMath::Max(GetAnimationLength(StartAnimation), 0.05f);
		FTimerDelegate LoopDelegate;
		if (PreviewRole == EPragmataPlayerRole::Hugh)
		{
			LoopDelegate.BindUObject(this, &ThisClass::PlayHughLoopAnimation);
		}
		else
		{
			LoopDelegate.BindUObject(this, &ThisClass::PlayDianaLoopAnimation);
		}
		World->GetTimerManager().SetTimer(LoopTimerHandle, LoopDelegate, StartLength, false);
	}
}

bool ALobbyPresentationDirector::ResolveAnimationSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole, FLobbyPreviewAnimationSequence& OutSequence) const
{
	if (LoadedAnimationSet && LoadedAnimationSet->FindSequence(PreviewRole, TargetRole, OutSequence))
	{
		return true;
	}

	OutSequence = GetFallbackAnimationSequence(PreviewRole, TargetRole);
	return !OutSequence.StartAnimation.IsNull() && !OutSequence.LoopAnimation.IsNull();
}

FLobbyPreviewAnimationSequence ALobbyPresentationDirector::GetFallbackAnimationSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole) const
{
	FLobbyPreviewAnimationSequence Sequence;
	Sequence.PreviewRole = PreviewRole;
	Sequence.FocusedRole = TargetRole;

	if (TargetRole == EPragmataPlayerRole::Diana)
	{
		Sequence.StartAnimation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(PreviewRole == EPragmataPlayerRole::Hugh ? HughFocusDianaStartPath : DianaFocusDianaStartPath));
		Sequence.LoopAnimation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(PreviewRole == EPragmataPlayerRole::Hugh ? HughFocusDianaLoopPath : DianaFocusDianaLoopPath));
	}
	else if (TargetRole == EPragmataPlayerRole::Hugh)
	{
		Sequence.StartAnimation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(PreviewRole == EPragmataPlayerRole::Hugh ? HughFocusHughStartPath : DianaFocusHughStartPath));
		Sequence.LoopAnimation = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(PreviewRole == EPragmataPlayerRole::Hugh ? HughFocusHughLoopPath : DianaFocusHughLoopPath));
	}

	return Sequence;
}

ULobbyCharacterSkinSet* ALobbyPresentationDirector::LoadLobbySkinSet()
{
	if (LoadedSkinSet)
	{
		return LoadedSkinSet;
	}

	if (!SkinSet.IsNull())
	{
		LoadedSkinSet = SkinSet.LoadSynchronous();
	}

	if (!LoadedSkinSet)
	{
		LoadedSkinSet = NewObject<ULobbyCharacterSkinSet>(this, TEXT("RuntimeLobbyPreviewSkinSet"));
	}

	return LoadedSkinSet;
}

// preview 진입 전 skin mesh를 메모리에 올려 packaged 전환 지연을 줄임
void ALobbyPresentationDirector::PreloadSkinMeshes()
{
	if (ULobbyCharacterSkinSet* LoadedSet = LoadLobbySkinSet())
	{
		LoadedSet->LoadAllMeshes();
	}
}

UAnimationAsset* ALobbyPresentationDirector::LoadAnimation(const TSoftObjectPtr<UAnimationAsset>& Animation, const TCHAR* Context) const
{
	if (Animation.IsNull())
	{
		return nullptr;
	}

	UAnimationAsset* LoadedAnimation = Animation.LoadSynchronous();
	if (!LoadedAnimation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPresentationDirector::LoadAnimation] Missing %s: %s"), Context, *Animation.ToSoftObjectPath().ToString());
	}

	return LoadedAnimation;
}

float ALobbyPresentationDirector::GetAnimationLength(const UAnimationAsset* Animation) const
{
	if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(Animation))
	{
		return SequenceBase->GetPlayLength();
	}

	return 0.1f;
}

void ALobbyPresentationDirector::PlayHughLoopAnimation()
{
	if (HughMeshComponent && HughLoopAnimation)
	{
		HughMeshComponent->SetAnimation(HughLoopAnimation);
		HughMeshComponent->Play(true);
	}
}

void ALobbyPresentationDirector::PlayDianaLoopAnimation()
{
	if (DianaMeshComponent && DianaLoopAnimation)
	{
		DianaMeshComponent->SetAnimation(DianaLoopAnimation);
		DianaMeshComponent->Play(true);
	}
}
