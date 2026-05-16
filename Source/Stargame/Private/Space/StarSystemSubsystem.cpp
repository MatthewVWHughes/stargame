#include "Space/StarSystemSubsystem.h"

void UStarSystemSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GameTimeSeconds = 0.0;
}

void UStarSystemSubsystem::AdvanceGameTime(double DeltaSeconds)
{
	GameTimeSeconds += FMath::Max(0.0, DeltaSeconds);
}

