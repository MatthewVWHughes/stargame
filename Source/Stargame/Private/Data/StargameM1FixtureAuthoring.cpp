#include "Data/StargameM1FixtureAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FrontierTestFixtureProvider.h"
#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Flight/SpaceFlightPawn.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	template <typename AssetType>
	AssetType* CreateOrLoadAsset(const TCHAR* PackageName, const TCHAR* AssetName)
	{
		UPackage* Package = CreatePackage(PackageName);
		Package->FullyLoad();

		if (AssetType* ExistingAsset = FindObject<AssetType>(Package, AssetName))
		{
			return ExistingAsset;
		}

		AssetType* Asset = NewObject<AssetType>(Package, AssetType::StaticClass(), FName(AssetName), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	bool SaveAsset(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetPackage();
		Package->MarkPackageDirty();

		const FString PackageName = Package->GetName();
		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	FNavigationTargetDefinition MakeNavigationTarget(FName TargetId, const TCHAR* DisplayName, FName TargetType)
	{
		FNavigationTargetDefinition Target;
		Target.TargetId = TargetId;
		Target.DisplayName = FText::FromString(DisplayName);
		Target.TargetType = TargetType;
		Target.FrameType = TEXT("system_barycentric");
		Target.bShowInHud = true;
		Target.bShowInSystemMap = true;
		Target.bCanTarget = true;
		return Target;
	}

	FStarSystemDefinition MakeM1SystemDefinition()
	{
		FStarSystemDefinition SystemDefinition;
		FFrontierTestFixtureProvider::ResolveSystemDefinition(FFrontierTestFixtureProvider::FrontierSystemId, SystemDefinition);
		SystemDefinition.SourceType = ESystemSourceType::Authored;
		SystemDefinition.Seed = 101;
		SystemDefinition.Scale = FStargameScaleContract();

		SystemDefinition.Bodies.Reset();
		SystemDefinition.Stations.Reset();

		auto MakeBody = [](FName BodyId, const TCHAR* DisplayName, FName BodyType, double RadiusCm, FName ParentId, double SemiMajorAxisCm, double PeriodSeconds, double PhaseOffsetRadians)
		{
			FBodyDefinition Body;
			Body.BodyId = BodyId;
			Body.DisplayName = FText::FromString(DisplayName);
			Body.BodyType = BodyType;
			Body.FrameType = ParentId.IsNone() ? FName(TEXT("system_barycentric")) : FName(TEXT("body_relative"));
			Body.AnchorId = ParentId;
			Body.VisualRadiusCm = RadiusCm;
			Body.PhysicalReferenceRadiusCm = RadiusCm;
			Body.CollisionRadiusCm = RadiusCm;
			Body.VisualProfileId = FPrimaryAssetId(UBodyVisualProfileAsset::AssetType, TEXT("ember_visual"));
			Body.NavigationTarget = MakeNavigationTarget(BodyId, DisplayName, TEXT("body"));
			Body.NavigationTarget.FrameType = Body.FrameType;
			Body.NavigationTarget.AnchorId = ParentId;
			Body.Orbit.ParentId = ParentId;
			Body.Orbit.SemiMajorAxisCm = SemiMajorAxisCm;
			Body.Orbit.PeriodSeconds = PeriodSeconds;
			Body.Orbit.PhaseOffsetRadians = PhaseOffsetRadians;
			return Body;
		};

		SystemDefinition.Bodies.Add(MakeBody(TEXT("frontier_primary"), TEXT("Frontier Primary"), TEXT("star"), 200000.0, NAME_None, 0.0, 0.0, 0.0));
		SystemDefinition.Bodies.Add(MakeBody(TEXT("ember"), TEXT("Ember"), TEXT("rocky_planet"), 45000.0, TEXT("frontier_primary"), 12000000.0, 900.0, 0.2));
		SystemDefinition.Bodies.Add(MakeBody(TEXT("brink"), TEXT("Brink"), TEXT("outer_planet"), 90000.0, TEXT("frontier_primary"), 40000000.0, 1800.0, 2.2));
		SystemDefinition.Bodies.Add(MakeBody(TEXT("brink_minor"), TEXT("Brink Minor"), TEXT("moon"), 18000.0, TEXT("brink"), 3500000.0, 240.0, 0.7));

		auto MakeDockingPort = []()
		{
			FDockingPortDefinition DockingPort;
			DockingPort.PortId = TEXT("pad_01");
			DockingPort.DisplayName = FText::FromString(TEXT("Pad 01"));
			DockingPort.ApproachTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -18000.0, 0.0));
			DockingPort.DockedTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -12000.0, 0.0));
			DockingPort.UndockTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -22000.0, 0.0));
			DockingPort.AllowedShipClasses.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Light"), false));
			return DockingPort;
		};

		FStationDefinition BrinkWatch;
		BrinkWatch.StationId = TEXT("brink_watch");
		BrinkWatch.DisplayName = FText::FromString(TEXT("Brink Watch"));
		BrinkWatch.FrameType = TEXT("station_relative");
		BrinkWatch.AnchorId = TEXT("brink");
		BrinkWatch.VisualRadiusCm = 12000.0;
		BrinkWatch.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
		BrinkWatch.DockingPorts = { MakeDockingPort() };
		BrinkWatch.NavigationTarget = MakeNavigationTarget(TEXT("brink_watch"), TEXT("Brink Watch"), TEXT("station"));
		BrinkWatch.NavigationTarget.FrameType = BrinkWatch.FrameType;
		BrinkWatch.NavigationTarget.AnchorId = BrinkWatch.AnchorId;
		BrinkWatch.Orbit.ParentId = TEXT("brink");
		BrinkWatch.Orbit.SemiMajorAxisCm = 3000000.0;
		BrinkWatch.Orbit.PeriodSeconds = 90.0;
		BrinkWatch.Orbit.PhaseOffsetRadians = 1.4;
		SystemDefinition.Stations.Add(BrinkWatch);

		FStationDefinition WayfarerDepot;
		WayfarerDepot.StationId = TEXT("wayfarer_depot");
		WayfarerDepot.DisplayName = FText::FromString(TEXT("Wayfarer Depot"));
		WayfarerDepot.FrameType = TEXT("station_relative");
		WayfarerDepot.AnchorId = TEXT("frontier_primary");
		WayfarerDepot.VisualRadiusCm = 14000.0;
		WayfarerDepot.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
		WayfarerDepot.DockingPorts = { MakeDockingPort() };
		WayfarerDepot.NavigationTarget = MakeNavigationTarget(TEXT("wayfarer_depot"), TEXT("Wayfarer Depot"), TEXT("station"));
		WayfarerDepot.NavigationTarget.FrameType = WayfarerDepot.FrameType;
		WayfarerDepot.NavigationTarget.AnchorId = WayfarerDepot.AnchorId;
		WayfarerDepot.Orbit.ParentId = TEXT("frontier_primary");
		WayfarerDepot.Orbit.SemiMajorAxisCm = 26000000.0;
		WayfarerDepot.Orbit.PeriodSeconds = 1500.0;
		WayfarerDepot.Orbit.PhaseOffsetRadians = 3.0;
		SystemDefinition.Stations.Add(WayfarerDepot);

		if (SystemDefinition.Gates.Num() > 0)
		{
			SystemDefinition.Gates[0].GateProfileId = FPrimaryAssetId(UGateProfileAsset::AssetType, TEXT("frontier_gate_basic"));
			SystemDefinition.Gates[0].NavigationTarget = MakeNavigationTarget(TEXT("frontier_gate_a"), TEXT("Frontier Gate A"), TEXT("gate"));
		}

		FGravityWellDefinition EmberWell;
		EmberWell.WellId = TEXT("ember_gravity_well");
		EmberWell.AnchorBodyId = TEXT("ember");
		EmberWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
		EmberWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
		EmberWell.DropoutRadiusCm = 350000.0;

		FGravityWellDefinition BrinkWell;
		BrinkWell.WellId = TEXT("brink_gravity_well");
		BrinkWell.AnchorBodyId = TEXT("brink");
		BrinkWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
		BrinkWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
		BrinkWell.DropoutRadiusCm = 350000.0;
		SystemDefinition.GravityWells = { EmberWell, BrinkWell };

		SystemDefinition.MapEntries.Reset();
		const TArray<TPair<FName, FName>> MapSources = {
			{ TEXT("frontier_primary"), TEXT("body") },
			{ TEXT("ember"), TEXT("body") },
			{ TEXT("brink"), TEXT("body") },
			{ TEXT("brink_minor"), TEXT("body") },
			{ TEXT("brink_watch"), TEXT("station") },
			{ TEXT("wayfarer_depot"), TEXT("station") },
			{ TEXT("frontier_gate_a"), TEXT("gate") }
		};
		for (const TPair<FName, FName>& MapSource : MapSources)
		{
			FMapEntryDefinition Entry;
			Entry.MapEntryId = MapSource.Key;
			Entry.SourceId = MapSource.Key;
			Entry.EntryType = MapSource.Value;
			Entry.bVisibleInSystemMap = true;
			SystemDefinition.MapEntries.Add(Entry);
		}

		return SystemDefinition;
	}
}

