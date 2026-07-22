# 적 해킹 퍼즐 — 다이애나 연결 가이드

다이애나 플레이어가 적을 조준하고 키를 눌러 해킹 퍼즐을 시작/진행/취소하고, 완료 시 적에게 데미지를 입히는 흐름을 연결하기 위한 문서입니다.

> **전제**: 적 해킹 조작은 **다이애나 플레이어 전용**. 단 **퍼즐 화면(그리드)은 휴/다이애나 양쪽 HUD에 동일하게 표시**됩니다(서버 권위 복제).
> **분담**: 퍼즐 로직(Model, 서버 권위 네트워크 포함) + UI(ViewModel/위젯)는 완료된 상태. 다이애나의 **조준 트레이스 · 입력 Server RPC · 바인딩 트리거 · 서버 데미지 적용**은 C++ 통합 경로가 연결되어 있고, HUD 내부 ViewModel 연결은 BP에서 완료해야 합니다.

---

## 0. 구현 상태 (먼저 읽을 것)

> ⚠️ 문서의 **`Server_StartHack` / `Server_ProcessInput` / `Server_CancelHack` / `HandleHackCompleted`** 이름은 흐름 설명용 예시입니다. 현재 코드는 `UDianaHackComponent::Server_RequestHack` / `Server_ProcessEnemyHackDirection` / `Server_CancelEnemyHack` / `HandleEnemyHackCompleted` 경로로 구현합니다.

| 구분 | 상태 | 위치 |
|------|------|------|
| 퍼즐 컴포넌트 서버 권위 시뮬 + 상태 복제 | ✅ **완료** | `UEnemyHackingPuzzleComponent` |
| `StartHack`/`ProcessDirectionInput`/`CancelHack`/`SetBaseHackDamage` (서버 권위) | ✅ **완료** | 〃 |
| ViewModel 표시/애니/`SetHackTarget`/`SyncFromComponent` | ✅ **완료** | `UEnemyHackingPuzzleViewModel` |
| 적 해킹 위젯 asset | ✅ **존재** | `/Game/UI/Widget/EnemyHacking/WBP_EnemyHackingWidget` |
| 역할별 HUD asset | ✅ **존재** | `/Game/UI/Widget/WBP_HughHUD`, `/Game/UI/Widget/WBP_DianaHUD` |
| 컴포넌트를 MonsterBase에 부착 | ✅ **완료** | `AMonsterBase` |
| 조준 트레이스(카메라 중앙 → 적) | ✅ **완료** | `ADianaOperatorPawn::RequestHackAtReticle` |
| 입력 Server RPC | ✅ **완료** | `UDianaHackComponent` |
| 데미지 핸들러(`OnHackCompleted` → `ApplySuccessfulHack`+오버드라이브) | ✅ **완료** | `UDianaHackComponent::HandleEnemyHackCompleted` |
| 복제된 "현재 해킹 대상" 신호(양쪽 HUD 바인딩 트리거) | ✅ **완료(C++ 신호)** | `APragmataGameState::ActiveEnemyHackTarget` |
| 역할별 HUD 생성 경로 | ✅ **완료** | `APragmataPlayerController::EnsureGameplayHUD` → possessed pawn `GameplayHUDClass` |

→ 즉 **컴포넌트는 "서버에서 불리면 동작"하게 완성**돼 있고, C++ 호출 경로도 연결되어 있습니다. 아래 예시 코드의 `Server_*`는 현재 코드의 실제 함수명과 다를 수 있는 흐름 템플릿입니다.

> HUD root 생성은 C++ 경로로 일원화. `APragmataPlayerController::EnsureGameplayHUD()`가 local controller의 possessed pawn을 확인하고, `BP_HughCharacter`/`BP_DianaOperatorPawn` defaults의 `GameplayHUDClass`에서 `WBP_HughHUD`/`WBP_DianaHUD`를 읽어 생성.

---

## 1. 구조 개요

```
View       = WBP_EnemyHackingWidget 등          (UI, 양쪽 HUD)
ViewModel  = UEnemyHackingPuzzleViewModel        (UI 어댑터, 클라마다 1개)
Model      = UEnemyHackingPuzzleComponent        (서버 권위 시뮬 + 복제) ← 적(MonsterBase)에 부착
```

