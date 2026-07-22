// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"
#include "Data/PragmataTypes.h"
#include "HackingPuzzleViewModel.generated.h"

class UMapHackingPuzzleComponent;

UCLASS()
class PRAGMATA_2P_API UHackingPuzzleViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	bool GetIsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	ESlateVisibility GetIsActiveVisibility() const { return IsActiveVisibility; }

	// 해킹 시작 시 토글 — 진입 애니메이션 바인딩용
	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	bool GetStartedTrigger() const { return bStartedTrigger; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	int32 GetCurrentDepth() const { return CurrentDepth; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	EHackDirection GetCurrentDirection() const { return CurrentDirection; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	bool GetIsSolved() const { return bIsSolved; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	bool GetResetTrigger() const { return bResetTrigger; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	bool GetSolvedTrigger() const { return bSolvedTrigger; }

	// 성공 애니메이션 종료 시 WBP에서 호출
	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM")
	void OnSuccessAnimationFinished();

	// 실패 애니메이션 종료 시 WBP에서 호출
	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM")
	void OnFailureAnimationFinished();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	int32 GetCompletedDepthCount() const { return CompletedDepthCount; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	ESlateVisibility GetDepth1SuccessRingVisibility() const { return Depth1SuccessRingVisibility; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	ESlateVisibility GetDepth2SuccessRingVisibility() const { return Depth2SuccessRingVisibility; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	ESlateVisibility GetDepth3SuccessRingVisibility() const { return Depth3SuccessRingVisibility; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	float GetDepth1DirectionOpacity() const { return Depth1DirectionOpacity; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	float GetDepth2DirectionOpacity() const { return Depth2DirectionOpacity; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	float GetDepth3DirectionOpacity() const { return Depth3DirectionOpacity; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	EHackDirection GetDirection0() const { return Direction0; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	EHackDirection GetDirection1() const { return Direction1; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	EHackDirection GetDirection2() const { return Direction2; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	int32 GetDirection0Index() const { return Direction0Index; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	int32 GetDirection1Index() const { return Direction1Index; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HackVM")
	int32 GetDirection2Index() const { return Direction2Index; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM")
	void BindToComponent(UMapHackingPuzzleComponent* PuzzleComp);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM")
	void UnbindFromComponent();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM", meta=(DeprecatedFunction, DeprecationMessage="Prefer BindToComponent; this legacy wrapper still starts gameplay for standalone compatibility."))
	void StartMapHacking(UMapHackingPuzzleComponent* PuzzleComp);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackVM", meta=(DeprecatedFunction, DeprecationMessage="Prefer UnbindFromComponent; this legacy wrapper still cancels gameplay for standalone compatibility."))
	void StopMapHacking();

private:
	void HandleHackStarted();
	void HandleDepthSolved(int32 SolvedDepth);
	void HandleDepthChanged(int32 Depth, EHackDirection Direction);
	void HandleSequenceReady(const TArray<EHackDirection>& Sequence);
	void HandleHackReset();
	void HandleHackCompleted();

	void SyncFromComponent();

	void SetIsActive(bool bNewValue);
	void SetStartedTrigger(bool bNewValue);
	void SetCurrentDepth(int32 NewDepth);
	void SetCurrentDirection(EHackDirection NewDir);
	void SetIsSolved(bool bNewValue);
	void SetResetTrigger(bool bNewValue);
	void SetSolvedTrigger(bool bNewValue);
	void SetCompletedDepthCount(int32 NewCount);
	void SetDirection0(EHackDirection NewDir);
	void SetDirection1(EHackDirection NewDir);
	void SetDirection2(EHackDirection NewDir);
	static int32 DirectionToWidgetIndex(EHackDirection Direction);

private:
	// 해킹 UI 활성화 여부
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	bool bIsActive = false;

	// 토글 방식 트리거 — 진입(시작) 애니메이션
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	bool bStartedTrigger = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility IsActiveVisibility = ESlateVisibility::Collapsed;

	// 현재 뎁스 (0~2)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	int32 CurrentDepth = 0;

	// 현재 정답 방향
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	EHackDirection CurrentDirection = EHackDirection::None;

	// 퍼즐 완료 여부
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	bool bIsSolved = false;

	// 실패 애니메이션 트리거 (toggle 방식)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	bool bResetTrigger = false;

	// 전체 성공 애니메이션 트리거 (toggle 방식)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	bool bSolvedTrigger = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	int32 CompletedDepthCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility Depth1SuccessRingVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility Depth2SuccessRingVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility Depth3SuccessRingVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	float Depth1DirectionOpacity = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	float Depth2DirectionOpacity = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	float Depth3DirectionOpacity = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	EHackDirection Direction0 = EHackDirection::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	EHackDirection Direction1 = EHackDirection::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	EHackDirection Direction2 = EHackDirection::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	int32 Direction0Index = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	int32 Direction1Index = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HackVM", meta = (AllowPrivateAccess = "true"))
	int32 Direction2Index = 0;

	TWeakObjectPtr<UMapHackingPuzzleComponent> BoundPuzzleComp;
};
