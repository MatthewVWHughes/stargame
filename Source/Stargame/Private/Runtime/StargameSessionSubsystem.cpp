#include "Runtime/StargameSessionSubsystem.h"

#include "Data/FrontierTestFixtureProvider.h"
#include "Data/StarCatalogSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/StargameM0SaveGame.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStargameStartup, Log, All);

namespace
{
	bool ValidateNativeFallbackFixture(const UStarCatalogSubsystem* Catalog, const FStarSystemDefinition& SystemDefinition, FString& OutError)
	{
		if (Catalog && Catalog->IsUsingAssetCatalog())
		{
			return true;
		}

		return FFrontierTestFixtureProvider::ValidateM0Fixture(SystemDefinition, OutError);
	}
}

EStartSessionResult UStargameSessionSubsystem::StartNewSession(FName InStartProfileId)
{
	LastSessionError.Reset();
	LastStartSessionResult = EStartSessionResult::ValidationFailed;

	const FName RequestedStartProfileId = InStartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: InStartProfileId;

	FStartProfileDefinition StartProfile;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	const bool bResolvedStartProfile = Catalog
		? Catalog->ResolveStartProfile(RequestedStartProfileId, StartProfile)
		: FFrontierTestFixtureProvider::ResolveStartProfile(RequestedStartProfileId, StartProfile);
	if (!bResolvedStartProfile)
	{
		LastStartSessionResult = EStartSessionResult::MissingStartProfile;
		ReportStartupError(FString::Printf(TEXT("Missing start profile '%s'."), *RequestedStartProfileId.ToString()));
		return LastStartSessionResult;
	}

	if (!BuildAndSpawnFromStartProfile(StartProfile))
	{
		return LastStartSessionResult;
	}

	LastStartSessionResult = EStartSessionResult::Success;
	UE_LOG(LogStargameStartup, Display, TEXT("Started session '%s' in system '%s' at spawn zone '%s'."),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString());
	return LastStartSessionResult;
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot()
{
	return SaveDevelopmentSlot(MakeCurrentM0SaveState());
}

void UStargameSessionSubsystem::AdvanceSimulationClock(double DeltaSeconds)
{
	ClockSnapshot.AuthoritativeSimulationTimeSeconds += FMath::Max(0.0, DeltaSeconds) * ClockSnapshot.TimeScale;
}

bool UStargameSessionSubsystem::SelectNavigationTargetById(FName TargetId)
{
	if (TargetId.IsNone())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	FNavigationTargetDefinition Target;
	if (!StarSystem || !StarSystem->FindNavigationTarget(TargetId, Target) || !Target.bCanTarget)
	{
		return false;
	}

	SelectedTargetId = TargetId;
	return true;
}

bool UStargameSessionSubsystem::CycleNavigationTarget()
{
	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		return false;
	}

	TArray<FNavigationTargetDefinition> Targets;
	StarSystem->GetNavigationTargets(Targets);
	Targets.RemoveAll([](const FNavigationTargetDefinition& Target)
	{
		return !Target.bCanTarget || !Target.bShowInHud;
	});

	if (Targets.IsEmpty())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	int32 NextIndex = 0;
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		if (Targets[Index].TargetId == SelectedTargetId)
		{
			NextIndex = (Index + 1) % Targets.Num();
			break;
		}
	}

	SelectedTargetId = Targets[NextIndex].TargetId;
	return true;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot()
{
	FStargameM0SaveState LoadedState;
	if (!LoadDevelopmentSlot(LoadedState))
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	const bool bResolvedSystem = Catalog
		? Catalog->ResolveSystemDefinition(LoadedState.SystemId, SystemDefinition)
		: FFrontierTestFixtureProvider::ResolveSystemDefinition(LoadedState.SystemId, SystemDefinition);
	if (!bResolvedSystem)
	{
		ReportStartupError(FString::Printf(TEXT("Saved system '%s' could not be resolved."), *LoadedState.SystemId.ToString()));
		return false;
	}

	FString ValidationError;
	if (!ValidateNativeFallbackFixture(Catalog, SystemDefinition, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || !StarSystem->BuildSystem(SystemDefinition))
	{
		ReportStartupError(StarSystem ? StarSystem->GetLastBuildError() : TEXT("Missing star system subsystem."));
		return false;
	}

	CurrentSystemId = LoadedState.SystemId;
	CurrentSpawnZoneId = LoadedState.SpawnZoneId;
	SelectedTargetId = LoadedState.SelectedTargetId;
	ClockSnapshot = LoadedState.ClockSnapshot;
	if (ClockSnapshot.ClockOwner.IsNone())
	{
		ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(CurrentSystemId, LoadedState.ShipLocation.AuthoritativeSimulationTimeSeconds);
	}

	TSubclassOf<APawn> PawnClass = nullptr;
	FStartProfileDefinition StartProfile;
	const FName LoadStartProfileId = StartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: StartProfileId;
	const bool bResolvedLoadStartProfile = Catalog
		? Catalog->ResolveStartProfile(LoadStartProfileId, StartProfile)
		: FFrontierTestFixtureProvider::ResolveStartProfile(LoadStartProfileId, StartProfile);
	if (bResolvedLoadStartProfile && StartProfile.SystemId == LoadedState.SystemId && StartProfile.SpawnZoneId == LoadedState.SpawnZoneId)
	{
		FShipArchetypeDefinition Ship;
		if (Catalog && Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
		{
			PawnClass = Ship.PawnClass.LoadSynchronous();
		}
	}

	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!StarSystem->SpawnPlayerAtSpawnZone(CurrentSpawnZoneId, PlayerController, PawnClass))
	{
		ReportStartupError(StarSystem->GetLastBuildError());
		return false;
	}

	return true;
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot(const FStargameM0SaveState& State)
{
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::CreateSaveGameObject(UStargameM0SaveGame::StaticClass()));
	if (!SaveGame)
	{
		ReportStartupError(TEXT("Failed to create M0 save game object."));
		return false;
	}

	SaveGame->State = State;

	constexpr int32 UserIndex = 0;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, DevelopmentSlotName, UserIndex);
	if (!bSaved)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to save development slot '%s'."), DevelopmentSlotName));
	}
	return bSaved;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot(FStargameM0SaveState& OutState)
{
	constexpr int32 UserIndex = 0;
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::LoadGameFromSlot(DevelopmentSlotName, UserIndex));
	if (!SaveGame)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to load development slot '%s'."), DevelopmentSlotName));
		return false;
	}

	OutState = SaveGame->State;
	return true;
}