- 퍼즐 컴포넌트(Model)는 **적 액터에 부착**. **해킹 대상 = 컴포넌트의 `GetOwner()`**.
- **서버 권위**: 그리드 생성·커서 이동·완료 판정은 **서버에서만** 실행. 상태는 복제되어 **휴/다이애나 양쪽 클라가 동일하게 표시**.
- **조작은 다이애나만**: 입력은 다이애나가 보내지만, 적은 서버 소유라 **다이애나 폰의 Server RPC를 거쳐 서버에서** 컴포넌트를 조작.
- **데미지 적용은 다이애나 쪽**(서버)에서 처리. 컴포넌트는 데미지를 직접 적용하지 않음.

---

## 2. 선행 세팅 (필수)

- `UEnemyHackingPuzzleComponent`는 **`AMonsterBase`에 기본 컴포넌트로 추가**되어 있습니다. 특수 적은 C++/BP defaults에서 `EnemyType` 등을 덮어씁니다.
- **소유 액터가 복제돼야 함**(`bReplicates=true`). 컴포넌트는 `SetIsReplicatedByDefault(true)`라, 복제 안 되는 액터에 붙으면 양쪽 표시가 안 됨.
- 컴포넌트 EditAnywhere 설정값(런타임 불변, 복제 안 함 — 에디터/맵 값이 양쪽 동일):
  | 프로퍼티 | 의미 | 기본 |
  |----------|------|------|
  | `EnemyType` | 그리드 크기 (Watcher 3×3 / Walker 4×4 / Executor 6×6 / Sentinel 8×8) | Walker |
  | `AMonsterBase::bIsTainted` | 보라(Corrupted) 노드 생성 여부. 정화 전 true, 정화 후 false | true |
  | `FirstNodeDamageMultiplier` | 첫 파란 노드 통과 시 데미지 배수 (2 = 2배), 이후 가산분 절반씩 감소 | 2.0 |

---

## 3. 네트워크 모델 (핵심)

| 역할 | 휴 (리슨 서버 호스트 = 권위) | 다이애나 (클라이언트) |
|------|------|------|
| 시뮬(시작/이동/완료) | 서버에서 실행 | ❌ 직접 못 함 → **Server RPC로 요청** |
| 표시 | 서버 변경 즉시 델리게이트 발동 | 복제 수신 → `OnRep`이 델리게이트 발동 |
| 조작 입력 | (호스트지만 조작자는 다이애나) | 다이애나 폰 **Server RPC** |

**반드시 알아야 할 2가지:**

1. **조작 함수는 서버 전용**: `StartHack` / `ProcessDirectionInput` / `CancelHack` / `SetBaseHackDamage`는 `HasAuthority` 가드라 **클라에서 호출하면 no-op**. → 다이애나(클라)는 **자기 폰의 Server RPC**를 통해 서버에서 호출해야 함.
   - ⚠️ 그래서 `ViewModel->StartEnemyHacking()`(내부에서 StartHack 직접 호출)은 **다이애나 클라에서 시작이 안 됨**. 스탠드얼론/단독 테스트용으로만 유효.
2. **클라→서버 RPC는 그 클라가 소유한 액터에서만** 라우팅됨. 적(MonsterBase)은 서버 소유라 적 위의 `Server_` 함수는 다이애나가 못 부름 → **RPC는 `ADianaOperatorPawn`(또는 그 컴포넌트)에 둬야** 함.

---

## 4. 전체 호출 시퀀스

