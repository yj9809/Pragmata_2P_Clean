#include "Animation/AnimNotify_AttackHitOverlap.h"

#include "Combat/PragmataDamageLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PragmataDamageSource.h"
#if WITH_EDITOR
#include "Animation/AnimSingleNodeInstance.h"
#include "PrimitiveDrawingUtils.h"
#endif

// 타격 프레임의 socket 주변 대상에게 server 권한 damage를 1회 전달
void UAnimNotify_AttackHitOverlap::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!CanRunServerHitOverlap(MeshComp))
	{
		return;
	}

	float SourceDamageAmount = 0.f;
	if (!TryGetSourceDamage(MeshComp, SourceDamageAmount))
	{
		return;
	}

	// socket 위치와 회전을 그대로 사용해 공격 부위에 box 정렬
	FTransform OverlapTransform;
	if (!TryGetOverlapTransform(MeshComp, OverlapTransform))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UAnimNotify_AttackHitOverlap::Notify] Socket '%s' not found on '%s'"),
			*AttackSocketName.ToString(),
			*GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr));
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackHitOverlap), false);
	if (bIgnoreOwner && MeshComp->GetOwner())
	{
		Params.AddIgnoredActor(MeshComp->GetOwner());
	}

	// 타격 프레임에 box 범위와 겹친 collision component를 한 번 조회
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		OverlapTransform.GetLocation(),
		OverlapTransform.GetRotation(),
		TraceChannel.GetValue(),
		FCollisionShape::MakeBox(HitBoxExtent),
		Params);

	// 한 actor에 여러 collision component가 있어도 damage는 한 번만 전달
	TSet<TObjectKey<AActor>> ProcessedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor)
		{
			continue;
		}

		const TObjectKey<AActor> HitActorKey(HitActor);
		if (ProcessedActors.Contains(HitActorKey))
		{
			continue;
		}

		ProcessedActors.Add(HitActorKey);
		DealDamageFromOverlap(MeshComp, Overlap, OverlapTransform.GetLocation(), SourceDamageAmount);
	}
}

#if WITH_EDITOR
void UAnimNotify_AttackHitOverlap::DrawInEditor(
	FPrimitiveDrawInterface* PDI,
	USkeletalMeshComponent* MeshComp,
	const UAnimSequenceBase*,
	const FAnimNotifyEvent& NotifyEvent
) const
{
	if (!PDI || HitBoxExtent.GetMin() <= 0.f)
	{
		return;
	}

	if (bPreviewOnlyNearNotifyTime)
	{
		const UAnimSingleNodeInstance* PreviewInstance = MeshComp ? MeshComp->GetSingleNodeInstance() : nullptr;
		if (!PreviewInstance)
		{
			return;
		}

		// point notify는 길이가 없으므로 설정 시간만큼 trigger time 주변에 표시
		const float PreviewTime = PreviewInstance->GetCurrentTime();
		const float NotifyTime = NotifyEvent.GetTriggerTime();
		if (!FMath::IsNearlyEqual(PreviewTime, NotifyTime, PreviewTimeTolerance))
		{
			return;
		}
	}

	FTransform OverlapTransform;
	if (!TryGetOverlapTransform(MeshComp, OverlapTransform))
	{
		return;
	}

	// runtime box와 같은 socket 위치와 회전으로 montage preview에 표시
	const FBox LocalBox(-HitBoxExtent, HitBoxExtent);
	DrawWireBox(
		PDI,
		OverlapTransform.ToMatrixNoScale(),
		LocalBox,
		PreviewColor,
		SDPG_World,
		2.f);
}
#endif

bool UAnimNotify_AttackHitOverlap::CanRunServerHitOverlap(const USkeletalMeshComponent* MeshComp) const
{
	const AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;

	// client montage preview나 잘못된 collision 설정에서는 gameplay 판정 차단
	return OwnerActor && OwnerActor->HasAuthority() && HitBoxExtent.GetMin() > 0.f;
}

bool UAnimNotify_AttackHitOverlap::TryGetSourceDamage(
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
			TEXT("[UAnimNotify_AttackHitOverlap::TryGetSourceDamage] Owner '%s' does not implement PragmataDamageSource"),
			*GetNameSafe(OwnerActor));
		return false;
	}

	OutDamageAmount = IPragmataDamageSource::Execute_GetCurrentAttackDamage(OwnerActor);
	if (OutDamageAmount <= 0.f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UAnimNotify_AttackHitOverlap::TryGetSourceDamage] Invalid damage %.2f from '%s'"),
			OutDamageAmount,
			*GetNameSafe(OwnerActor));
		return false;
	}

	return true;
}

bool UAnimNotify_AttackHitOverlap::TryGetOverlapTransform(const USkeletalMeshComponent* MeshComp,FTransform& OutTransform) const
{
	if (!MeshComp)
	{
		return false;
	}

	if (AttackSocketName.IsNone())
	{
		// socket 미지정 공격은 mesh component transform을 기준으로 사용
		OutTransform = MeshComp->GetComponentTransform();
		return true;
	}

	if (!MeshComp->DoesSocketExist(AttackSocketName))
	{
		return false;
	}

	OutTransform = MeshComp->GetSocketTransform(AttackSocketName, RTS_World);
	return true;
}

void UAnimNotify_AttackHitOverlap::DealDamageFromOverlap(
	USkeletalMeshComponent* MeshComp,
	const FOverlapResult& Overlap,
	const FVector& OverlapLocation,
	float SourceDamageAmount
) const
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	AActor* HitActor = Overlap.GetActor();
	if (!OwnerActor || !HitActor || (bIgnoreOwner && OwnerActor == HitActor))
	{
		return;
	}

	UPrimitiveComponent* HitComponent = Overlap.GetComponent();

	// overlap은 contact point가 없으므로 hit component 중심으로 impact 정보 구성
	const FVector HitLocation = HitComponent ? HitComponent->Bounds.Origin : HitActor->GetActorLocation();
	const FVector ImpactDirection = (HitLocation - OwnerActor->GetActorLocation()).GetSafeNormal();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = SourceDamageAmount;
	DamageSpec.SourceActor = OwnerActor;
	DamageSpec.DamageCauser = OwnerActor;
	DamageSpec.InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	DamageSpec.DamageKind = DamageKind;
	DamageSpec.HitReactStrength = HitReactStrength;
	DamageSpec.HitDirection = HitDirection;
	DamageSpec.bHasImpact = true;
	DamageSpec.ImpactHit = FHitResult(HitActor, HitComponent, HitLocation, -ImpactDirection);
	DamageSpec.ImpactHit.TraceStart = OverlapLocation;
	DamageSpec.ImpactHit.TraceEnd = HitLocation;
	DamageSpec.ImpactDirection = ImpactDirection;

	// faction 검사와 실제 health 변경은 프로젝트 공통 damage 경로에 위임
	UPragmataDamageLibrary::DealDamageToActor(HitActor, DamageSpec);
}
