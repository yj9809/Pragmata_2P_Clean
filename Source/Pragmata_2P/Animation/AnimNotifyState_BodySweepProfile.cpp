#include "Animation/AnimNotifyState_BodySweepProfile.h"

#include "Components/SkeletalMeshComponent.h"
#include "Enemy/PragmataMonsterMovementComponent.h"
#include "GameFramework/Character.h"
#if WITH_EDITOR
#include "Animation/AnimSingleNodeInstance.h"
#include "Enemy/MonsterBase.h"
#include "PrimitiveDrawingUtils.h"
#endif

namespace
{
	UPragmataMonsterMovementComponent* GetMonsterMovementComponent(USkeletalMeshComponent* MeshComp)
	{
		ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
		return Character
			? Cast<UPragmataMonsterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
	}
}

void UAnimNotifyState_BodySweepProfile::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	bool bProfileActivated = false;
	// notify asset이 owner 종류를 고르지 않고 현재 monster의 공통 movement만 제어
	if (UPragmataMonsterMovementComponent* Movement = GetMonsterMovementComponent(MeshComp))
	{
		if (ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride)
		{
			bProfileActivated = Movement->PushBodySweepProfileOverride(MakeNotifyOverrideProfile());
		}
		else
		{
			bProfileActivated = Movement->PushBodySweepProfile(ProfileName);
		}

		if (bProfileActivated && MeshComp
			&& (bUseAnimatedProfileOffset || bConstrainMeshMotionToBodySweep))
		{
			FBodySweepProfileNotifyRuntime RuntimeState;
			RuntimeState.TotalDuration = FMath::Max(TotalDuration, UE_KINDA_SMALL_NUMBER);

			if (bUseAnimatedProfileOffset)
			{
				const FPragmataMonsterBodySweepProfile* BaseProfile = ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride
					? nullptr
					: Movement->GetBodySweepProfile(ProfileName);
				FPragmataMonsterBodySweepProfile NotifyOverrideProfile;
				if (ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride)
				{
					NotifyOverrideProfile = MakeNotifyOverrideProfile();
					BaseProfile = &NotifyOverrideProfile;
				}

				if (BaseProfile)
				{
					TArray<FVector> InitialOffsets;
					EvaluateAnimatedSphereOffsets(0.f, *BaseProfile, InitialOffsets);
					Movement->UpdateBodySweepProfileSphereOffsets(ProfileName, InitialOffsets);
				}
			}

			if (bConstrainMeshMotionToBodySweep)
			{
				if (InitializeMeshConstraint(MeshComp, Movement, RuntimeState)
					&& bDriveFollowSphereFromTrackingBone)
				{
					TArray<FVector> InitialDrivenOffsets;
					if (Movement->GetActiveBodySweepProfileSphereOffsets(ProfileName, InitialDrivenOffsets)
						&& UpdateTrackingBoneDrivenSphereOffset(MeshComp, RuntimeState, InitialDrivenOffsets))
					{
						if (bAttachFollowSphereToTrackingBone)
						{
							Movement->SetBodySweepProfileSphereOffsets(ProfileName, InitialDrivenOffsets);
						}
						else
						{
							Movement->UpdateBodySweepProfileSphereOffsets(ProfileName, InitialDrivenOffsets);
						}
					}
				}
			}
			ActiveOffsetWindows.Add(TObjectKey<USkeletalMeshComponent>(MeshComp), RuntimeState);
		}
	}
}

void UAnimNotifyState_BodySweepProfile::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if ((!bUseAnimatedProfileOffset && !bConstrainMeshMotionToBodySweep) || !MeshComp)
	{
		return;
	}

	FBodySweepProfileNotifyRuntime* RuntimeState = ActiveOffsetWindows.Find(
		TObjectKey<USkeletalMeshComponent>(MeshComp));
	UPragmataMonsterMovementComponent* Movement = GetMonsterMovementComponent(MeshComp);
	if (!RuntimeState || !Movement)
	{
		return;
	}

	RuntimeState->ElapsedTime = FMath::Min(
		RuntimeState->ElapsedTime + FrameDeltaTime,
		RuntimeState->TotalDuration);

	if (bUseAnimatedProfileOffset)
	{
		const FPragmataMonsterBodySweepProfile* BaseProfile = ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride
			? nullptr
			: Movement->GetBodySweepProfile(ProfileName);
		FPragmataMonsterBodySweepProfile NotifyOverrideProfile;
		if (ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride)
		{
			NotifyOverrideProfile = MakeNotifyOverrideProfile();
			BaseProfile = &NotifyOverrideProfile;
		}
		if (!BaseProfile)
		{
			return;
		}

		TArray<FVector> DesiredLocalOffsets;
		EvaluateAnimatedSphereOffsets(RuntimeState->ElapsedTime, *BaseProfile, DesiredLocalOffsets);
		Movement->UpdateBodySweepProfileSphereOffsets(ProfileName, DesiredLocalOffsets);
	}
	else if (bConstrainMeshMotionToBodySweep && bDriveFollowSphereFromTrackingBone)
	{
		TArray<FVector> DesiredLocalOffsets;
		if (Movement->GetActiveBodySweepProfileSphereOffsets(ProfileName, DesiredLocalOffsets)
			&& UpdateTrackingBoneDrivenSphereOffset(MeshComp, *RuntimeState, DesiredLocalOffsets))
		{
			Movement->UpdateBodySweepProfileSphereOffsets(ProfileName, DesiredLocalOffsets);
		}
	}

	if (bConstrainMeshMotionToBodySweep)
	{
		ApplyMeshConstraint(MeshComp, Movement, *RuntimeState);
	}
}