```
[다이애나 조준 + 키 입력] (다이애나 클라)
   └─ 카메라 정면 트레이스 → 적 찾기 (클라에서 대상 결정)
        └─ DianaPawn ▶ Server_StartHack(적)
              └─[서버] 적의 PuzzleComp 획득
                      ├ PuzzleComp.SetBaseHackDamage(다이애나 데미지)  ★ StartHack 전
                      ├ PuzzleComp.StartHack()
                      └ PuzzleComp.OnHackCompleted 에 데미지 핸들러 바인딩(서버)
        └─ (복제된 "해킹 대상" 세팅 → 양쪽 HUD가 관찰해 바인딩)

[해킹 중 방향 입력] (다이애나 클라)
   └─ DianaPawn ▶ Server_ProcessInput(적, Dir) →[서버] PuzzleComp.ProcessDirectionInput(Dir)

[취소: 키 떼기/재입력]
   └─ DianaPawn ▶ Server_CancelHack(적) →[서버] PuzzleComp.CancelHack()

[완료: 커서가 Target 도달 → 서버가 감지]
   └─[서버] PuzzleComp.OnHackCompleted 발동
           └ 데미지 핸들러: ApplySuccessfulHack(적, 다이애나, GetProjectedDamage(), GetResolvedHackDurationSeconds()) + 오버드라이브
```

> 위 `Server_StartHack`/`Server_ProcessInput`/`Server_CancelHack`는 흐름 설명용 이름입니다(§0). 실제 구현은 `UDianaHackComponent`의 Server RPC 이름을 사용합니다.

---

## 5. 상황별 "무엇을 호출하나" (C++ / BP / 권한)

| 상황 | 호출 대상 | 함수 | 실행 위치 | C++ | BP |
|------|-----------|------|-----------|:---:|:--:|
| 해킹 시작 | PuzzleComponent | `StartHack()` | **서버** (RPC 경유) | ✅ | ✅ |
| 시작 전 데미지 주입 | PuzzleComponent | `SetBaseHackDamage(float)` | **서버** | ✅ | ✅ |
| 커서 이동 | PuzzleComponent | `ProcessDirectionInput(EHackDirection)` | **서버** (RPC 경유) | ✅ | ✅ |
| 취소(연출 O) | PuzzleComponent | `CancelHack()` | **서버** (RPC 경유) | ✅ | ✅ |
| UI 바인딩(표시) | ViewModel | `SetHackTarget(AActor*)` | **각 클라 로컬** | ✅ | ✅ |
| 완료 → 데미지 | PuzzleComponent | `OnHackCompleted` (C++ 델리게이트) | **서버** | ✅ | ❌ |
| 다이애나 데미지 조회 | DianaHackComponent | `GetHackDamage()` | 서버 | ✅ | ✅ |
| 적에 결과 적용 | 적(IPragmataHackable) | `ApplySuccessfulHack(Hacker, Damage, Duration)` | **서버** | ✅ | ✅(인터페이스) |

> **델리게이트 주의**: 컴포넌트 C++ 멀티캐스트(`OnHackStarted`/`OnGridReady`/`OnCursorMoved`/`OnHackCompleted`/`OnHackCancelled`)는 **C++ 전용**(BP 바인딩 불가). 데미지 적용은 **서버에서 `OnHackCompleted`(C++)** 로 받으세요. ViewModel의 `OnHackCompletedWithDamage`(BP)는 **양쪽 클라에서 발동되는 UI용**이라 데미지엔 쓰지 말 것.
> **접근 주의**: 현재 `ADianaOperatorPawn` 내부의 `HackComponent`는 private입니다. RPC/완료 처리를 `ADianaOperatorPawn` 내부에 만들면 직접 접근 가능하지만, 별도 컴포넌트나 컨트롤러에서 구현한다면 `GetHackComponent()` 같은 접근 경로를 먼저 추가해야 합니다.

---

## 6. 진입점 상세

### 6-1. 해킹 시작 (조준은 클라, 시작은 서버)

조준 트레이스는 **다이애나 클라에서** 대상을 고르고, 실제 시작은 **Server RPC**로:

> 아래 `Server_StartHack`은 흐름 설명용 이름입니다(§0). 현재 구현은 `UDianaHackComponent::Server_RequestHack`에서 같은 순서로 처리합니다.

