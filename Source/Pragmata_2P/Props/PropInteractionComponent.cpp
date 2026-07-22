// Fill out your copyright notice in the Description page of Project Settings.


#include "Props/PropInteractionComponent.h"
#include "PropInteractableInterface.h"

UPropInteractionComponent::UPropInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // 성능 최적화를 위해 틱 비활성화
}

void UPropInteractionComponent::TriggerInteraction(AActor* Interactor)
{
	AActor* Owner = GetOwner();

	// 부모 액터가 유효하고, IInteractableInterface를 상속받아 구현했는지 검사
	if (Owner && Owner->Implements<UPropInteractableInterface>())
	{
		// 인터페이스 함수 실행 (블루프린트/C++ 구현체 모두 안전하게 호출됨)
		IPropInteractableInterface::Execute_Interact(Owner, Interactor);
	}
}