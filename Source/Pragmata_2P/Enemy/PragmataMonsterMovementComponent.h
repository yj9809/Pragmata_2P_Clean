#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PragmataMonsterMovementComponent.generated.h"

// 루트 캡슐 기준으로 배치할 단일 body sweep sphere
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataMonsterBodySweepSphere
{
	GENERATED_BODY()

public:
	// 루트 캡슐 중심 기준 상대 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep")
	FVector LocalOffset = FVector::ZeroVector;

	// 이동 중 벽을 검사할 sphere 반지름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float Radius = 30.f;
};

// 자세나 공격 상태별로 사용할 body sweep sphere 묶음
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataMonsterBodySweepProfile
{
	GENERATED_BODY()

public:
	// AnimNotifyState와 기본 활성 설정에서 조회할 profile 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep")
	FName ProfileName = NAME_None;

	// 긴 몸체를 앞·중앙·뒤로 근사할 sphere 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep")
	TArray<FPragmataMonsterBodySweepSphere> Spheres;

	// Blueprint 또는 Level Editor viewport에서 현재 profile 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep|Preview")
	bool bPreviewInEditor = false;

	// Editor와 runtime debug sphere 색상
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep|Preview")
	FLinearColor PreviewColor = FLinearColor(0.f, 0.8f, 1.f, 1.f);
};

UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UPragmataMonsterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPragmataMonsterMovementComponent();

	// notify window가 요청한 profile을 최상위 활성 상태로 추가
	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Body Sweep")
	bool PushBodySweepProfile(FName ProfileName);

	// montage 전용 sphere 데이터를 BP profile 복사 없이 활성 stack에 추가
	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Body Sweep")
	bool PushBodySweepProfileOverride(const FPragmataMonsterBodySweepProfile& Profile);

	// notify window 종료 시 같은 profile의 마지막 요청을 제거
	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Body Sweep")
	void PopBodySweepProfile(FName ProfileName);

	// animation-only sphere 이동은 root Capsule을 밀지 않고 충돌 지점까지만 허용
	bool UpdateBodySweepProfileSphereOffsets(
		FName ProfileName,
		const TArray<FVector>& DesiredLocalOffsets
	);

	// Notify 시작 frame의 기준 sphere 위치는 sweep 없이 현재 pose에 맞춤
	bool SetBodySweepProfileSphereOffsets(
		FName ProfileName,
		const TArray<FVector>& LocalOffsets
	);

	// 현재 이동에 적용되는 profile 이름 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Body Sweep")
	FName GetActiveBodySweepProfileName() const;

	// montage editor preview가 지정한 monster class의 profile 데이터를 조회
	const FPragmataMonsterBodySweepProfile* GetBodySweepProfile(FName ProfileName) const;
	// Notify의 bone-driven sphere가 현재 profile 위치를 기준으로 다음 목표를 구성
	bool GetActiveBodySweepProfileSphereOffsets(
		FName ProfileName,
		TArray<FVector>& OutLocalOffsets
	) const;
	// Notify가 mesh tracking 기준으로 사용할 활성 sphere의 world 중심 조회
	bool GetActiveBodySweepSphereWorldLocation(
		FName ProfileName,
		int32 SphereIndex,
		FVector& OutWorldLocation
	) const;

	// Editor viewport와 PIE debug sphere 갱신
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// 루트 캡슐 이동 전에 자세별 sphere를 검사하고 같은 hit를 기존 slide 경로로 전달
	virtual bool MoveUpdatedComponentImpl(
		const FVector& Delta,
		const FQuat& NewRotation,
		bool bSweep,
		FHitResult* OutHit = nullptr,
		ETeleportType Teleport = ETeleportType::None
	) override;

private:
	const FPragmataMonsterBodySweepProfile* FindBodySweepProfile(FName ProfileName) const;
	FPragmataMonsterBodySweepProfile* FindActiveBodySweepProfile(FName ProfileName);
	const FPragmataMonsterBodySweepProfile* GetActiveBodySweepProfile() const;
	bool ShouldApplyBodySweep(bool bSweep, ETeleportType Teleport) const;
	bool SweepBodyProfile(
		const FPragmataMonsterBodySweepProfile& Profile,
		const FVector& Delta,
		const FQuat& NewRotation,
		FHitResult& OutHit,
		float& OutProbeTravelDistance
	) const;
	void DrawBodySweepProfile(const FPragmataMonsterBodySweepProfile& Profile) const;
	void DrawBodySweepDebug() const;

	// 모든 몬스터 Blueprint가 체형별로 구성할 자세 profile
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Body Sweep", meta=(AllowPrivateAccess="true"))
	TArray<FPragmataMonsterBodySweepProfile> BodySweepProfiles;

	// notify가 활성화되지 않았을 때 지속적으로 사용할 profile
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Body Sweep", meta=(AllowPrivateAccess="true"))
	FName DefaultBodySweepProfileName = NAME_None;

	// 게임 중 활성 profile을 PIE viewport에 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Body Sweep|Debug", meta=(AllowPrivateAccess="true"))
	bool bDrawActiveBodySweepInGame = false;

	// 충돌 지점보다 조금 앞에서 멈춰 반복 penetration을 줄이는 여유 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Body Sweep", meta=(ClampMin="0.0", UIMin="0.0", Units="cm", AllowPrivateAccess="true"))
	float SweepSkinWidth = 1.f;

	// viewport sphere 선 굵기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Body Sweep|Debug", meta=(ClampMin="0.0", UIMin="0.0", AllowPrivateAccess="true"))
	float DebugLineThickness = 1.5f;

	// BP profile과 montage override를 같은 runtime 데이터로 복사해 중첩 순서 유지
	UPROPERTY(Transient)
	TArray<FPragmataMonsterBodySweepProfile> ActiveBodySweepProfileStack;
};