```cpp
// (다이애나 클라) 키 입력 시 — 대상만 결정
FHitResult Hit;
const FVector Origin = GetCameraOrigin();
const FVector End    = Origin + GetCameraDirection() * TraceDistance;
World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params /*self 무시*/);
if (AActor* Target = Hit.GetActor())
{
    Server_StartHack(Target);   // 서버로 요청 (소유 폰의 Server RPC)
}

// (서버) Server_StartHack_Implementation
if (!Target || !Target->GetClass()->ImplementsInterface(UPragmataHackable::StaticClass())) return;
if (!IPragmataHackable::Execute_CanBeHacked(Target, this)) return;
if (UEnemyHackingPuzzleComponent* Puzzle = Target->FindComponentByClass<UEnemyHackingPuzzleComponent>())
{
    Puzzle->SetBaseHackDamage(DianaHackComponent->GetHackDamage()); // ★ StartHack 전
    Puzzle->StartHack();
    Puzzle->OnHackCompleted.RemoveAll(this); // 재시작/타겟 변경 시 중복 바인딩 방지
    Puzzle->OnHackCompleted.AddUObject(this, &ADianaXxx::HandleHackCompleted); // 완료 시 데미지
    // 복제된 "현재 해킹 대상"에 Target 세팅 → 양쪽 HUD가 바인딩 (8장)
}
```

- **`SetBaseHackDamage`는 `StartHack` 전에** (그리드 생성 시 데미지 표시 초기값을 읽음).
- 트레이스는 클라에서 대상만 고르고, **검증·시작은 서버**에서.
- 완료 델리게이트는 시작마다 누적되지 않게 `RemoveAll(this)` 후 바인딩하거나, 완료/취소/타겟 변경 시 명시적으로 해제.

### 6-2. 노드 이동

```cpp
// (다이애나 클라) 방향 입력 → Server RPC
Server_ProcessInput(CurrentTarget, EHackDirection::Right);
// (서버) → Puzzle->ProcessDirectionInput(Dir);
```

- 입력 변환(키/패드/마우스 → `EHackDirection`)은 클라 자유. 종착점이 `ProcessDirectionInput` 하나.
- 서버에서 `PuzzleState != Active`면 무시되므로 안전.
- **백트래킹**: 바로 직전 칸으로의 한 칸 후퇴만 허용(중간 컷 금지).
- **보라(Corrupted) 노드**: 진입하려면 같은 방향 **2회 연속 입력**(진입 게이트). 진입 후 탈출은 일반 이동.

### 6-3. 취소

```cpp
// (다이애나 클라) → Server RPC → (서버) Puzzle->CancelHack();
```

- 취소는 UI에서 취소 애니 재생 후 패널을 닫음(`OnHackCancelled` → ViewModel).
- ViewModel `StopEnemyHacking()`은 **UI 언바인딩 + 권위가 있으면 `CancelHack()` 호출**까지 수행하므로, 일반 입력 취소보다는 테스트/적 사망/구역 이탈 같은 하드 정리용으로만 쓸 것.

---

## 7. 완료 → 데미지 적용 (서버에서)

서버가 시뮬을 돌리므로 **완료도 서버에서 감지**됩니다. 클라 왕복 불필요.

### 데미지 공식 (Model이 단일 소스)

```
GetProjectedDamage() = Base × TotalMultiplier
   TotalMultiplier: 0노드=1, 1노드=2, 2노드=2.5, 3노드=2.75 ... (3배 수렴)
   가산분이 매 파란 노드마다 절반씩 감소 (FirstNodeDamageMultiplier로 첫 배수 조절)
```

UI 표시값과 실제 데미지가 **같은 `GetProjectedDamage()`** → 어긋나지 않음.

### 적용 (서버, 다이애나 쪽)

```cpp
// 서버에서 OnHackCompleted 수신 (6-1에서 바인딩)
void ADianaXxx::HandleHackCompleted()
{
    if (!CurrentTarget || !CurrentPuzzle) return;
    if (CurrentPuzzle->GetOwner() != CurrentTarget) return; // 이전 타겟/퍼즐 콜백 방지

    const float FinalDamage = CurrentPuzzle->GetProjectedDamage();
    const float HackDurationSeconds = CurrentPuzzle->GetResolvedHackDurationSeconds();
    IPragmataHackable::Execute_ApplySuccessfulHack(CurrentTarget, this, FinalDamage, HackDurationSeconds);
    // OverDriveComponent->AddOverDrive(...) 등 다이애나 보상 처리

    CurrentPuzzle->OnHackCompleted.RemoveAll(this);
    // 복제된 "현재 해킹 대상"도 null/종료 상태로 갱신
}
```