void UAnimNotifyState_BodySweepProfile::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (MeshComp)
	{
		if (const FBodySweepProfileNotifyRuntime* RuntimeState = ActiveOffsetWindows.Find(
			TObjectKey<USkeletalMeshComponent>(MeshComp)))
		{
			if (RuntimeState->bMeshConstraintInitialized)
			{
				MeshComp->SetRelativeLocation(
					RuntimeState->InitialMeshRelativeLocation,
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
		}
		ActiveOffsetWindows.Remove(TObjectKey<USkeletalMeshComponent>(MeshComp));
	}

	// 중첩 notify가 있어도 같은 profile의 마지막 요청 하나만 제거
	if (UPragmataMonsterMovementComponent* Movement = GetMonsterMovementComponent(MeshComp))
	{
		Movement->PopBodySweepProfile(ProfileName);
	}
}

FString UAnimNotifyState_BodySweepProfile::GetNotifyName_Implementation() const
{
	return ProfileName.IsNone()
		? TEXT("Monster Body Sweep")
		: FString::Printf(TEXT("Body Sweep: %s"), *ProfileName.ToString());
}

FPragmataMonsterBodySweepProfile UAnimNotifyState_BodySweepProfile::MakeNotifyOverrideProfile() const
{
	// Notify Details 값을 runtime movement와 editor preview가 공유할 profile로 구성
	FPragmataMonsterBodySweepProfile Profile;
	Profile.ProfileName = ProfileName;
	Profile.PreviewColor = OverridePreviewColor;
	if (!bUseAnimatedProfileOffset)
	{
		Profile.Spheres = OverrideSpheres;
		return Profile;
	}

	Profile.Spheres.Reserve(AnimatedSpheres.Num());
	for (const FPragmataAnimatedBodySweepSphere& AnimatedSphere : AnimatedSpheres)
	{
		FPragmataMonsterBodySweepSphere Sphere;
		Sphere.Radius = AnimatedSphere.Radius;
		// NotifyBegin에서 Return 위치부터 Start 위치까지 sweep해 초기 벽 중첩도 검사
		Sphere.LocalOffset = AnimatedSphere.ReturnLocalOffset;
		Profile.Spheres.Add(Sphere);
	}
	return Profile;
}

void UAnimNotifyState_BodySweepProfile::EvaluateAnimatedSphereOffsets(
	float ElapsedTime,
	const FPragmataMonsterBodySweepProfile& BaseProfile,
	TArray<FVector>& OutLocalOffsets
) const
{
	OutLocalOffsets.Reset(BaseProfile.Spheres.Num());
	if (!bUseAnimatedProfileOffset)
	{
		for (const FPragmataMonsterBodySweepSphere& Sphere : BaseProfile.Spheres)
		{
			OutLocalOffsets.Add(Sphere.LocalOffset);
		}
		return;
	}

	for (int32 SphereIndex = 0; SphereIndex < BaseProfile.Spheres.Num(); ++SphereIndex)
	{
		if (!AnimatedSpheres.IsValidIndex(SphereIndex))
		{
			OutLocalOffsets.Add(BaseProfile.Spheres[SphereIndex].LocalOffset);
			continue;
		}

		const FPragmataAnimatedBodySweepSphere& AnimatedSphere = AnimatedSpheres[SphereIndex];
		if (ElapsedTime < OffsetStartDelay)
		{
			OutLocalOffsets.Add(AnimatedSphere.StartLocalOffset);
			continue;
		}

		float RemainingTime = ElapsedTime - OffsetStartDelay;
		if (MoveToOffsetDuration > UE_KINDA_SMALL_NUMBER && RemainingTime < MoveToOffsetDuration)
		{
			OutLocalOffsets.Add(FMath::Lerp(
				AnimatedSphere.StartLocalOffset,
				AnimatedSphere.MaxLocalOffset,
				RemainingTime / MoveToOffsetDuration));
			continue;
		}

		RemainingTime -= MoveToOffsetDuration;
		if (RemainingTime < HoldOffsetDuration)
		{
			OutLocalOffsets.Add(AnimatedSphere.MaxLocalOffset);
			continue;
		}

		RemainingTime -= HoldOffsetDuration;
		if (ReturnToBaseDuration > UE_KINDA_SMALL_NUMBER && RemainingTime < ReturnToBaseDuration)
		{
			OutLocalOffsets.Add(FMath::Lerp(
				AnimatedSphere.MaxLocalOffset,
				AnimatedSphere.ReturnLocalOffset,
				RemainingTime / ReturnToBaseDuration));
			continue;
		}

		OutLocalOffsets.Add(AnimatedSphere.ReturnLocalOffset);
	}
}

bool UAnimNotifyState_BodySweepProfile::InitializeMeshConstraint(
	USkeletalMeshComponent* MeshComp,
	const UPragmataMonsterMovementComponent* Movement,
	FBodySweepProfileNotifyRuntime& RuntimeState
) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!MeshComp || !Movement || !Owner)
	{
		return false;
	}

	FVector SphereWorldLocation;
	if (!Movement->GetActiveBodySweepSphereWorldLocation(
		ProfileName,
		MeshFollowSphereIndex,
		SphereWorldLocation))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BodySweepProfile] Invalid MeshFollowSphereIndex %d for profile '%s'"),
			MeshFollowSphereIndex,
			*ProfileName.ToString());
		return false;
	}

	FName TrackingBoneName = MeshTrackingBoneName;
	if (TrackingBoneName.IsNone())
	{
		TrackingBoneName = MeshComp->GetBoneName(0);
	}
	if (TrackingBoneName.IsNone() || !MeshComp->DoesSocketExist(TrackingBoneName))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BodySweepProfile] Missing mesh tracking bone/socket '%s' on '%s'"),
			*TrackingBoneName.ToString(),
			*GetNameSafe(MeshComp));
		return false;
	}

	const FTransform OwnerTransform = Owner->GetActorTransform();
	RuntimeState.InitialMeshRelativeLocation = MeshComp->GetRelativeLocation();
	RuntimeState.InitialMeshRelativeTransform = MeshComp->GetRelativeTransform();
	RuntimeState.InitialTrackingBoneOwnerLocalLocation = RuntimeState.InitialMeshRelativeTransform.TransformPosition(
		MeshComp->GetSocketTransform(TrackingBoneName, RTS_Component).GetLocation());
	if (bAttachFollowSphereToTrackingBone)
	{
		const FTransform TrackingBoneComponentTransform = MeshComp->GetSocketTransform(
			TrackingBoneName,
			RTS_Component);
		RuntimeState.InitialFollowSphereLocalOffset = RuntimeState.InitialMeshRelativeTransform.TransformPosition(
			TrackingBoneComponentTransform.TransformPosition(TrackingBoneSphereLocalOffset));
	}
	else
	{
		RuntimeState.InitialFollowSphereLocalOffset = OwnerTransform.InverseTransformPositionNoScale(
			SphereWorldLocation);
	}
	RuntimeState.TrackingBoneToSphereLocalOffset =
		RuntimeState.InitialTrackingBoneOwnerLocalLocation - RuntimeState.InitialFollowSphereLocalOffset;
	RuntimeState.ResolvedTrackingBoneName = TrackingBoneName;
	RuntimeState.bMeshConstraintInitialized = true;
	return true;
}