FStargameM0SaveState UStargameSessionSubsystem::MakeCurrentM0SaveState() const
{
	FStargameM0SaveState State;
	State.SystemId = CurrentSystemId;
	State.SpawnZoneId = CurrentSpawnZoneId;
	State.SelectedTargetId = SelectedTargetId;
	State.ClockSnapshot = ClockSnapshot;
	State.ShipLocation.SystemId = CurrentSystemId;
	State.ShipLocation.CoordinateFrame.FrameType = TEXT("local_free_flight");
	State.ShipLocation.CoordinateFrame.AnchorId = CurrentSpawnZoneId;
	State.ShipLocation.LocationMode = EShipLocationMode::Respawn;
	State.ShipLocation.AnchorId = CurrentSpawnZoneId;
	State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
	State.ShipLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	return State;
}

FString UStargameSessionSubsystem::GetM0DebugSummary() const
{
	const FString SaveSlotStatus = UGameplayStatics::DoesSaveGameExist(DevelopmentSlotName, 0)
		? TEXT("present")
		: TEXT("missing");

	return FString::Printf(
		TEXT("StartProfile=%s\nCurrentSystem=%s\nSpawnZone=%s\nSelectedTarget=%s\nClockOwner=%s\nSimulationTimeSeconds=%.2f\nLastStartResult=%d\nLastSessionError=%s\nSaveSlot=%s"),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString(),
		*SelectedTargetId.ToString(),
		*ClockSnapshot.ClockOwner.ToString(),
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		static_cast<int32>(LastStartSessionResult),
		*LastSessionError,
		*SaveSlotStatus);
}

bool UStargameSessionSubsystem::BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile)
{
	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	const bool bResolvedSystem = Catalog
		? Catalog->ResolveSystemDefinition(StartProfile.SystemId, SystemDefinition)
		: FFrontierTestFixtureProvider::ResolveSystemDefinition(StartProfile.SystemId, SystemDefinition);
	if (!bResolvedSystem)
	{
		LastStartSessionResult = EStartSessionResult::MissingSystemDefinition;
		ReportStartupError(FString::Printf(TEXT("Missing system definition '%s'."), *StartProfile.SystemId.ToString()));
		return false;
	}

	FString ValidationError;
	if (!ValidateNativeFallbackFixture(Catalog, SystemDefinition, ValidationError))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(ValidationError);
		return false;
	}

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(TEXT("Missing star system subsystem."));
		return false;
	}

	if (!StarSystem->BuildSystem(SystemDefinition))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(StarSystem->GetLastBuildError());
		return false;
	}

	FSpawnZoneDefinition SpawnZone;
	if (!StarSystem->FindSpawnZone(StartProfile.SpawnZoneId, SpawnZone))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(FString::Printf(TEXT("Missing spawn zone '%s'."), *StartProfile.SpawnZoneId.ToString()));
		return false;
	}

	StartProfileId = StartProfile.StartProfileId;
	CurrentSystemId = StartProfile.SystemId;
	CurrentSpawnZoneId = StartProfile.SpawnZoneId;
	SelectedTargetId = NAME_None;
	ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(CurrentSystemId, 0.0);

	TSubclassOf<APawn> PawnClass = nullptr;
	if (Catalog)
	{
		FShipArchetypeDefinition Ship;
		if (Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
		{
			PawnClass = Ship.PawnClass.LoadSynchronous();
		}
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!StarSystem->SpawnPlayerAtSpawnZone(CurrentSpawnZoneId, PlayerController, PawnClass))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(StarSystem->GetLastBuildError());
		return false;
	}

	return true;
}

void UStargameSessionSubsystem::ReportStartupError(const FString& Error)
{
	LastSessionError = Error;
	UE_LOG(LogStargameStartup, Error, TEXT("%s"), *LastSessionError);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.0f,
			FColor::Red,
			FString::Printf(TEXT("Stargame startup error: %s"), *LastSessionError));
	}
}