- `ApplySuccessfulHack`은 적 내부에서 `FPragmataDamageSpec`(Kind=Hack) 생성 후 `ReceiveDamage` 경로로 처리하고, 최초 hacked 진입에만 puzzle이 계산한 유지 시간을 timer에 적용.
- **데미지는 컴포넌트가 아니라 다이애나 쪽이 적용** — 오버드라이브 적립 등 다이애나 상태와 묶이기 때문(분담 유지).
- 서버 권위라 노드 수는 서버가 직접 시뮬한 값(신뢰 가능).
- 완료 콜백은 서버의 현재 타겟/퍼즐과 일치하는지 검증. 이전 해킹의 콜백이 늦게 들어와 다른 적에 데미지를 주는 상황을 막기 위함.

### 7-1. Already-Hacked Enemy Repeat Reward Policy

이미 hacked 상태인 enemy도 Diana enemy hack puzzle 시작 대상이다. `CanBeHacked()`는 puzzle 시작 가능 여부를 판단하며, `bHacked`는 더 이상 시작 차단 조건이 아니다.

Repeat completion 규칙:

- enemy에는 `ApplySuccessfulHack()`을 통해 hack damage 적용
- Diana에는 기존 completion과 동일하게 OverDrive와 success signal 지급
- `bHacked` duration timer는 reset/extend 없음
- `OnHackApplied()` 같은 hacked-state 진입 효과는 최초 `false -> true` 전환 때만 실행
- 기존 timer가 만료돼 `bHacked == false`가 된 뒤 다시 완료하면 새 duration 시작

### 7-2. 파란 노드별 hacked 유지 시간

`UEnemyHackingPuzzleComponent`가 완료 순간의 `EnemyType`과 `NormalNodesVisited`로 duration을 계산한다. 기준보다 많은 파란 노드는 마지막 tier로 clamp한다.

| 적 종류 | 0개 | 1개 | 2개 | 3개 이상 |
| --- | ---: | ---: | ---: | ---: |
| Watcher | 6.00초 | 15.00초 | 15.00초 | 15.00초 |
| Walker | 2.06초 | 6.71초 | 15.00초 | 15.00초 |
| Executor | 1.10초 | 4.55초 | 9.10초 | 15.00초 |
| Sentinel boss | 1.00초 | 2.50초 | 5.50초 | 3개 10.00초, 4개 이상 15.00초 |

- Watcher, Walker, Executor, Sentinel profile은 puzzle component class default에서 조정.
- Sentinel은 boss 난이도를 위해 4개 이상 파란 노드에서만 15초에 도달.
- 이미 hacked 상태에서 다시 완료하면 새 duration을 계산해도 기존 timer는 reset/extend하지 않음.

---

## 8. UI 바인딩 — 양쪽 HUD (표시 레이어)

각 클라(휴·다이애나)의 ViewModel이 **자기 로컬 컴포넌트 인스턴스에 바인딩**하면, 복제 상태를 동일하게 표시합니다. 진입점은 **`SetHackTarget(AActor*)`** 하나:

```cpp
ViewModel->SetHackTarget(TargetEnemy);  // 적 → 내부에서 컴포넌트 찾아 BindToComponent (없으면 Unbind)
ViewModel->SetHackTarget(nullptr);      // 해제
```

- `SetHackTarget`은 `BlueprintCallable` → BP/C++ 양쪽 호출 가능. **바인딩은 read-only 표시라 휴/다이애나 동일 코드**.
- 바인딩 시점에 컴포넌트가 이미 Active면 `SyncFromComponent`가 **현재 상태를 즉시 채움**(늦은 바인딩/중간 접속 대응).

### HUD host 연결

권장 배치:

- `WBP_HughHUD`: `WBP_EnemyHackingWidget` 포함, 표시 전용 또는 compact 표시
- `WBP_DianaHUD`: `WBP_EnemyHackingWidget` 포함, 조작 중 전체 퍼즐 표시
- 각 HUD 내부에 `UEnemyHackingPuzzleViewModel` 인스턴스 또는 MVVM ViewModel 지정
- 복제된 current enemy target 변경 시 두 HUD 모두 `SetHackTarget(TargetEnemy)` 호출
- 종료/취소/적 사망 시 두 HUD 모두 `SetHackTarget(nullptr)` 호출