int32 UStargameCreateM1FixtureAssetsCommandlet::Main(const FString& Params)
{
	TArray<UObject*> AssetsToSave;

	UBodyVisualProfileAsset* BodyVisual = CreateOrLoadAsset<UBodyVisualProfileAsset>(TEXT("/Game/Data/Profiles/Bodies/ember_visual"), TEXT("ember_visual"));
	BodyVisual->VisualProfileId = TEXT("ember_visual");
	BodyVisual->VisualKind = TEXT("placeholder_body");
	AssetsToSave.Add(BodyVisual);

	UAtmosphereProfileAsset* Atmosphere = CreateOrLoadAsset<UAtmosphereProfileAsset>(TEXT("/Game/Data/Profiles/Atmospheres/no_atmosphere"), TEXT("no_atmosphere"));
	Atmosphere->AtmosphereProfileId = TEXT("no_atmosphere");
	Atmosphere->AtmosphereKind = TEXT("none");
	AssetsToSave.Add(Atmosphere);

	UStationProfileAsset* StationProfile = CreateOrLoadAsset<UStationProfileAsset>(TEXT("/Game/Data/Profiles/Stations/frontier_station_basic"), TEXT("frontier_station_basic"));
	StationProfile->StationProfileId = TEXT("frontier_station_basic");
	StationProfile->StationRole = TEXT("outpost");
	AssetsToSave.Add(StationProfile);

	UGateProfileAsset* GateProfile = CreateOrLoadAsset<UGateProfileAsset>(TEXT("/Game/Data/Profiles/Gates/frontier_gate_basic"), TEXT("frontier_gate_basic"));
	GateProfile->GateProfileId = TEXT("frontier_gate_basic");
	GateProfile->GateKind = TEXT("inactive_frontier_gate");
	AssetsToSave.Add(GateProfile);

	UShipMovementProfileAsset* Movement = CreateOrLoadAsset<UShipMovementProfileAsset>(TEXT("/Game/Data/Ships/Profiles/wayfarer_movement_basic"), TEXT("wayfarer_movement_basic"));
	Movement->Definition.MovementProfileId = TEXT("wayfarer_movement_basic");
	Movement->Definition.SupportedFlightModeTags.Reset();
	Movement->Definition.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Normal"), false));
	Movement->Definition.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Supercruise"), false));
	Movement->Definition.NormalFlightTuningId = TEXT("normal_basic");
	Movement->Definition.SupercruiseTuningId = TEXT("supercruise_basic");
	Movement->Definition.MaxLocalSpeedCmPerSec = 24000.0;
	Movement->Definition.MaxAngularSpeedDegPerSec = 80.0;
	AssetsToSave.Add(Movement);

	UShipDurabilityProfileAsset* Durability = CreateOrLoadAsset<UShipDurabilityProfileAsset>(TEXT("/Game/Data/Ships/Profiles/wayfarer_durability_basic"), TEXT("wayfarer_durability_basic"));
	Durability->Definition.DurabilityProfileId = TEXT("wayfarer_durability_basic");
	Durability->Definition.MaxHull = 100.0;
	Durability->Definition.MaxShield = 50.0;
	Durability->Definition.DisabledHullFraction = 0.15;
	Durability->Definition.DestroyedHullFraction = 0.0;
	AssetsToSave.Add(Durability);

	UShipResourceProfileAsset* Resources = CreateOrLoadAsset<UShipResourceProfileAsset>(TEXT("/Game/Data/Ships/Profiles/wayfarer_resources_basic"), TEXT("wayfarer_resources_basic"));
	Resources->Definition.ResourceProfileId = TEXT("wayfarer_resources_basic");
	FShipResourceCapacityDefinition FuelCapacity;
	FuelCapacity.ResourceId = TEXT("fuel");
	FuelCapacity.Capacity = 100.0;
	Resources->Definition.ResourceCapacities = { FuelCapacity };
	Resources->Definition.DefaultFillPolicy = TEXT("full");
	AssetsToSave.Add(Resources);

	UShipLoadoutProfileAsset* Loadout = CreateOrLoadAsset<UShipLoadoutProfileAsset>(TEXT("/Game/Data/Ships/Profiles/wayfarer_loadout_basic"), TEXT("wayfarer_loadout_basic"));
	Loadout->Definition.LoadoutProfileId = TEXT("wayfarer_loadout_basic");
	Loadout->Definition.DefaultInstalledSlotIds = { TEXT("utility_empty") };
	Loadout->Definition.RequiredResourceProfileId = Resources->Definition.ResourceProfileId;
	AssetsToSave.Add(Loadout);

	UShipArchetypeDataAsset* Ship = CreateOrLoadAsset<UShipArchetypeDataAsset>(TEXT("/Game/Data/Ships/Archetypes/wayfarer_mk1"), TEXT("wayfarer_mk1"));
	Ship->Definition.ShipArchetypeId = TEXT("wayfarer_mk1");
	Ship->Definition.DisplayName = FText::FromString(TEXT("Wayfarer Mk1"));
	Ship->Definition.ShipClassTags.Reset();
	Ship->Definition.ShipClassTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Light"), false));
	Ship->Definition.PawnClass = ASpaceFlightPawn::StaticClass();
	Ship->Definition.MovementProfileId = Movement->Definition.MovementProfileId;
	Ship->Definition.DefaultCargoCapacityMassKg = 1000.0;
	Ship->Definition.DefaultCargoCapacityVolumeM3 = 12.0;
	Ship->Definition.DefaultDurabilityProfileId = Durability->Definition.DurabilityProfileId;
	Ship->Definition.DefaultLoadoutProfileId = Loadout->Definition.LoadoutProfileId;
	Ship->Definition.DefaultResourceProfileId = Resources->Definition.ResourceProfileId;
	Ship->Definition.DockingSizeClass = TEXT("small");
	AssetsToSave.Add(Ship);

	UStarSystemDefinitionAsset* System = CreateOrLoadAsset<UStarSystemDefinitionAsset>(TEXT("/Game/Data/Systems/frontier_test_01"), TEXT("frontier_test_01"));
	System->Definition = MakeM1SystemDefinition();
	AssetsToSave.Add(System);

	UStartProfileAsset* StartProfile = CreateOrLoadAsset<UStartProfileAsset>(TEXT("/Game/Data/StartProfiles/frontier_test_start"), TEXT("frontier_test_start"));
	FFrontierTestFixtureProvider::ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile->Definition);
	StartProfile->Definition.ShipArchetypeId = Ship->Definition.ShipArchetypeId;
	AssetsToSave.Add(StartProfile);

	UStarCatalogAsset* Catalog = CreateOrLoadAsset<UStarCatalogAsset>(TEXT("/Game/Data/Catalog/frontier_catalog"), TEXT("frontier_catalog"));
	Catalog->CatalogId = TEXT("frontier_catalog");
	Catalog->Systems.Reset();
	FStarCatalogEntry CatalogEntry;
	CatalogEntry.SystemId = System->Definition.SystemId;
	CatalogEntry.DisplayName = System->Definition.DisplayName;
	CatalogEntry.StellarClass = TEXT("test_fixture");
	CatalogEntry.CatalogSource = TEXT("authored_m1_fixture");
	CatalogEntry.SystemDefinitionAsset = FPrimaryAssetId(UStarSystemDefinitionAsset::AssetType, System->Definition.SystemId);
	Catalog->Systems.Add(CatalogEntry);
	AssetsToSave.Add(Catalog);

	bool bSavedAll = true;
	for (UObject* Asset : AssetsToSave)
	{
		bSavedAll &= SaveAsset(Asset);
	}

	UE_LOG(LogTemp, Display, TEXT("M1 fixture asset authoring %s."), bSavedAll ? TEXT("succeeded") : TEXT("failed"));
	return bSavedAll ? 0 : 1;
}

