#if WITH_DEV_AUTOMATION_TESTS

#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Space/StarSystemSubsystem.h"

namespace
{
	UStarCatalogSubsystem* CreateM1Catalog()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		UStarCatalogSubsystem* Catalog = NewObject<UStarCatalogSubsystem>(GameInstance);
		if (Catalog)
		{
			Catalog->BuildAssetCatalogCache(false);
		}
		return Catalog;
	}

	UWorld* FindM1AutomationWorld()
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

	bool AssetManagerScansRoot(FPrimaryAssetType AssetType, const FString& RequiredRoot)
	{
		const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
		if (!Settings)
		{
			return false;
		}

		for (const FPrimaryAssetTypeInfo& TypeInfo : Settings->PrimaryAssetTypesToScan)
		{
			if (TypeInfo.PrimaryAssetType != AssetType)
			{
				continue;
			}

			for (const FDirectoryPath& Directory : TypeInfo.GetDirectories())
			{
				if (Directory.Path == RequiredRoot || RequiredRoot.StartsWith(Directory.Path + TEXT("/")))
				{
					return true;
				}
			}
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1CatalogValidationTest,
	"Stargame.M1.Catalog.ValidatesFrontierFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1CatalogValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}
	TestTrue(TEXT("M1 catalog is Asset Manager backed"), Catalog->IsUsingAssetCatalog());

	FStartProfileDefinition StartProfile;
	TestTrue(TEXT("M1 start profile resolves"), Catalog->ResolveStartProfile(FName(TEXT("frontier_test_start")), StartProfile));
	TestEqual(TEXT("M1 start profile targets frontier fixture"), StartProfile.SystemId, FName(TEXT("frontier_test_01")));
	TestEqual(TEXT("M1 start profile uses deep-space spawn"), StartProfile.SpawnZoneId, FName(TEXT("spawn_deep_space")));
	TestEqual(TEXT("M1 start profile has ship archetype"), StartProfile.ShipArchetypeId, FName(TEXT("wayfarer_mk1")));

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M1 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	TestTrue(TEXT("M1 body visual profile is set"), SystemDefinition.Bodies.Num() == 1 && SystemDefinition.Bodies[0].VisualProfileId.IsValid());
	TestTrue(TEXT("M1 station profile is set"), SystemDefinition.Stations.Num() == 1 && SystemDefinition.Stations[0].StationProfileId.IsValid());
	TestEqual(TEXT("M1 station docking port count"), SystemDefinition.Stations.Num() == 1 ? SystemDefinition.Stations[0].DockingPorts.Num() : 0, 1);
	TestTrue(TEXT("M1 gate profile is set"), SystemDefinition.Gates.Num() == 1 && SystemDefinition.Gates[0].GateProfileId.IsValid());
	TestEqual(TEXT("M1 gravity well count"), SystemDefinition.GravityWells.Num(), 1);
	TestEqual(TEXT("M1 map entry count"), SystemDefinition.MapEntries.Num(), 3);

	FShipArchetypeDefinition Ship;
	TestTrue(TEXT("M1 ship archetype resolves"), Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship));
	TestFalse(TEXT("M1 ship pawn class is set"), Ship.PawnClass.IsNull());
	TestFalse(TEXT("M1 ship class tags are set"), Ship.ShipClassTags.IsEmpty());
	TestTrue(TEXT("M1 cargo mass is positive"), Ship.DefaultCargoCapacityMassKg > 0.0);
	TestTrue(TEXT("M1 cargo volume is positive"), Ship.DefaultCargoCapacityVolumeM3 > 0.0);

	const FStargameValidationReport Report = Catalog->ValidateM1Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M1 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1RegistryRebuildTest,
	"Stargame.M1.Registry.BuildTeardownRebuildInvalidatesCleanly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1RegistryRebuildTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M1 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}

	TestTrue(TEXT("First build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	TArray<FName> EntityIds;
	StarSystem->GetRegisteredEntityIds(EntityIds);
	TestEqual(TEXT("First build entity count"), EntityIds.Num(), 3);

	TArray<FName> SpawnZoneIds;
	StarSystem->GetRegisteredSpawnZoneIds(SpawnZoneIds);
	TestEqual(TEXT("First build spawn zone count"), SpawnZoneIds.Num(), 1);

	TArray<FName> GravityWellIds;
	StarSystem->GetRegisteredGravityWellIds(GravityWellIds);
	TestEqual(TEXT("First build gravity well count"), GravityWellIds.Num(), 1);

	TArray<FName> DockingPortIds;
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	TestEqual(TEXT("First build docking port count"), DockingPortIds.Num(), 1);
	TestTrue(TEXT("First build docking port registry ID"), DockingPortIds.Contains(FName(TEXT("brink_watch/pad_01"))));

	TArray<FName> MapEntryIds;
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("First build map entry count"), MapEntryIds.Num(), 3);

	const FString DebugSummary = StarSystem->GetM1DebugSummary();
	TestTrue(TEXT("M1 debug summary comes from registry entities"), DebugSummary.Contains(TEXT("Entities=brink_watch,ember,frontier_gate_a")));
	TestTrue(TEXT("M1 debug summary includes spawn zones"), DebugSummary.Contains(TEXT("SpawnZones=spawn_deep_space")));
	TestTrue(TEXT("M1 debug summary includes docking ports"), DebugSummary.Contains(TEXT("DockingPorts=brink_watch/pad_01")));
	TestTrue(TEXT("M1 debug summary includes gravity wells"), DebugSummary.Contains(TEXT("GravityWells=ember_gravity_well")));
	TestTrue(TEXT("M1 debug summary includes map entries"), DebugSummary.Contains(TEXT("MapEntries=brink_watch,ember,frontier_gate_a")));
	TestTrue(TEXT("M1 debug summary includes navigation targets"), DebugSummary.Contains(TEXT("NavigationTargets=brink_watch,ember,frontier_gate_a")));

	StarSystem->TearDownActiveSystem();
	StarSystem->GetRegisteredEntityIds(EntityIds);
	StarSystem->GetRegisteredSpawnZoneIds(SpawnZoneIds);
	StarSystem->GetRegisteredGravityWellIds(GravityWellIds);
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("Teardown clears entities"), EntityIds.Num(), 0);
	TestEqual(TEXT("Teardown clears spawn zones"), SpawnZoneIds.Num(), 0);
	TestEqual(TEXT("Teardown clears gravity wells"), GravityWellIds.Num(), 0);
	TestEqual(TEXT("Teardown clears docking ports"), DockingPortIds.Num(), 0);
	TestEqual(TEXT("Teardown clears map entries"), MapEntryIds.Num(), 0);

	TestTrue(TEXT("Rebuild succeeds"), StarSystem->BuildSystem(SystemDefinition));
	StarSystem->GetRegisteredEntityIds(EntityIds);
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	TestEqual(TEXT("Rebuild entity count"), EntityIds.Num(), 3);
	TestEqual(TEXT("Rebuild map entry count"), MapEntryIds.Num(), 3);
	TestEqual(TEXT("Rebuild docking port count"), DockingPortIds.Num(), 1);

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1AssetManagerConfigTest,
	"Stargame.M1.AssetManager.RequiredTypesConfigured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1AssetManagerConfigTest::RunTest(const FString& Parameters)
{
	struct FRequiredAssetRoot
	{
		FPrimaryAssetType AssetType;
		FString Root;
	};
	const TArray<FRequiredAssetRoot> RequiredRoots = {
		{ UStarCatalogAsset::AssetType, TEXT("/Game/Data/Catalog") },
		{ UStartProfileAsset::AssetType, TEXT("/Game/Data/StartProfiles") },
		{ UStarSystemDefinitionAsset::AssetType, TEXT("/Game/Data/Systems") },
		{ UBodyVisualProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Bodies") },
		{ UAtmosphereProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Atmospheres") },
		{ UStationProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Stations") },
		{ UGateProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Gates") },
		{ UShipArchetypeDataAsset::AssetType, TEXT("/Game/Data/Ships/Archetypes") },
		{ UShipMovementProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipDurabilityProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipLoadoutProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipResourceProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") }
	};

	for (const FRequiredAssetRoot& RequiredRoot : RequiredRoots)
	{
		TestTrue(
			FString::Printf(TEXT("Asset Manager scans '%s' under '%s'"), *RequiredRoot.AssetType.ToString(), *RequiredRoot.Root),
			AssetManagerScansRoot(RequiredRoot.AssetType, RequiredRoot.Root));
	}

	return true;
}

#endif