역할별 HUD 생성 확인:

- `APragmataPlayerController::EnsureGameplayHUD()`가 root HUD를 1회 생성
- Hugh possess 시 `AHughCharacter::GetGameplayHUDClass()` → `WBP_HughHUD`
- Diana possess 시 `ADianaOperatorPawn::GetGameplayHUDClass()` → `WBP_DianaHUD`
- WBP 교체는 `BP_HughCharacter`/`BP_DianaOperatorPawn` class defaults의 `GameplayHUDClass`에서 처리
- pawn Blueprint EventGraph에서 HUD를 별도로 `Create Widget`하면 같은 플레이어에 HUD가 2개 생길 수 있음

### 바인딩 트리거

각 클라가 "지금 어떤 적이 해킹 중인지" 알아야 `SetHackTarget`을 부를 수 있음:

- **복제된 "현재 해킹 대상"**은 `APragmataGameState::ActiveEnemyHackTarget`에 두고, 서버가 시작/종료 시 세팅.
- `TWeakObjectPtr`는 로컬 약참조 용도라 "복제되는 현재 대상" 필드 예시로 쓰지 말 것.
- `APragmataPlayerController`가 GameState 변경을 root HUD의 `OnActiveEnemyHackTargetChanged(TargetActor)` BP 이벤트로 전달.
- **양쪽 HUD가 그 값을 관찰** → 변경 시 `SetHackTarget(대상)` / 종료 시 `SetHackTarget(null)`.

---

## 9. 단독 테스트 (네트워크 전, 1인 PIE)

서버 권위 가드는 **Standalone(1인 PIE)에선 로컬이 곧 서버**라 통과됩니다. 트레이스/복제 신호 없이 UI 전체를 검증 가능:

```
[테스트 키] → GetAllActorsOfClass(BP_WalkerMonster)[0]
   → Get Component by Class(EnemyHackingPuzzleComponent)  ▶ comp
   → comp ▶ Set Base Hack Damage(50)
   → comp ▶ Start Hack
   → ViewModel ▶ Set Hack Target(그 적)        // 표시 바인딩
[방향 키] → comp ▶ Process Direction Input(Dir)
[취소 키] → comp ▶ Cancel Hack
```

> 2인 PIE(Listen Server)부터 휴 복제 표시 / 다이애나 입력 RPC가 필요. 단독 PIE는 표시·동작 검증용.

---

## 10. API 레퍼런스

### UEnemyHackingPuzzleComponent (적에 부착, Model — 서버 권위 + 복제)

| 멤버 | 종류 | 실행/권한 | C++/BP | 비고 |
|------|------|-----------|--------|------|
| `StartHack()` | 함수 | 서버 전용 | C++/BP | 그리드 생성+시작 |
| `ProcessDirectionInput(EHackDirection)` | 함수 | 서버 전용 | C++/BP | 유일한 이동 진입점 |
| `CancelHack()` | 함수 | 서버 전용 | C++/BP | 연출 있는 취소 |
| `SetBaseHackDamage(float)` | 함수 | 서버 전용 | C++/BP | 시작 전 데미지 주입 |
| `GetProjectedDamage()` | 함수 | 양쪽(읽기) | C++/BP | Base×배수, 실제/표시 공통 |
| `GetBaseHackDamage()` | 함수 | 양쪽 | C++/BP | |
| `GetPuzzleState()` | 함수 | 양쪽 | C++/BP | Idle/Active/Completed |
| `GetGridSize()` / `GetCurrentGridSize()` | 함수 | 양쪽 | C++/BP | 후자는 복제된 실제 크기 |
| `GetNodeType` / `GetCellState` | 함수 | 양쪽 | C++/BP | |
| `GetCursorPos` / `GetTargetPos` | 함수 | 양쪽 | C++/BP | |
| `IsCorruptedPending()` | 함수 | 양쪽 | C++/BP | 진입 대기 인디케이터 |
| `GetNormalNodesVisited` / `GetTotalNormalNodes` | 함수 | 양쪽 | C++/BP | |
| `OnHackStarted`/`OnGridReady`/`OnCursorMoved`/`OnHackCompleted`/`OnHackCancelled` | 델리게이트 | 서버 직접+클라 OnRep | **C++ 전용** | 데미지는 서버 `OnHackCompleted`로 |