int32 UStargameValidateContentCommandlet::Main(const FString& Params)
{
	FString ProfileString;
	FParse::Value(*Params, TEXT("ValidationProfile="), ProfileString);

	FName SystemId(TEXT("frontier_test_01"));
	FString SystemIdString;
	if (FParse::Value(*Params, TEXT("SystemId="), SystemIdString))
	{
		SystemId = FName(*SystemIdString);
	}

	const bool bValidateM1 = ProfileString.Equals(TEXT("M1"), ESearchCase::IgnoreCase);
	const bool bValidateM2 = ProfileString.Equals(TEXT("M2"), ESearchCase::IgnoreCase);
	const bool bValidateM3 = ProfileString.Equals(TEXT("M3"), ESearchCase::IgnoreCase);
	if (!bValidateM1 && !bValidateM2 && !bValidateM3)
	{
		UE_LOG(LogTemp, Error, TEXT("Unsupported validation profile '%s'."), *ProfileString);
		return 1;
	}

	UGameInstance* ValidationGameInstance = NewObject<UGameInstance>();
	UStarCatalogSubsystem* Catalog = NewObject<UStarCatalogSubsystem>(ValidationGameInstance);
	if (!Catalog->BuildAssetCatalogCache(false))
	{
		UE_LOG(LogTemp, Error, TEXT("%s validation could not build an Asset Manager-backed catalog."), bValidateM3 ? TEXT("M3") : (bValidateM2 ? TEXT("M2") : TEXT("M1")));
		return 1;
	}

	const FStargameValidationReport Report = bValidateM3
		? Catalog->ValidateM3Fixture(SystemId)
		: (bValidateM2 ? Catalog->ValidateM2Fixture(SystemId) : Catalog->ValidateM1Fixture(SystemId));
	for (const FStargameValidationIssue& Issue : Report.Issues)
	{
		const ELogVerbosity::Type Verbosity = (Issue.Severity == EStargameValidationSeverity::Error || Issue.Severity == EStargameValidationSeverity::Fatal)
			? ELogVerbosity::Error
			: ELogVerbosity::Warning;
		FMsg::Logf(__FILE__, __LINE__, LogTemp.GetCategoryName(), Verbosity, TEXT("[%s] %s: %s"),
			*UEnum::GetValueAsString(Issue.Severity),
			*Issue.RuleId.ToString(),
			*Issue.Message);
	}

	if (Report.HasBlockingIssues())
	{
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("%s validation passed for system '%s'."), bValidateM3 ? TEXT("M3") : (bValidateM2 ? TEXT("M2") : TEXT("M1")), *SystemId.ToString());
	return 0;
}
