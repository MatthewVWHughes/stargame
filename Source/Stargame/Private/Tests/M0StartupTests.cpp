#if WITH_DEV_AUTOMATION_TESTS

#include "Data/FrontierTestFixtureProvider.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flight/SpaceFlightPawn.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/StarSystemSubsystem.h"

namespace
{
	UWorld* CreateM0TestWorld(const TCHAR* WorldName)
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (World && World->PersistentLevel)
			{
				return World;
			}
		}

		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0StartProfileTest,
	"Stargame.M0.StartNewSession.UsesDefaultStartProfileData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0StartProfileTest::RunTest(const FString& Parameters)
{
	FStartProfileDefinition StartProfile;
	TestTrue(TEXT("frontier_test_start resolves"), FFrontierTestFixtureProvider::ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile));
	TestEqual(TEXT("Default start profile ID"), StartProfile.StartProfileId, FName(TEXT("frontier_test_start")));
	TestEqual(TEXT("Default system ID"), StartProfile.SystemId, FName(TEXT("frontier_test_01")));
	TestEqual(TEXT("Default spawn zone ID"), StartProfile.SpawnZoneId, FName(TEXT("spawn_deep_space")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0FixtureTargetsTest,
	"Stargame.M0.BuildM0Fixture.RegistersTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0FixtureTargetsTest::RunTest(const FString& Parameters)
{
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("frontier_test_01 resolves"), FFrontierTestFixtureProvider::ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	FString ValidationError;
	TestTrue(TEXT("M0 fixture validates"), FFrontierTestFixtureProvider::ValidateM0Fixture(SystemDefinition, ValidationError));
	if (!ValidationError.IsEmpty())
	{
		AddError(ValidationError);
	}

	TSet<FName> TargetIds;
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		TargetIds.Add(Body.NavigationTarget.TargetId);
	}
	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		TargetIds.Add(Station.NavigationTarget.TargetId);
	}
	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		TargetIds.Add(Gate.NavigationTarget.TargetId);
	}

	TestEqual(TEXT("M0 target count"), TargetIds.Num(), 3);
	TestTrue(TEXT("Contains ember"), TargetIds.Contains(FName(TEXT("ember"))));
	TestTrue(TEXT("Contains brink_watch"), TargetIds.Contains(FName(TEXT("brink_watch"))));
	TestTrue(TEXT("Contains frontier_gate_a"), TargetIds.Contains(FName(TEXT("frontier_gate_a"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0InvalidStartProfileTest,
	"Stargame.M0.StartNewSession.InvalidStartProfileDoesNotResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0InvalidStartProfileTest::RunTest(const FString& Parameters)
{
	FStartProfileDefinition StartProfile;
	TestFalse(TEXT("Unknown profile does not resolve"), FFrontierTestFixtureProvider::ResolveStartProfile(FName(TEXT("unknown_profile")), StartProfile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0NoLegacyStartupReferencesTest,
	"Stargame.M0.StartupSource.NoLegacyPrototypeReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0NoLegacyStartupReferencesTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::ProjectDir() / TEXT("Source/Stargame");
	TArray<FString> SourceFiles;
	IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*.cpp"), true, false);

	const TArray<FString> ForbiddenTokens = {
		TEXT("TActorIterator"),
		TEXT("GasGiant"),
		TEXT("PrototypeGasGiant"),
		TEXT("Earth"),
		TEXT("Mars"),
		TEXT("Luna"),
		TEXT("TEXT(\"sol\")"),
		TEXT("TEXT(\"Sol\")")
	};

	for (const FString& SourceFile : SourceFiles)
	{
		if (SourceFile.Contains(TEXT("/Tests/")))
		{
			continue;
		}

		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *SourceFile))
		{
			AddError(FString::Printf(TEXT("Failed to read source file '%s'."), *SourceFile));
			continue;
		}

		for (const FString& ForbiddenToken : ForbiddenTokens)
		{
			if (Contents.Contains(ForbiddenToken))
			{
				AddError(FString::Printf(TEXT("M0 source file '%s' contains forbidden legacy token '%s'."), *SourceFile, *ForbiddenToken));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0CycleTargetInputTest,
	"Stargame.M0.Targeting.CycleTargetInputConfigured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0CycleTargetInputTest::RunTest(const FString& Parameters)
{
	FString InputConfig;
	const FString InputConfigPath = FPaths::ProjectDir() / TEXT("Config/DefaultInput.ini");
	TestTrue(TEXT("DefaultInput.ini loads"), FFileHelper::LoadFileToString(InputConfig, *InputConfigPath));
	TestTrue(TEXT("CycleTarget input action exists"), InputConfig.Contains(TEXT("ActionName=\"CycleTarget\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0WorldBuildRegistersTargetsTest,
	"Stargame.M0.WorldBuild.RegistersTargetsAndDebugSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0WorldBuildRegistersTargetsTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateM0TestWorld(TEXT("M0WorldBuildTest"));
	TestNotNull(TEXT("World created"), World);
	if (!World)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("fixture resolves"), FFrontierTestFixtureProvider::ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	TestTrue(TEXT("BuildSystem succeeds"), StarSystem && StarSystem->BuildSystem(SystemDefinition));
	TestTrue(TEXT("Build is complete"), StarSystem && StarSystem->IsSystemBuildComplete());
	TestEqual(TEXT("Active system ID"), StarSystem->GetActiveSystemId(), FName(TEXT("frontier_test_01")));

	TArray<FName> EntityIds;
	StarSystem->GetRegisteredEntityIds(EntityIds);
	TestEqual(TEXT("Registered entity count"), EntityIds.Num(), 3);
	TestTrue(TEXT("Entity ember registered"), EntityIds.Contains(FName(TEXT("ember"))));
	TestTrue(TEXT("Entity brink_watch registered"), EntityIds.Contains(FName(TEXT("brink_watch"))));
	TestTrue(TEXT("Entity frontier_gate_a registered"), EntityIds.Contains(FName(TEXT("frontier_gate_a"))));

	TArray<FNavigationTargetDefinition> Targets;
	StarSystem->GetNavigationTargets(Targets);
	TestEqual(TEXT("Navigation target count"), Targets.Num(), 3);

	const FString DebugSummary = StarSystem->GetM0DebugSummary();
	TestTrue(TEXT("Debug includes build state"), DebugSummary.Contains(TEXT("BuildComplete=true")));
	TestTrue(TEXT("Debug includes target"), DebugSummary.Contains(TEXT("frontier_gate_a")));

	if (StarSystem)
	{
		StarSystem->TearDownActiveSystem();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0SpawnAndFlightTest,
	"Stargame.M0.Flight.SpawnAndMovePawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0SpawnAndFlightTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateM0TestWorld(TEXT("M0SpawnAndFlightTest"));
	TestNotNull(TEXT("World created"), World);
	if (!World)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("fixture resolves"), FFrontierTestFixtureProvider::ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestTrue(TEXT("BuildSystem succeeds"), StarSystem && StarSystem->BuildSystem(SystemDefinition));
	TestTrue(TEXT("Spawn player succeeds without controller"), StarSystem && StarSystem->SpawnPlayerAtSpawnZone(FName(TEXT("spawn_deep_space")), nullptr, ASpaceFlightPawn::StaticClass()));

	ASpaceFlightPawn* Pawn = nullptr;
	for (TActorIterator<ASpaceFlightPawn> It(World); It; ++It)
	{
		Pawn = *It;
		break;
	}

	TestNotNull(TEXT("Flight pawn spawned"), Pawn);
	if (Pawn)
	{
		const FVector StartLocation = Pawn->GetActorLocation();
		Pawn->AddActorWorldOffset(FVector(1000.0, 0.0, 0.0), false);
		TestTrue(TEXT("Pawn transform changes"), !Pawn->GetActorLocation().Equals(StartLocation));
		Pawn->Destroy();
	}

	if (StarSystem)
	{
		StarSystem->TearDownActiveSystem();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0SavePayloadRoundTripTest,
	"Stargame.M0.SaveReload.PreservesSystemAndSpawnZonePayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0SavePayloadRoundTripTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("Game instance object created"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	UStargameSessionSubsystem* Session = NewObject<UStargameSessionSubsystem>(GameInstance);
	TestNotNull(TEXT("Session object created"), Session);
	if (!Session)
	{
		return false;
	}

	FStargameM0SaveState State;
	State.SystemId = FName(TEXT("frontier_test_01"));
	State.SpawnZoneId = FName(TEXT("spawn_deep_space"));
	State.SelectedTargetId = FName(TEXT("brink_watch"));

	TestTrue(TEXT("Save payload succeeds"), Session->SaveDevelopmentSlot(State));

	FStargameM0SaveState LoadedState;
	TestTrue(TEXT("Load payload succeeds"), Session->LoadDevelopmentSlot(LoadedState));
	TestEqual(TEXT("Loaded system ID"), LoadedState.SystemId, State.SystemId);
	TestEqual(TEXT("Loaded spawn zone ID"), LoadedState.SpawnZoneId, State.SpawnZoneId);
	TestEqual(TEXT("Loaded selected target ID"), LoadedState.SelectedTargetId, State.SelectedTargetId);

	UGameplayStatics::DeleteGameInSlot(UStargameSessionSubsystem::DevelopmentSlotName, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM0InvalidSystemBuildStopsBeforeSpawnTest,
	"Stargame.M0.WorldBuild.InvalidDefinitionStopsBeforeSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM0InvalidSystemBuildStopsBeforeSpawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateM0TestWorld(TEXT("M0InvalidSystemBuildTest"));
	TestNotNull(TEXT("World created"), World);
	if (!World)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("fixture resolves"), FFrontierTestFixtureProvider::ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	if (SystemDefinition.Bodies.IsEmpty())
	{
		AddError(TEXT("Fixture has no body to duplicate for invalid build test."));
		return false;
	}

	FBodyDefinition DuplicateBody = SystemDefinition.Bodies[0];
	DuplicateBody.DisplayName = FText::FromString(TEXT("Duplicate Ember"));
	SystemDefinition.Bodies.Add(DuplicateBody);

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	TestFalse(TEXT("BuildSystem rejects duplicate entity IDs"), StarSystem && StarSystem->BuildSystem(SystemDefinition));
	TestFalse(TEXT("Build is not complete"), StarSystem && StarSystem->IsSystemBuildComplete());
	TestTrue(TEXT("Build error explains duplicate ID"), StarSystem && StarSystem->GetLastBuildError().Contains(TEXT("Duplicate active system entity ID")));

	TArray<FName> EntityIds;
	if (StarSystem)
	{
		StarSystem->GetRegisteredEntityIds(EntityIds);
	}
	TestEqual(TEXT("No active entities registered after invalid build"), EntityIds.Num(), 0);

	const FStargameValidationReport Report = StarSystem ? StarSystem->GetLastBuildValidationReport() : FStargameValidationReport();
	TestTrue(TEXT("Validation report has blocking issue"), Report.HasBlockingIssues());
	TestEqual(TEXT("Validation report system"), Report.SystemId, FName(TEXT("frontier_test_01")));
	TestEqual(TEXT("Validation report result"), Report.ComputeResultType(), EStargameValidationResultType::Invalid);
	return true;
}

#endif
