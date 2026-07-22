#include "Enemy/PragmataMonsterMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UPragmataMonsterMovementComponent::UPragmataMonsterMovementComponent()
{
	// Blueprint와 Level Editor viewport에서도 profile 조정 결과를 계속 표시
	bTickInEditor = true;
}

bool UPragmataMonsterMovementComponent::PushBodySweepProfile(FName ProfileName)
{
	const FPragmataMonsterBodySweepProfile* Profile = FindBodySweepProfile(ProfileName);
	if (!Profile)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UPragmataMonsterMovementComponent::PushBodySweepProfile] Missing profile '%s' on '%s'"),
			*ProfileName.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ActiveBodySweepProfileStack.Add(*Profile);
	return true;
}

bool UPragmataMonsterMovementComponent::PushBodySweepProfileOverride(
	const FPragmataMonsterBodySweepProfile& Profile
)
{
	if (Profile.ProfileName.IsNone() || Profile.Spheres.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UPragmataMonsterMovementComponent::PushBodySweepProfileOverride] Invalid profile '%s' on '%s'"),
			*Profile.ProfileName.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ActiveBodySweepProfileStack.Add(Profile);
	return true;
}

void UPragmataMonsterMovementComponent::PopBodySweepProfile(FName ProfileName)
{
	for (int32 Index = ActiveBodySweepProfileStack.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveBodySweepProfileStack[Index].ProfileName == ProfileName)
		{
			ActiveBodySweepProfileStack.RemoveAt(Index);
			return;
		}
	}
}

bool UPragmataMonsterMovementComponent::UpdateBodySweepProfileSphereOffsets(
	FName ProfileName,
	const TArray<FVector>& DesiredLocalOffsets
)
{
	FPragmataMonsterBodySweepProfile* ActiveProfile = FindActiveBodySweepProfile(ProfileName);
	if (!ActiveProfile || ActiveProfile->Spheres.Num() != DesiredLocalOffsets.Num())
	{
		return false;
	}

	bool bHasOffsetChange = false;
	for (int32 SphereIndex = 0; SphereIndex < ActiveProfile->Spheres.Num(); ++SphereIndex)
	{
		bHasOffsetChange |= !ActiveProfile->Spheres[SphereIndex].LocalOffset.Equals(
			DesiredLocalOffsets[SphereIndex]);
	}
	if (!bHasOffsetChange)
	{
		return true;
	}

	auto ApplyDesiredOffsets = [ActiveProfile, &DesiredLocalOffsets]()
	{
		for (int32 SphereIndex = 0; SphereIndex < ActiveProfile->Spheres.Num(); ++SphereIndex)
		{
			ActiveProfile->Spheres[SphereIndex].LocalOffset = DesiredLocalOffsets[SphereIndex];
		}
	};

	const UWorld* World = GetWorld();
	const UPrimitiveComponent* UpdatedPrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!World || !UpdatedPrimitiveComponent || !CharacterOwner)
	{
		ApplyDesiredOffsets();
		return true;
	}

	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FQuat ComponentRotation = UpdatedComponent->GetComponentQuat();
	const ECollisionChannel CollisionObjectType = UpdatedPrimitiveComponent->GetCollisionObjectType();
	const FCollisionResponseParams ResponseParams(UpdatedPrimitiveComponent->GetCollisionResponseToChannels());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterBodySweepAnimatedOffset), false, CharacterOwner);
	QueryParams.AddIgnoredActor(CharacterOwner);

	FHitResult EarliestHit;
	int32 EarliestHitSphereIndex = INDEX_NONE;
	float EarliestProbeTravelDistance = 0.f;
	for (int32 SphereIndex = 0; SphereIndex < ActiveProfile->Spheres.Num(); ++SphereIndex)
	{
		const FPragmataMonsterBodySweepSphere& Sphere = ActiveProfile->Spheres[SphereIndex];
		if (Sphere.Radius <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Start = ComponentLocation + ComponentRotation.RotateVector(Sphere.LocalOffset);
		const FVector End = ComponentLocation + ComponentRotation.RotateVector(DesiredLocalOffsets[SphereIndex]);
		if (Start.Equals(End))
		{
			continue;
		}

		FHitResult ProbeHit;
		if (World->SweepSingleByChannel(
			ProbeHit,
			Start,
			End,
			FQuat::Identity,
			CollisionObjectType,
			FCollisionShape::MakeSphere(Sphere.Radius),
			QueryParams,
			ResponseParams)
			&& (EarliestHitSphereIndex == INDEX_NONE || ProbeHit.Time < EarliestHit.Time))
		{
			EarliestHit = ProbeHit;
			EarliestHitSphereIndex = SphereIndex;
			EarliestProbeTravelDistance = FVector::Distance(Start, End);
		}
	}

	if (EarliestHitSphereIndex == INDEX_NONE)
	{
		ApplyDesiredOffsets();
		return true;
	}

	// pose-only 이동은 root Capsule을 밀지 않고 가장 먼저 막힌 sphere 시간에 전체 profile을 정지
	const float SkinTime = EarliestProbeTravelDistance > UE_KINDA_SMALL_NUMBER
		? SweepSkinWidth / EarliestProbeTravelDistance
		: 0.f;
	const float AllowedTime = FMath::Clamp(EarliestHit.Time - SkinTime, 0.f, 1.f);
	for (int32 SphereIndex = 0; SphereIndex < ActiveProfile->Spheres.Num(); ++SphereIndex)
	{
		const FVector CurrentOffset = ActiveProfile->Spheres[SphereIndex].LocalOffset;
		ActiveProfile->Spheres[SphereIndex].LocalOffset = FMath::Lerp(
			CurrentOffset,
			DesiredLocalOffsets[SphereIndex],
			AllowedTime);
	}
	return true;
}