// component-space bone delta만 사용해 이전 mesh 역보정이 sphere 목표에 다시 섞이지 않게 함
bool UAnimNotifyState_BodySweepProfile::UpdateTrackingBoneDrivenSphereOffset(
	USkeletalMeshComponent* MeshComp,
	const FBodySweepProfileNotifyRuntime& RuntimeState,
	TArray<FVector>& InOutLocalOffsets
) const
{
	if (!RuntimeState.bMeshConstraintInitialized || !MeshComp
		|| !InOutLocalOffsets.IsValidIndex(MeshFollowSphereIndex))
	{
		return false;
	}

	const FTransform CurrentTrackingBoneComponentTransform = MeshComp->GetSocketTransform(
		RuntimeState.ResolvedTrackingBoneName,
		RTS_Component);
	const FVector CurrentTrackingBoneOwnerLocalLocation =
		RuntimeState.InitialMeshRelativeTransform.TransformPosition(
			CurrentTrackingBoneComponentTransform.GetLocation());
	if (bAttachFollowSphereToTrackingBone)
	{
		FVector DesiredSphereOwnerLocalLocation = RuntimeState.InitialMeshRelativeTransform.TransformPosition(
			CurrentTrackingBoneComponentTransform.TransformPosition(TrackingBoneSphereLocalOffset));
		DesiredSphereOwnerLocalLocation.X = FMath::Lerp(
			RuntimeState.InitialFollowSphereLocalOffset.X,
			DesiredSphereOwnerLocalLocation.X,
			FMath::Clamp(MeshCompensationAxisMask.X, 0.f, 1.f));
		DesiredSphereOwnerLocalLocation.Y = FMath::Lerp(
			RuntimeState.InitialFollowSphereLocalOffset.Y,
			DesiredSphereOwnerLocalLocation.Y,
			FMath::Clamp(MeshCompensationAxisMask.Y, 0.f, 1.f));
		DesiredSphereOwnerLocalLocation.Z = FMath::Lerp(
			RuntimeState.InitialFollowSphereLocalOffset.Z,
			DesiredSphereOwnerLocalLocation.Z,
			FMath::Clamp(MeshCompensationAxisMask.Z, 0.f, 1.f));
		InOutLocalOffsets[MeshFollowSphereIndex] = DesiredSphereOwnerLocalLocation;
		return true;
	}

	FVector BoneDelta = CurrentTrackingBoneOwnerLocalLocation
		- RuntimeState.InitialTrackingBoneOwnerLocalLocation;
	BoneDelta.X *= FMath::Clamp(MeshCompensationAxisMask.X, 0.f, 1.f);
	BoneDelta.Y *= FMath::Clamp(MeshCompensationAxisMask.Y, 0.f, 1.f);
	BoneDelta.Z *= FMath::Clamp(MeshCompensationAxisMask.Z, 0.f, 1.f);
	InOutLocalOffsets[MeshFollowSphereIndex] = RuntimeState.InitialFollowSphereLocalOffset + BoneDelta;
	return true;
}

