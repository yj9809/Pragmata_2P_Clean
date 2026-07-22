#include "LightBase.h"
#include "Game/PragmataGameState.h" 
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"

ALightBase::ALightBase()
{
	// 틱 비활성화 (성능 최적화)
	PrimaryActorTick.bCanEverTick = false;

	// 컴포넌트 부착의 기준이 될 루트 컴포넌트 생성
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;
}

void ALightBase::BeginPlay()
{
	Super::BeginPlay();

	if (!bIsAlwaysOn)
	{
		if (APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>())
		{
			// 전원 상태 변경 델리게이트 구독
			GS->OnPowerStateChanged.AddDynamic(this, &ALightBase::HandlePowerStateChanged);

			// 초기 생성 시 현재 게임의 전원 상태 동기화
			HandlePowerStateChanged(GS->GetGlobalPowerState());
		}
	}
}

void ALightBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 메모리 누수 방지를 위한 구독 해제
	if (APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>())
	{
		GS->OnPowerStateChanged.RemoveDynamic(this, &ALightBase::HandlePowerStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ALightBase::HandlePowerStateChanged(bool bIsPowerOn)
{
	// 액터에 부착된 모든 하위 LightComponent를 찾아 가시성 설정
	// (PointLight, SpotLight, RectLight 모두 ULightComponent를 상속받으므로 전부 제어됨)
	TArray<ULightComponent*> LightComps;
	GetComponents<ULightComponent>(LightComps);

	for (ULightComponent* Light : LightComps)
	{
		if (Light)
		{
			Light->SetVisibility(bIsPowerOn);
		}
	}
}