// Notify 시작 frame에서는 pose 기준 위치를 활성 profile의 새 기준점으로 직접 맞춤
bool UPragmataMonsterMovementComponent::SetBodySweepProfileSphereOffsets(
	FName ProfileName,
	const TArray<FVector>& LocalOffsets
)
{
	FPragmataMonsterBodySweepProfile* ActiveProfile = FindActiveBodySweepProfile(ProfileName);
	if (!ActiveProfile || ActiveProfile->Spheres.Num() != LocalOffsets.Num())
	{
		return false;
	}

	for (int32 SphereIndex = 0; SphereIndex < ActiveProfile->Spheres.Num(); ++SphereIndex)
	{
		ActiveProfile->Spheres[SphereIndex].LocalOffset = LocalOffsets[SphereIndex];
	}
	return true;
}

FName UPragmataMonsterMovementComponent::GetActiveBodySweepProfileName() const
{
	for (int32 Index = ActiveBodySweepProfileStack.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveBodySweepProfileStack[Index].ProfileName.IsNone())
		{
			return ActiveBodySweepProfileStack[Index].ProfileName;
		}
	}

	return FindBodySweepProfile(DefaultBodySweepProfileName)
		? DefaultBodySweepProfileName
		: NAME_None;
}

const FPragmataMonsterBodySweepProfile* UPragmataMonsterMovementComponent::GetBodySweepProfile(
	FName ProfileName
) const
{
	return FindBodySweepProfile(ProfileName);
}

// Notify가 활성 stack의 실제 정지 위치를 이어서 sweep하도록 현재 sphere offset 복사
bool UPragmataMonsterMovementComponent::GetActiveBodySweepProfileSphereOffsets(
	FName ProfileName,
	TArray<FVector>& OutLocalOffsets
) const
{
	OutLocalOffsets.Reset();
	if (ProfileName.IsNone())
	{
		return false;
	}

	const FPragmataMonsterBodySweepProfile* Profile = nullptr;
	for (int32 Index = ActiveBodySweepProfileStack.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveBodySweepProfileStack[Index].ProfileName == ProfileName)
		{
			Profile = &ActiveBodySweepProfileStack[Index];
			break;
		}
	}
	if (!Profile)
	{
		Profile = FindBodySweepProfile(ProfileName);
	}
	if (!Profile)
	{
		return false;
	}

	OutLocalOffsets.Reserve(Profile->Spheres.Num());
	for (const FPragmataMonsterBodySweepSphere& Sphere : Profile->Spheres)
	{
		OutLocalOffsets.Add(Sphere.LocalOffset);
	}
	return true;
}

