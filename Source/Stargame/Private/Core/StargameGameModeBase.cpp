#include "Core/StargameGameModeBase.h"

#include "Core/StargamePlayerController.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "UI/PrototypeFlightHud.h"
#include "Engine/GameInstance.h"

AStargameGameModeBase::AStargameGameModeBase()
{
	PlayerControllerClass = AStargamePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = APrototypeFlightHud::StaticClass();
}

void AStargameGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UStargameSessionSubsystem* Session = GameInstance->GetSubsystem<UStargameSessionSubsystem>())
		{
			Session->StartNewSession();
		}
	}
}
