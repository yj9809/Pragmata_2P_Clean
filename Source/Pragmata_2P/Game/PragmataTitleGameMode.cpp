#include "Game/PragmataTitleGameMode.h"

#include "Game/PragmataTitlePlayerController.h"

// title map은 pawn 없이 local UI와 camera만 사용
APragmataTitleGameMode::APragmataTitleGameMode()
{
	PlayerControllerClass = APragmataTitlePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