bool UPragmataMonsterMovementComponent::GetActiveBodySweepSphereWorldLocation(
	FName ProfileName,
	int32 SphereIndex,
	FVector& OutWorldLocation
) const
{
	if (!UpdatedComponent || ProfileName.IsNone())
	{
		return false;
	}

	const FPragmataMonsterBodySweepProfile* Profile = nullptr;
	for (int32 Index = ActiveBodySweepProfileStack.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveBodySweepProfileStack[Index].ProfileName == ProfileName)
		{
			Profile = &ActiveBodySweepProfileStack[Index];
			break;
		}
	}

	if (!Profile)
	{
		Profile = FindBodySweepProfile(ProfileName);
	}
	if (!Profile || !Profile->Spheres.IsValidIndex(SphereIndex))
	{
		return false;
	}

	const FPragmataMonsterBodySweepSphere& Sphere = Profile->Spheres[SphereIndex];
	OutWorldLocation = UpdatedComponent->GetComponentLocation()
		+ UpdatedComponent->GetComponentQuat().RotateVector(Sphere.LocalOffset);
	return true;
}

void UPragmataMonsterMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	DrawBodySweepDebug();
}

bool UPragmataMonsterMovementComponent::MoveUpdatedComponentImpl(
	const FVector& Delta,
	const FQuat& NewRotation,
	bool bSweep,
	FHitResult* OutHit,
	ETeleportType Teleport
)
{
	if (!ShouldApplyBodySweep(bSweep, Teleport))
	{
		return Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}

	const FPragmataMonsterBodySweepProfile* ActiveProfile = GetActiveBodySweepProfile();
	if (!ActiveProfile)
	{
		return Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}

	FHitResult BodyHit;
	float ProbeTravelDistance = 0.f;
	if (!SweepBodyProfile(*ActiveProfile, Delta, NewRotation, BodyHit, ProbeTravelDistance))
	{
		return Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}

	// body hit 비율만큼 위치와 회전을 함께 제한해 앞·뒤 돌출부가 벽 안으로 들어가지 않게 함
	const float SkinTime = ProbeTravelDistance > UE_KINDA_SMALL_NUMBER
		? SweepSkinWidth / ProbeTravelDistance
		: 0.f;
	const float AllowedTime = FMath::Clamp(BodyHit.Time - SkinTime, 0.f, 1.f);
	const FVector AllowedDelta = Delta * AllowedTime;
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FQuat AllowedRotation = FQuat::Slerp(CurrentRotation, NewRotation, AllowedTime).GetNormalized();

	FHitResult CapsuleHit;
	const bool bMoved = Super::MoveUpdatedComponentImpl(
		AllowedDelta,
		AllowedRotation,
		bSweep,
		&CapsuleHit,
		Teleport);

	if (OutHit)
	{
		if (CapsuleHit.bBlockingHit)
		{
			// 짧아진 캡슐 이동의 hit 비율을 원래 요청 Delta 기준으로 환산
			CapsuleHit.Time *= AllowedTime;
			*OutHit = CapsuleHit;
		}
		else
		{
			// CharacterMovement가 기존 SlideAlongSurface 경로에서 body wall normal을 사용
			*OutHit = BodyHit;
		}
	}

	return bMoved;
}

const FPragmataMonsterBodySweepProfile* UPragmataMonsterMovementComponent::FindBodySweepProfile(FName ProfileName) const
{
	if (ProfileName.IsNone())
	{
		return nullptr;
	}

	return BodySweepProfiles.FindByPredicate(
		[ProfileName](const FPragmataMonsterBodySweepProfile& Profile)
		{
			return Profile.ProfileName == ProfileName;
		});
}

FPragmataMonsterBodySweepProfile* UPragmataMonsterMovementComponent::FindActiveBodySweepProfile(FName ProfileName)
{
	for (int32 Index = ActiveBodySweepProfileStack.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveBodySweepProfileStack[Index].ProfileName == ProfileName)
		{
			return &ActiveBodySweepProfileStack[Index];
		}
	}

	return nullptr;
}