### UEnemyHackingPuzzleViewModel (UI, 클라마다)

| 멤버 | 종류 | C++/BP | 비고 |
|------|------|--------|------|
| `SetHackTarget(AActor*)` | 함수 | C++/BP | **바인딩 통합 진입점** (적→컴포넌트→bind, null→unbind) |
| `BindToComponent` / `UnbindFromComponent` | 함수 | C++/BP | 보통 SetHackTarget 경유 |
| `StartEnemyHacking(PuzzleComp)` | 함수 | C++/BP | bind+StartHack — **클라 no-op(테스트용)** |
| `StopEnemyHacking()` | 함수 | C++/BP | UI 언바인딩 + 권위가 있으면 CancelHack 호출. 테스트/하드 정리용 |
| `OnCompletedAnimationFinished()` / `OnCancelledAnimationFinished()` | 함수 | C++/BP | **UI 위젯이 호출** |
| `OnHackCompletedWithDamage(float)` | 델리게이트 | C++/BP | **UI용**(양쪽 발동) — 데미지 적용엔 쓰지 말 것 |
| `GetProjectedDamage()` / `GetProjectedDamageText()` | 함수 | C++/BP | 표시용 |
| `GetIsActive`/`GetStartedTrigger`/`GetCancelledTrigger`/`GetCompletedTrigger`/`GetCorruptedPending`/`GetNormalProgressRatio` 등 | 함수 | C++/BP | UI 바인딩용 |

---

## 11. 열거형 참조

```
EHackDirection : Up, Down, Left, Right, None
EEnemyHackType : Watcher(3×3), Walker(4×4), Executor(6×6), Sentinel(8×8)
EHackNodeType  : Empty, Normal(파랑), Corrupted(보라), Target(목표)
EHackCellState : Default, Visited(회색 경로), Cursor
EEnemyHackState: Idle, Active, Completed
```

---

## 12. 체크리스트 (다이애나 연결 담당)

- [x] `EnemyHackingPuzzleComponent`를 **복제되는 MonsterBase/몬스터 BP에 부착**
- [x] 조준 키 입력 → 카메라 정면 트레이스로 대상 결정 (다이애나 클라)
- [x] **다이애나 폰/컴포넌트에 Server RPC** 추가: 시작 / 방향 / 취소
- [x] 시작 RPC(서버): `CanBeHacked` 검증(이미 hacked여도 허용) → `SetBaseHackDamage(GetHackDamage())` → `StartHack()` → 기존 `OnHackCompleted` 바인딩 해제 후 재바인딩
- [x] 방향/취소 입력 → 각 Server RPC로 라우팅
- [x] **복제된 "현재 해킹 대상"** 추가(GameState, `TObjectPtr<AActor>` 복제) → 양쪽 HUD가 관찰해 `SetHackTarget` 호출
- [ ] `BP_HughCharacter`/`BP_DianaOperatorPawn` defaults의 `GameplayHUDClass`가 각각 `WBP_HughHUD`/`WBP_DianaHUD`인지 확인
- [ ] 두 HUD 내부 `WBP_EnemyHackingWidget`이 root HUD 이벤트에서 같은 current target으로 `SetHackTarget` 호출
- [x] 종료/취소 시 `SetHackTarget(nullptr)`로 양쪽 HUD 해제 신호 전파
- [x] 서버 `OnHackCompleted` 핸들러 → 현재 타겟/퍼즐 일치 검증 → `ApplySuccessfulHack(적, 다이애나, GetProjectedDamage(), GetResolvedHackDurationSeconds())` + 오버드라이브 → 델리게이트 해제/현재 대상 정리
- [x] 이미 hacked 상태에서 완료한 repeat hack은 damage/보상만 적용하고 기존 `bHacked` timer는 reset하지 않음
