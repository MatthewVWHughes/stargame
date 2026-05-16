#include "Space/StarSystemSubsystem.h"

#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Space/M0SystemMarkerActor.h"

void UStarSystemSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GameTimeSeconds = 0.0;
}

void UStarSystemSubsystem::Deinitialize()
{
	TearDownActiveSystem();
	Super::Deinitialize();
}

void UStarSystemSubsystem::AdvanceGameTime(double DeltaSeconds)
{
	GameTimeSeconds += FMath::Max(0.0, DeltaSeconds);
}

bool UStarSystemSubsystem::BuildSystem(const FStarSystemDefinition& SystemDefinition)
{
	TearDownActiveSystem();
	++ActiveBuildGeneration;
	LastBuildError.Reset();

	if (!ValidateSystemForBuild(SystemDefinition, LastBuildError))
	{
		return false;
	}

	ActiveSystemDefinition = SystemDefinition;
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		if (!RegisterEntity(Body.BodyId, TEXT("body"), Body.Transform, Body.VisualRadiusCm) ||
			!RegisterNavigationTarget(Body.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		if (!RegisterEntity(Station.StationId, TEXT("station"), Station.Transform, Station.VisualRadiusCm) ||
			!RegisterNavigationTarget(Station.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		if (!RegisterEntity(Gate.GateId, TEXT("gate"), Gate.Transform, Gate.VisualRadiusCm) ||
			!RegisterNavigationTarget(Gate.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	bBuildComplete = true;
	OnSystemBuildComplete.Broadcast();
	return true;
}

void UStarSystemSubsystem::TearDownActiveSystem()
{
	for (TObjectPtr<AActor> Actor : SpawnedSystemActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}

	SpawnedSystemActors.Reset();
	RegisteredEntities.Reset();
	NavigationTargetsById.Reset();
	ActiveSystemDefinition = FStarSystemDefinition();
	bBuildComplete = false;
}

bool UStarSystemSubsystem::SpawnPlayerAtSpawnZone(FName SpawnZoneId, APlayerController* PlayerController)
{
	FSpawnZoneDefinition SpawnZone;
	if (!FindSpawnZone(SpawnZoneId, SpawnZone))
	{
		LastBuildError = FString::Printf(TEXT("Missing spawn zone '%s'."), *SpawnZoneId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot spawn player without a world.");
		return false;
	}

	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = World->SpawnActor<ASpaceFlightPawn>(ASpaceFlightPawn::StaticClass(), SpawnZone.Transform, SpawnParameters);
		if (PlayerController && Pawn)
		{
			PlayerController->Possess(Pawn);
		}
	}
	else
	{
		Pawn->SetActorTransform(SpawnZone.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	return Pawn != nullptr;
}

void UStarSystemSubsystem::GetNavigationTargets(TArray<FNavigationTargetDefinition>& OutTargets) const
{
	NavigationTargetsById.GenerateValueArray(OutTargets);
	OutTargets.Sort([](const FNavigationTargetDefinition& Left, const FNavigationTargetDefinition& Right)
	{
		return Left.TargetId.LexicalLess(Right.TargetId);
	});
}

void UStarSystemSubsystem::GetRegisteredEntityIds(TArray<FName>& OutEntityIds) const
{
	RegisteredEntities.GenerateKeyArray(OutEntityIds);
	OutEntityIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UStarSystemSubsystem::GetRegisteredEntities(TArray<FActiveSystemEntityEntry>& OutEntities) const
{
	RegisteredEntities.GenerateValueArray(OutEntities);
	OutEntities.Sort([](const FActiveSystemEntityEntry& Left, const FActiveSystemEntityEntry& Right)
	{
		return Left.EntityId.LexicalLess(Right.EntityId);
	});
}

bool UStarSystemSubsystem::FindSpawnZone(FName SpawnZoneId, FSpawnZoneDefinition& OutSpawnZone) const
{
	for (const FSpawnZoneDefinition& SpawnZone : ActiveSystemDefinition.SpawnZones)
	{
		if (SpawnZone.SpawnZoneId == SpawnZoneId)
		{
			OutSpawnZone = SpawnZone;
			return true;
		}
	}
	return false;
}

FString UStarSystemSubsystem::GetM0DebugSummary() const
{
	TArray<FActiveSystemEntityEntry> Entities;
	GetRegisteredEntities(Entities);

	TArray<FNavigationTargetDefinition> Targets;
	GetNavigationTargets(Targets);

	TArray<FString> EntityLines;
	for (const FActiveSystemEntityEntry& Entity : Entities)
	{
		EntityLines.Add(FString::Printf(TEXT("%s:%s"), *Entity.EntityId.ToString(), *Entity.EntityType.ToString()));
	}

	TArray<FString> TargetLines;
	for (const FNavigationTargetDefinition& Target : Targets)
	{
		if (Target.bCanTarget && Target.bShowInHud)
		{
			TargetLines.Add(FString::Printf(TEXT("%s:%s"), *Target.TargetId.ToString(), *Target.DisplayName.ToString()));
		}
	}

	return FString::Printf(
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nLastBuildError=%s\nEntities=%s\nTargets=%s"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*LastBuildError,
		*FString::Join(EntityLines, TEXT(",")),
		*FString::Join(TargetLines, TEXT(",")));
}

bool UStarSystemSubsystem::ValidateSystemForBuild(const FStarSystemDefinition& SystemDefinition, FString& OutError) const
{
	if (SystemDefinition.SystemId.IsNone())
	{
		OutError = TEXT("System definition has an empty system ID.");
		return false;
	}

	TSet<FName> EntityIds;
	auto AddEntityId = [&EntityIds, &OutError](FName EntityId, const TCHAR* EntityType)
	{
		if (EntityId.IsNone())
		{
			OutError = FString::Printf(TEXT("%s contains an empty ID."), EntityType);
			return false;
		}

		if (EntityIds.Contains(EntityId))
		{
			OutError = FString::Printf(TEXT("Duplicate active system entity ID '%s'."), *EntityId.ToString());
			return false;
		}

		EntityIds.Add(EntityId);
		return true;
	};

	TSet<FName> TargetIds;
	auto AddTargetId = [&TargetIds, &OutError](const FNavigationTargetDefinition& Target)
	{
		if (!Target.bCanTarget)
		{
			return true;
		}

		if (Target.TargetId.IsNone())
		{
			OutError = TEXT("Navigation target contains an empty ID.");
			return false;
		}

		if (TargetIds.Contains(Target.TargetId))
		{
			OutError = FString::Printf(TEXT("Duplicate navigation target ID '%s'."), *Target.TargetId.ToString());
			return false;
		}

		TargetIds.Add(Target.TargetId);
		return true;
	};

	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		if (!AddEntityId(Body.BodyId, TEXT("Body")) || !AddTargetId(Body.NavigationTarget))
		{
			return false;
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		if (!AddEntityId(Station.StationId, TEXT("Station")) || !AddTargetId(Station.NavigationTarget))
		{
			return false;
		}
	}

	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		if (!AddEntityId(Gate.GateId, TEXT("Gate")) || !AddTargetId(Gate.NavigationTarget))
		{
			return false;
		}
	}

	TSet<FName> SpawnZoneIds;
	for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
	{
		if (SpawnZone.SpawnZoneId.IsNone())
		{
			OutError = TEXT("Spawn zone contains an empty ID.");
			return false;
		}

		if (SpawnZoneIds.Contains(SpawnZone.SpawnZoneId))
		{
			OutError = FString::Printf(TEXT("Duplicate spawn zone ID '%s'."), *SpawnZone.SpawnZoneId.ToString());
			return false;
		}

		SpawnZoneIds.Add(SpawnZone.SpawnZoneId);
	}

	return true;
}

bool UStarSystemSubsystem::RegisterEntity(FName EntityId, FName EntityType, const FTransform& Transform, double VisualRadiusCm)
{
	if (EntityId.IsNone())
	{
		LastBuildError = FString::Printf(TEXT("Cannot register %s with an empty ID."), *EntityType.ToString());
		return false;
	}

	if (RegisteredEntities.Contains(EntityId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate active system entity ID '%s'."), *EntityId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot register active system entity without a world.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AM0SystemMarkerActor* Marker = World->SpawnActor<AM0SystemMarkerActor>(AM0SystemMarkerActor::StaticClass(), Transform, SpawnParameters);
	if (!Marker)
	{
		LastBuildError = FString::Printf(TEXT("Failed to spawn marker for '%s'."), *EntityId.ToString());
		return false;
	}

	Marker->InitializeMarker(EntityId, EntityType, VisualRadiusCm);
	SpawnedSystemActors.Add(Marker);

	FActiveSystemEntityEntry Entry;
	Entry.EntityId = EntityId;
	Entry.EntityType = EntityType;
	Entry.BuildGeneration = ActiveBuildGeneration;
	Entry.Actor = Marker;
	RegisteredEntities.Add(EntityId, Entry);
	return true;
}

bool UStarSystemSubsystem::RegisterNavigationTarget(const FNavigationTargetDefinition& Target)
{
	if (!Target.bCanTarget)
	{
		return true;
	}

	if (Target.TargetId.IsNone())
	{
		LastBuildError = TEXT("Cannot register navigation target with an empty ID.");
		return false;
	}

	if (NavigationTargetsById.Contains(Target.TargetId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate navigation target ID '%s'."), *Target.TargetId.ToString());
		return false;
	}

	NavigationTargetsById.Add(Target.TargetId, Target);
	return true;
}