// animation bone 진행은 유지하고 MeshComponent만 역이동해 tracking bone이 collision sphere를 넘지 않게 고정
void UAnimNotifyState_BodySweepProfile::ApplyMeshConstraint(
	USkeletalMeshComponent* MeshComp,
	const UPragmataMonsterMovementComponent* Movement,
	const FBodySweepProfileNotifyRuntime& RuntimeState
) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!RuntimeState.bMeshConstraintInitialized || !MeshComp || !Movement || !Owner)
	{
		return;
	}

	FVector SphereWorldLocation;
	if (!Movement->GetActiveBodySweepSphereWorldLocation(
		ProfileName,
		MeshFollowSphereIndex,
		SphereWorldLocation))
	{
		return;
	}

	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector TargetBoneWorldLocation = SphereWorldLocation
		+ OwnerTransform.TransformVectorNoScale(RuntimeState.TrackingBoneToSphereLocalOffset);
	FVector CorrectionLocal = OwnerTransform.InverseTransformVectorNoScale(
		TargetBoneWorldLocation - MeshComp->GetSocketLocation(RuntimeState.ResolvedTrackingBoneName));
	CorrectionLocal.X *= FMath::Clamp(MeshCompensationAxisMask.X, 0.f, 1.f);
	CorrectionLocal.Y *= FMath::Clamp(MeshCompensationAxisMask.Y, 0.f, 1.f);
	CorrectionLocal.Z *= FMath::Clamp(MeshCompensationAxisMask.Z, 0.f, 1.f);
	if (CorrectionLocal.IsNearlyZero())
	{
		return;
	}

	MeshComp->AddWorldOffset(
		OwnerTransform.TransformVectorNoScale(CorrectionLocal),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

#if WITH_EDITOR
void UAnimNotifyState_BodySweepProfile::DrawInEditor(
	FPrimitiveDrawInterface* PDI,
	USkeletalMeshComponent* MeshComp,
	const UAnimSequenceBase*,
	const FAnimNotifyEvent& NotifyEvent
) const
{
#if WITH_EDITORONLY_DATA
	if (!PDI || !MeshComp || ProfileName.IsNone())
	{
		return;
	}

	const UAnimSingleNodeInstance* PreviewInstance = MeshComp->GetSingleNodeInstance();
	if (bPreviewOnlyNearNotifyTime)
	{
		if (!PreviewInstance)
		{
			return;
		}

		const float PreviewTime = PreviewInstance->GetCurrentTime();
		const float PreviewStartTime = NotifyEvent.GetTriggerTime() - PreviewTimeTolerance;
		const float PreviewEndTime = NotifyEvent.GetEndTriggerTime() + PreviewTimeTolerance;
		if (PreviewTime < PreviewStartTime || PreviewTime > PreviewEndTime)
		{
			return;
		}
	}

	const UPragmataMonsterMovementComponent* PreviewMovement = nullptr;
	const AMonsterBase* PreviewMonster = nullptr;
	if (!PreviewMonsterClass.IsNull())
	{
		if (const UClass* MonsterClass = PreviewMonsterClass.LoadSynchronous())
		{
			PreviewMonster = MonsterClass->GetDefaultObject<AMonsterBase>();
			PreviewMovement = PreviewMonster
				? Cast<UPragmataMonsterMovementComponent>(PreviewMonster->GetCharacterMovement())
				: nullptr;
		}
	}
	else
	{
		PreviewMovement = GetMonsterMovementComponent(MeshComp);
	}

	FPragmataMonsterBodySweepProfile NotifyOverrideProfile;
	const FPragmataMonsterBodySweepProfile* Profile = nullptr;
	if (ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride)
	{
		NotifyOverrideProfile = MakeNotifyOverrideProfile();
		Profile = &NotifyOverrideProfile;
	}
	else if (PreviewMovement)
	{
		Profile = PreviewMovement->GetBodySweepProfile(ProfileName);
	}
	if (!Profile)
	{
		return;
	}

	const float PreviewTime = PreviewInstance
		? PreviewInstance->GetCurrentTime()
		: NotifyEvent.GetTriggerTime();
	const float PreviewElapsedTime = FMath::Max(
		PreviewTime - NotifyEvent.GetTriggerTime(),
		0.f);
	TArray<FVector> PreviewLocalOffsets;
	EvaluateAnimatedSphereOffsets(PreviewElapsedTime, *Profile, PreviewLocalOffsets);

	const ACharacter* PreviewOwnerCharacter = MeshComp->GetOwner<ACharacter>();
	const USkeletalMeshComponent* ClassMesh = PreviewMonster ? PreviewMonster->GetMesh() : nullptr;
	FName PreviewTrackingBoneName = MeshTrackingBoneName;
	if (PreviewTrackingBoneName.IsNone())
	{
		PreviewTrackingBoneName = MeshComp->GetBoneName(0);
	}
	for (int32 SphereIndex = 0; SphereIndex < Profile->Spheres.Num(); ++SphereIndex)
	{
		const FPragmataMonsterBodySweepSphere& Sphere = Profile->Spheres[SphereIndex];
		if (Sphere.Radius <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector SphereLocalOffset = PreviewLocalOffsets.IsValidIndex(SphereIndex)
			? PreviewLocalOffsets[SphereIndex]
			: Sphere.LocalOffset;
		FVector Center = FVector::ZeroVector;
		if (bConstrainMeshMotionToBodySweep
			&& bDriveFollowSphereFromTrackingBone
			&& bAttachFollowSphereToTrackingBone
			&& SphereIndex == MeshFollowSphereIndex
			&& !PreviewTrackingBoneName.IsNone()
			&& MeshComp->DoesSocketExist(PreviewTrackingBoneName))
		{
			// montage preview에서도 bone 기준 sphere 위치를 현재 pose에 맞춰 표시
			Center = MeshComp->GetSocketTransform(PreviewTrackingBoneName, RTS_World).TransformPosition(
				TrackingBoneSphereLocalOffset);
		}
		else if (ClassMesh)
		{
			// class mesh의 capsule 상대 transform을 역변환해 montage preview mesh 공간에 맞춤
			const FVector MeshLocalCenter = ClassMesh->GetRelativeTransform().InverseTransformPosition(
				SphereLocalOffset);
			Center = MeshComp->GetComponentTransform().TransformPosition(MeshLocalCenter);
		}
		else if (PreviewOwnerCharacter)
		{
			Center = PreviewOwnerCharacter->GetActorTransform().TransformPositionNoScale(
				SphereLocalOffset);
		}
		else
		{
			continue;
		}

		DrawWireSphereAutoSides(
			PDI,
			Center,
			Profile->PreviewColor,
			Sphere.Radius,
			SDPG_World,
			PreviewLineThickness);
	}
#endif
}
#endif
