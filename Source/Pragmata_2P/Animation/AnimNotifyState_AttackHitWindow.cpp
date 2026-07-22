#include "Animation/AnimNotifyState_AttackHitWindow.h"

#include "Combat/PragmataDamageLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PragmataDamageSource.h"
#if WITH_EDITOR
#include "Animation/AnimSingleNodeInstance.h"
#include "PrimitiveDrawingUtils.h"
#endif

void UAnimNotifyState_AttackHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!CanRunServerHitWindow(MeshComp))
	{
		return;
	}

	float SourceDamageAmount = 0.f;
	if (!TryGetSourceDamage(MeshComp, SourceDamageAmount))
	{
		return;
	}

	FAttackHitWindowRuntime RuntimeState;
	RuntimeState.PreviousSocketLocation = GetAttackSocketLocation(MeshComp);
	RuntimeState.SourceDamageAmount = SourceDamageAmount;
	ActiveWindows.Add(TObjectKey<USkeletalMeshComponent>(MeshComp), MoveTemp(RuntimeState));
}

void UAnimNotifyState_AttackHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!CanRunServerHitWindow(MeshComp))
	{
		return;
	}

	FAttackHitWindowRuntime* RuntimeState = ActiveWindows.Find(TObjectKey<USkeletalMeshComponent>(MeshComp));
	if (!RuntimeState)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CurrentSocketLocation = GetAttackSocketLocation(MeshComp);
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackHitWindowSweep), false, MeshComp->GetOwner());
	if (bIgnoreOwner && MeshComp->GetOwner())
	{
		Params.AddIgnoredActor(MeshComp->GetOwner());
	}

	// 빠른 melee animation도 놓치지 않도록 이전 socket 위치에서 현재 위치까지 sweep
	World->SweepMultiByChannel(
		Hits,
		RuntimeState->PreviousSocketLocation,
		CurrentSocketLocation,
		FQuat::Identity,
		TraceChannel.GetValue(),
		FCollisionShape::MakeSphere(HitRadius),
		Params);

	for (const FHitResult& Hit : Hits)
	{
		DealDamageFromHit(MeshComp, *RuntimeState, Hit);
	}

	RuntimeState->PreviousSocketLocation = CurrentSocketLocation;
}

void UAnimNotifyState_AttackHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp)
	{
		ActiveWindows.Remove(TObjectKey<USkeletalMeshComponent>(MeshComp));
	}
}

#if WITH_EDITOR
void UAnimNotifyState_AttackHitWindow::DrawInEditor(
	FPrimitiveDrawInterface* PDI,
	USkeletalMeshComponent* MeshComp,
	const UAnimSequenceBase*,
	const FAnimNotifyEvent& NotifyEvent
) const
{
	if (!PDI || !MeshComp || HitRadius <= 0.f)
	{
		return;
	}

	if (bPreviewOnlyNearNotifyTime)
	{
		const UAnimSingleNodeInstance* PreviewInstance = MeshComp->GetSingleNodeInstance();
		if (!PreviewInstance)
		{
			return;
		}

		// state notify는 시작부터 종료까지의 구간에 tolerance를 더해 표시
		const float PreviewTime = PreviewInstance->GetCurrentTime();
		const float PreviewStartTime = NotifyEvent.GetTriggerTime() - PreviewTimeTolerance;
		const float PreviewEndTime = NotifyEvent.GetEndTriggerTime() + PreviewTimeTolerance;
		if (PreviewTime < PreviewStartTime || PreviewTime > PreviewEndTime)
		{
			return;
		}
	}

	// runtime sweep과 같은 socket 위치와 반지름으로 montage preview에 표시
	DrawWireSphereAutoSides(
		PDI,
		GetAttackSocketLocation(MeshComp),
		PreviewColor,
		HitRadius,
		SDPG_World,
		2.f);
}
#endif

bool UAnimNotifyState_AttackHitWindow::CanRunServerHitWindow(const USkeletalMeshComponent* MeshComp) const
{
	const AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	return OwnerActor && OwnerActor->HasAuthority() && HitRadius > 0.f;
}

bool UAnimNotifyState_AttackHitWindow::TryGetSourceDamage(
	const USkeletalMeshComponent* MeshComp,
	float& OutDamageAmount
) const
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor || !OwnerActor->GetClass()->ImplementsInterface(UPragmataDamageSource::StaticClass()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UAnimNotifyState_AttackHitWindow::TryGetSourceDamage] Owner '%s' does not implement PragmataDamageSource"),
			*GetNameSafe(OwnerActor));
		return false;
	}

	OutDamageAmount = IPragmataDamageSource::Execute_GetCurrentAttackDamage(OwnerActor);
	if (OutDamageAmount <= 0.f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UAnimNotifyState_AttackHitWindow::TryGetSourceDamage] Invalid damage %.2f from '%s'"),
			OutDamageAmount,
			*GetNameSafe(OwnerActor));
		return false;
	}

	return true;
}

FVector UAnimNotifyState_AttackHitWindow::GetAttackSocketLocation(const USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return FVector::ZeroVector;
	}

	// socket 회전을 포함한 local offset을 runtime sweep과 editor preview에 동일하게 적용
	const FTransform AnchorTransform = MeshComp->DoesSocketExist(AttackSocketName)
		? MeshComp->GetSocketTransform(AttackSocketName, RTS_World)
		: MeshComp->GetComponentTransform();
	return AnchorTransform.TransformPosition(AttackSocketOffset);
}

void UAnimNotifyState_AttackHitWindow::DealDamageFromHit(USkeletalMeshComponent* MeshComp, FAttackHitWindowRuntime& RuntimeState, const FHitResult& Hit) const
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	AActor* HitActor = Hit.GetActor();
	if (!OwnerActor || !HitActor || RuntimeState.HitActors.Contains(TObjectKey<AActor>(HitActor)))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = RuntimeState.SourceDamageAmount;
	DamageSpec.SourceActor = OwnerActor;
	DamageSpec.DamageCauser = OwnerActor;
	DamageSpec.InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	DamageSpec.DamageKind = DamageKind;
	DamageSpec.HitReactStrength = HitReactStrength;
	DamageSpec.HitDirection = HitDirection;
	DamageSpec.bHasImpact = true;
	DamageSpec.ImpactHit = Hit;
	DamageSpec.ImpactDirection = (HitActor->GetActorLocation() - OwnerActor->GetActorLocation()).GetSafeNormal();

	if (UPragmataDamageLibrary::DealDamageToActor(HitActor, DamageSpec))
	{
		RuntimeState.HitActors.Add(TObjectKey<AActor>(HitActor));
	}
}