const FPragmataMonsterBodySweepProfile* UPragmataMonsterMovementComponent::GetActiveBodySweepProfile() const
{
	if (!ActiveBodySweepProfileStack.IsEmpty())
	{
		return &ActiveBodySweepProfileStack.Last();
	}

	return FindBodySweepProfile(GetActiveBodySweepProfileName());
}

bool UPragmataMonsterMovementComponent::ShouldApplyBodySweep(bool bSweep, ETeleportType Teleport) const
{
	// simulated proxy는 server 이동 복제를 그대로 따라가고 자체 body sweep으로 보정하지 않음
	return bSweep
		&& Teleport == ETeleportType::None
		&& UpdatedComponent
		&& CharacterOwner
		&& CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy
		&& GetActiveBodySweepProfile() != nullptr;
}

bool UPragmataMonsterMovementComponent::SweepBodyProfile(
	const FPragmataMonsterBodySweepProfile& Profile,
	const FVector& Delta,
	const FQuat& NewRotation,
	FHitResult& OutHit,
	float& OutProbeTravelDistance
) const
{
	const UWorld* World = GetWorld();
	const UPrimitiveComponent* UpdatedPrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!World || !UpdatedPrimitiveComponent)
	{
		return false;
	}

	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const ECollisionChannel CollisionObjectType = UpdatedPrimitiveComponent->GetCollisionObjectType();
	const FCollisionResponseParams ResponseParams(UpdatedPrimitiveComponent->GetCollisionResponseToChannels());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterBodySweep), false, CharacterOwner);
	QueryParams.AddIgnoredActor(CharacterOwner);

	bool bFoundBlockingHit = false;
	for (const FPragmataMonsterBodySweepSphere& Sphere : Profile.Spheres)
	{
		if (Sphere.Radius <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Start = ComponentLocation + CurrentRotation.RotateVector(Sphere.LocalOffset);
		const FVector End = ComponentLocation + Delta + NewRotation.RotateVector(Sphere.LocalOffset);
		FHitResult ProbeHit;
		if (!World->SweepSingleByChannel(
			ProbeHit,
			Start,
			End,
			FQuat::Identity,
			CollisionObjectType,
			FCollisionShape::MakeSphere(Sphere.Radius),
			QueryParams,
			ResponseParams))
		{
			continue;
		}
		// animation 자세 때문에 이미 바닥과 겹친 body sphere는 새 이동을 영구 차단하지 않음
		if (ProbeHit.bStartPenetrating)
		{
			continue;
		}

		if (!bFoundBlockingHit || ProbeHit.Time < OutHit.Time)
		{
			bFoundBlockingHit = true;
			OutHit = ProbeHit;
			OutProbeTravelDistance = FVector::Distance(Start, End);
		}
	}

	return bFoundBlockingHit;
}

void UPragmataMonsterMovementComponent::DrawBodySweepProfile(
	const FPragmataMonsterBodySweepProfile& Profile
) const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !UpdatedComponent)
	{
		return;
	}

	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FQuat ComponentRotation = UpdatedComponent->GetComponentQuat();
	const FColor DrawColor = Profile.PreviewColor.ToFColor(true);
	for (const FPragmataMonsterBodySweepSphere& Sphere : Profile.Spheres)
	{
		if (Sphere.Radius <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Center = ComponentLocation + ComponentRotation.RotateVector(Sphere.LocalOffset);
		DrawDebugSphere(
			World,
			Center,
			Sphere.Radius,
			24,
			DrawColor,
			false,
			0.f,
			0,
			DebugLineThickness);
	}
#endif
}

void UPragmataMonsterMovementComponent::DrawBodySweepDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->IsGameWorld())
	{
		if (bDrawActiveBodySweepInGame)
		{
			if (const FPragmataMonsterBodySweepProfile* ActiveProfile = GetActiveBodySweepProfile())
			{
				DrawBodySweepProfile(*ActiveProfile);
			}
		}
		return;
	}

#if WITH_EDITOR
	// profile checkbox를 켜면 Blueprint와 Level Editor viewport에서 바로 크기와 위치 확인
	for (const FPragmataMonsterBodySweepProfile& Profile : BodySweepProfiles)
	{
		if (Profile.bPreviewInEditor)
		{
			DrawBodySweepProfile(Profile);
		}
	}
#endif
#endif
}
