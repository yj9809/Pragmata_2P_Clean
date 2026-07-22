// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Data/PragmataTypes.h"
#include "EnemyHackingPuzzleViewModel.generated.h"

class UEnemyHackingPuzzleComponent;

// 해킹 완료 시 최종 데미지를 실어 알림 — 진입점이 바인딩해 실제 액터에 데미지 적용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyHackCompletedWithDamage, float, FinalDamage);

UCLASS()
class PRAGMATA_2P_API UEnemyHackingPuzzleViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// 완료 판정 시 최종 데미지(GetProjectedDamage)를 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "Pragmata|EnemyHackVM")
	FOnEnemyHackCompletedWithDamage OnHackCompletedWithDamage;

	// --- FieldNotify Getters ---

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetIsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	int32 GetGridSize() const { return GridSize; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	int32 GetCursorRow() const { return CursorRow; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	int32 GetCursorCol() const { return CursorCol; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetCompletedTrigger() const { return bCompletedTrigger; }

	// 해킹 시작 시 토글 — 진입 애니메이션 바인딩용 (bCompletedTrigger와 동일 패턴)
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetStartedTrigger() const { return bStartedTrigger; }

	// 해킹 취소 시 토글 — 취소 애니메이션 바인딩용 (완료와 구분됨)
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetCancelledTrigger() const { return bCancelledTrigger; }

	// Corrupted 노드 위에서 탈출 시도 중(카운트 진행 중)일 때 true
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetCorruptedPending() const { return bCorruptedPending; }

	// 셀 상태 변경 시 토글 — Blueprint에서 이 값에 바인딩해 셀 위젯 갱신
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetGridRefreshTrigger() const { return bGridRefreshTrigger; }

	// 커서가 지나간 파란(Normal) 노드 수
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	int32 GetNormalNodesVisited() const { return NormalNodesVisited; }

	// 전체 파란 노드 수 (실제 배치)
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	int32 GetTotalNormalNodes() const { return TotalNormalNodes; }

	// 게이지 UI 바인딩용 진행률 (0~1, Visited / Total)
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetNormalProgressRatio() const { return NormalProgressRatio; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetDoorGaugeRatio() const { return DoorGaugeRatio; }

	// 적에게 입힐 예상 데미지 (파란 노드 진행에 따라 실시간 증가) — UI 바인딩용
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetProjectedDamage() const { return ProjectedDamage; }

	// 예상 데미지를 정수 반올림한 FText — TextBlock.Text에 변환 없이 직접 바인딩용
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	FText GetProjectedDamageText() const { return ProjectedDamageText; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetHackTimeRemainingRatio() const { return HackTimeRemainingRatio; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetHackTimeRemainingSeconds() const { return HackTimeRemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetHackCompletedHighlight() const { return bHackCompletedHighlight; }

	// 바인딩된 타겟 적의 현재 hacked 잔여 지속시간(초) — 위젯이 매 틱 읽어 게이지에 반영.
	// 몬스터 복제 상태가 진실원이므로 UI를 닫았다 열어도 정확한 잔량을 준다.
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetHackTargetRemainingSeconds() const;

	// 바인딩된 타겟 적의 이번 세션 적용 지속시간(초) — 게이지 분모.
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetHackTargetDuration() const;

	// hacked 지속시간 게이지 상한(초) — FullGaugeRatio 계산 기준(지속시간 / 상한).
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	float GetMaxHackDurationSeconds() const;

	// 이번 해킹 세션 시작 시점의 타겟 hacked 여부 — 셀이 색 분기(재해킹=노랑)에 사용. 세션 내 고정
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyHackVM")
	bool GetIsTargetHacked() const { return bTargetHacked; }

	// --- 셀 데이터 접근 (Blueprint에서 인덱스로 호출) ---

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	EHackNodeType GetNodeTypeAt(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	EHackCellState GetCellStateAt(int32 Index) const;

	// --- 생명주기 ---

	// 타겟 적을 받아 그 적의 퍼즐 컴포넌트에 바인딩 (null이면 해제) — 통합 진입점.
	// 호출 측(에임 트레이스 결과 / 복제된 해킹 대상 / 테스트)이 무엇이든 이 함수만 부르면 됨.
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void SetHackTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void BindToComponent(UEnemyHackingPuzzleComponent* PuzzleComp);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void UnbindFromComponent();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void SetHackTimeRemaining(float RemainingSeconds, float DurationSeconds, float FullGaugeRatio = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void StartEnemyHacking(UEnemyHackingPuzzleComponent* PuzzleComp);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void StopEnemyHacking();

	// 완료 애니메이션 종료 시 WBP에서 호출
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void OnCompletedAnimationFinished();

	// 취소 애니메이션 종료 시 WBP에서 호출 — 그때 패널 숨김
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackVM")
	void OnCancelledAnimationFinished();

private:
	void HandleHackStarted();
	void HandleGridReady(int32 InGridSize);
	void HandleCursorMoved(FIntPoint NewPos, bool bNewCorruptedPending);
	void HandleHackCompleted();
	void HandleHackCancelled();

	void SetIsActive(bool bNewValue);
	void SetGridSize(int32 NewSize);
	void SetCursorRow(int32 NewRow);
	void SetCursorCol(int32 NewCol);
	void SetCompletedTrigger(bool bNewValue);
	void SetStartedTrigger(bool bNewValue);
	void SetCancelledTrigger(bool bNewValue);
	void SetCorruptedPending(bool bNewValue);
	void SetTargetHacked(bool bNewValue);
	void SetGridRefreshTrigger(bool bNewValue);
	void SetNormalNodesVisited(int32 NewValue);
	void SetTotalNormalNodes(int32 NewValue);
	void SetNormalProgressRatio(float NewValue);
	void SetDoorGaugeRatio(float NewValue);
	void SetProjectedDamage(float NewValue);
	void SetProjectedDamageText(const FText& NewValue);
	void SetHackTimeRemainingRatio(float NewValue);
	void SetHackTimeRemainingSeconds(float NewValue);
	void SetHackCompletedHighlight(bool bNewValue);
	void RefreshDoorGaugeRatio();
	void UpdateNormalProgress();
	void UpdateProjectedDamage();

	void SyncFromComponent();
	void SyncVisualSnapshotFromComponent();
	void RebuildCellCache();
	bool IsBoundPuzzleActive() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	int32 GridSize = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	int32 CursorRow = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	int32 CursorCol = 0;

	// 토글 방식 트리거 — 완료 애니메이션
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bCompletedTrigger = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bHackCompletedHighlight = false;

	// 토글 방식 트리거 — 진입(시작) 애니메이션
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bStartedTrigger = false;

	// 토글 방식 트리거 — 취소 애니메이션
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bCancelledTrigger = false;

	// Corrupted 노드 탈출 시도 중
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bCorruptedPending = false;

	// 이번 세션 시작 시점의 타겟 hacked 여부 (시작 시 1회 샘플, 세션 내 불변) — 셀 색 분기용
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bTargetHacked = false;

	// 토글 방식 트리거 — 셀 상태 변경 시 Blueprint 갱신 신호
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	bool bGridRefreshTrigger = false;

	// 파란(Normal) 노드 진행 — 외부 데미지 처리 / 게이지 UI 바인딩용
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	int32 NormalNodesVisited = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	int32 TotalNormalNodes = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	float NormalProgressRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	float DoorGaugeRatio = 0.f;

	// 퍼즐 진행 중 게이지 채움 비율 = 현재 노드 기준 예상 지속시간 / 상한.
	// 노드 개수 비율(NormalProgressRatio) 대신 지속시간 가중이라 완료 후 카운트다운과 연속적으로 이어짐.
	float DurationFillRatio = 0.f;
	
	// 적에게 입힐 예상 데미지 (퍼즐 컴포넌트의 GetProjectedDamage를 미러링)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	float ProjectedDamage = 0.f;

	// 예상 데미지 정수 반올림 FText (TextBlock.Text 직접 바인딩용)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	FText ProjectedDamageText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	float HackTimeRemainingRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyHackVM", meta = (AllowPrivateAccess = "true"))
	float HackTimeRemainingSeconds = 0.f;

	// 셀 데이터 캐시 (플랫 배열, FieldNotify 미사용 — bGridRefreshTrigger로 갱신 신호)
	TArray<EHackNodeType> CachedNodeTypes;
	TArray<EHackCellState> CachedCellStates;

	TWeakObjectPtr<UEnemyHackingPuzzleComponent> BoundPuzzleComp;
};
