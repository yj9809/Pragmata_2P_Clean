#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StasisNetField.generated.h"

class AController;
class AMonsterBase;
class USphereComponent;

UCLASS()
class PRAGMATA_2P_API AStasisNetField : public AActor
{
	GENERATED_BODY()

public:
	AStasisNetField();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeField(
		AActor* InSourceActor,
		AController* InInstigatorController,
		float InBaseDamage,
		float InHackedDamage,
		float InFieldRadius,
		float InFieldDuration,
		float InStunDuration,
		float InDamageInterval);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stasis")
	TObjectPtr<USphereComponent> FieldCollision = nullptr;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float FieldRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float FieldDuration = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float StunDuration = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float DamageInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0"))
	float BaseDotDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis", meta=(ClampMin="0.0", UIMin="0.0"))
	float HackedDotDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis|Debug", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float DebugDrawLifetime = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stasis|Debug", meta=(ClampMin="0.0", UIMin="0.0"))
	float DebugLineThickness = 1.5f;

private:
	UFUNCTION()
	void HandleFieldBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleFieldEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void AddMonster(AMonsterBase* Monster);
	void RemoveMonster(AMonsterBase* Monster);
	void RemoveAllMonsters();
	void ApplyDotDamage();
	void DrawFieldDebug() const;

	UPROPERTY()
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY()
	TObjectPtr<AController> InstigatorController = nullptr;

	UPROPERTY()
	TSet<TObjectPtr<AMonsterBase>> ActiveMonsters;

	FTimerHandle DotDamageTimerHandle;
};
