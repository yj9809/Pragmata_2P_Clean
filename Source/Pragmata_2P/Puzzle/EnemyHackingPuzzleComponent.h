// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "EnemyHackingPuzzleComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnEnemyHackStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyGridReady, int32 /*GridSize*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEnemyCursorMoved, FIntPoint /*NewPos*/, bool /*bCorruptedPending*/);
DECLARE_MULTICAST_DELEGATE(FOnEnemyDecodeNodeVisited);
DECLARE_MULTICAST_DELEGATE(FOnEnemyHackCompleted);
DECLARE_MULTICAST_DELEGATE(FOnEnemyHackCancelled);

// 그리드 셀 1칸 — 복제를 위해 top-level USTRUCT
USTRUCT()
struct FHackGridCell
{
	GENERATED_BODY()

	UPROPERTY()
	EHackNodeType NodeType = EHackNodeType::Empty;

	UPROPERTY()
	EHackCellState CellState = EHackCellState::Default;
};

// 적 종류별 파란 노드 통과 수에 따른 hacked 유지 시간
USTRUCT(BlueprintType)
struct FEnemyHackDurationProfile
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pragmata|Puzzle")
	EEnemyHackType EnemyType = EEnemyHackType::Walker;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pragmata|Puzzle", meta = (ClampMin = "0.0"))
	TArray<float> DurationSecondsByVisitedNodeCount;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UEnemyHackingPuzzleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyHackingPuzzleComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|Puzzle")
	EEnemyHackType EnemyType = EEnemyHackType::Walker;

	// 적 종류와 파란 노드 수로 hacked 유지 시간을 결정하는 class default profile
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pragmata|Puzzle")
	TArray<FEnemyHackDurationProfile> HackDurationProfiles;

	// 첫 파란 노드 통과 시 총 데미지 배수 (2 = 첫 노드에서 2배).
	// 가산분(계수-1)이 이후 노드마다 절반씩 감소: 총배수 1 → 2 → 2.5 → 2.75 ... (3배 수렴)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|Puzzle", meta = (ClampMin = "1"))
	float FirstNodeDamageMultiplier = 2.f;

	// 아래 조작 함수들은 서버 권위 — 서버에서만 실행됨.
	// 다이애나(클라)는 자신이 소유한 액터의 Server RPC를 통해 서버에서 호출해야 함.
	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	bool StartHack();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	void ProcessDirectionInput(EHackDirection InputDirection);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	void CancelHack();

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetGridSize() const;

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetCurrentGridSize() const { return CachedGridSize; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	EEnemyHackState GetPuzzleState() const { return PuzzleState; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	EHackNodeType GetNodeType(int32 Row, int32 Col) const;

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	EHackCellState GetCellState(int32 Row, int32 Col) const;

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	FIntPoint GetCursorPos() const { return CursorPos; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	FIntPoint GetTargetPos() const { return TargetPos; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	bool IsCorruptedPending() const { return bCorruptedPending; }

	// 전체 파란(Normal) 노드 수 (실제 배치된 개수)
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetTotalNormalNodes() const { return TotalNormalNodes; }

	// 커서가 지나간 파란 노드 수 (고유, 재방문 제외)
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetNormalNodesVisited() const { return NormalNodesVisited; }

	// 노란(Decode) 노드가 현재 커서 경로 위에 있는지 — 방어력 감소 적용 여부의 단일 소스(서버 권위·복제).
	// 그리드당 1개, live 방식(백트래킹으로 벗어나면 다시 false). 완료 시점 값 = 이번 해킹의 디코드 성공 여부.
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	bool IsDecodeNodeVisited() const { return bDecodeNodeVisited; }

	// 완료 시점의 파란 노드 수로 hacked 유지 시간을 조회하고 profile이 없으면 0을 반환
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	float GetResolvedHackDurationSeconds() const;

	// 진입점(다이애나 시작 로직)이 StartHack 호출 전에 다이애나 핵 기본 데미지를 주입 (서버 권위)
	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	void SetBaseHackDamage(float InBaseHackDamage);

	// 진입점에서 주입된 기본 데미지 (배수 미적용)
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	float GetBaseHackDamage() const { return BaseHackDamage; }

	// 현재까지 지난 파란 노드를 반영한 예상 데미지: Base * TotalMultiplier (가산분 절반씩 감소)
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	float GetProjectedDamage() const;

	// 지나간 파란 노드 수에 따른 hacked 지속시간(초) — 적 종류별 밸런스 표 룩업, 상한 15초.
	// 퍼즐 진행 중에는 현재 방문 수 기준 예상 지속시간, 완료 시점엔 최종 지속시간이 됨.
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	float GetEarnedHackDuration() const;

	// hacked 지속시간 게이지의 분모 기준(초) — 모든 적 공통 상한.
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	float GetMaxHackDurationSeconds() const { return MaxHackDurationSeconds; }

	// 소유 적이 현재 hacked 상태인지 — 재해킹 시작 시 셀 색 분기용(hacked면 노랑/아니면 흰색)
	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	bool IsOwnerHacked() const;

	FOnEnemyHackStarted OnHackStarted;
	FOnEnemyGridReady OnGridReady;
	FOnEnemyCursorMoved OnCursorMoved;
	FOnEnemyDecodeNodeVisited OnDecodeNodeVisited;
	FOnEnemyHackCompleted OnHackCompleted;
	FOnEnemyHackCancelled OnHackCancelled;

private:
	// --- 복제 상태 (서버가 시뮬, 양쪽 클라가 표시) ---

	UPROPERTY(ReplicatedUsing = OnRep_PuzzleState)
	EEnemyHackState PuzzleState = EEnemyHackState::Idle;

	// 커서/대기 상태 변경 시 서버가 증가 → 클라가 OnCursorMoved 발동 (커서 위치가 같아도 알림 보장)
	UPROPERTY(ReplicatedUsing = OnRep_CursorRep)
	uint8 CursorRepCounter = 0;

	UPROPERTY(Replicated)
	int32 CachedGridSize = 4;

	UPROPERTY(Replicated)
	TArray<FHackGridCell> Grid; // 플랫 배열 [Row * GridSize + Col]

	UPROPERTY(Replicated)
	FIntPoint CursorPos = FIntPoint(0, 0);

	UPROPERTY(Replicated)
	FIntPoint TargetPos = FIntPoint(0, 0);

	// 파란(Normal) 노드 통계 — 데미지 / 게이지 UI용
	UPROPERTY(Replicated)
	int32 TotalNormalNodes = 0;

	UPROPERTY(Replicated)
	int32 NormalNodesVisited = 0;

	// 노란(Decode) 노드가 현재 경로 위에 있는지 — 방어력 감소 신호(데미지/지속시간과 무관)
	UPROPERTY(Replicated)
	bool bDecodeNodeVisited = false;

	// 진입점에서 주입된 다이애나 핵 기본 데미지 (배수 미적용)
	UPROPERTY(Replicated)
	float BaseHackDamage = 0.f;

	// Corrupted 노드 진입 대기 표시 (양쪽 HUD 인디케이터용)
	UPROPERTY(Replicated)
	bool bCorruptedPending = false;

	// --- 서버 전용 상태 (복제 안 함) ---

	// 현재 커서 경로 (start → 커서, 단순 경로). 백트래킹 시 뒤쪽을 되감아 복구
	TArray<FIntPoint> Path;

	// Corrupted 노드 진입 게이트 — 같은 방향 연속 입력 카운트 (서버 입력 처리용)
	EHackDirection CorruptedPendingDir = EHackDirection::None;
	int32 CorruptedPendingCount = 0;

	// --- 내부 로직 ---

	UFUNCTION()
	void OnRep_PuzzleState(EEnemyHackState OldPuzzleState);

	UFUNCTION()
	void OnRep_CursorRep();

	bool HasPuzzleAuthority() const;

	void InitGrid();
	void MoveCursor(FIntPoint NewPos);
	void CompleteHack();

	// 델리게이트 발동 (서버 직접 호출 + 클라 OnRep에서 호출 공유)
	void BroadcastStarted();
	void NotifyCursorMoved(); // 서버: 카운터++ 후 OnCursorMoved 발동

	FHackGridCell& GetCell(int32 Row, int32 Col) { return Grid[Row * CachedGridSize + Col]; }
	const FHackGridCell& GetCell(int32 Row, int32 Col) const { return Grid[Row * CachedGridSize + Col]; }

	bool IsValidPos(FIntPoint Pos) const
	{
		return Pos.X >= 0 && Pos.X < CachedGridSize && Pos.Y >= 0 && Pos.Y < CachedGridSize;
	}

	static FIntPoint DirectionToOffset(EHackDirection Dir);

	// hacked 지속시간 게이지 상한(초) — 밸런스 표의 최대치와 일치. 게이지 = 지속시간 / 이 값.
	static constexpr float MaxHackDurationSeconds = 15.f;
};
