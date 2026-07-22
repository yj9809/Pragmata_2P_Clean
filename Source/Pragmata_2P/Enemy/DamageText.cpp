// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/DamageText.h"
#include "Components/WidgetComponent.h"
#include "UI/DamageTextWidget.h" // Include the widget header to allow casting

// Sets default values
ADamageText::ADamageText()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>("WidgetComponent");

	RootComponent = WidgetComponent;

	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetDrawAtDesiredSize(true);
	WidgetComponent->SetDrawSize(FVector2D(120.0f, 60.0f));
}

// Called when the game starts or when spawned
void ADamageText::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);
}

// Called every frame
void ADamageText::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADamageText::Initialize(float DamageAmount)
{
	if (WidgetComponent)
	{
		WidgetComponent->InitWidget();
		if (WidgetComponent->GetWidgetClass() == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Widget Class is completely EMPTY in the Blueprint Editor!"));
		}

		UDamageTextWidget* Widget = Cast<UDamageTextWidget>(WidgetComponent->GetUserWidgetObject());
		if (Widget)
		{
			Widget->SetDamageText(DamageAmount);
			Widget->PlayFloatAndFadeAnimation();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DamageFont] Init 실패 - Widget 없음 (BP에서 Widget Class 지정 확인)"));
		}
	}
}