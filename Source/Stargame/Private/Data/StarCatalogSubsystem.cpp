#include "Data/StarCatalogSubsystem.h"

#include "Data/FrontierTestFixtureProvider.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Flight/SpaceFlightPawn.h"

namespace
{
	struct FM1RequiredAssetType
	{
		FPrimaryAssetType AssetType;
		FString RequiredRoot;
	};

	const TArray<FM1RequiredAssetType>& GetM1RequiredAssetTypes()
	{
		static const TArray<FM1RequiredAssetType> RequiredTypes = {
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
		return RequiredTypes;
	}

	bool AssetManagerSettingsScanRoot(FPrimaryAssetType AssetType, const FString& RequiredRoot)
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

	bool IsPrimaryAssetReferenceResolved(const FPrimaryAssetId& AssetId, FPrimaryAssetType ExpectedType)
	{
		if (!AssetId.IsValid() || AssetId.PrimaryAssetType != ExpectedType)
		{
			return false;
		}

		const FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(AssetId);
		return AssetPath.IsValid() && (AssetPath.ResolveObject() || AssetPath.TryLoad());
	}
}

void UStarCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildAssetCatalogCache(true);
}

void UStarCatalogSubsystem::ResetCatalogCache()
{
	CatalogEntries.Reset();
	StartProfilesById.Reset();
	SystemsById.Reset();
	ShipArchetypesById.Reset();
	MovementProfilesById.Reset();
	DurabilityProfilesById.Reset();
	LoadoutProfilesById.Reset();
	ResourceProfilesById.Reset();
	bUsingAssetCatalog = false;
}

void UStarCatalogSubsystem::BuildNativeFixtureCache()
{
	ResetCatalogCache();

	FStartProfileDefinition StartProfile;
	if (FFrontierTestFixtureProvider::ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile))
	{
		StartProfile.ShipArchetypeId = TEXT("wayfarer_mk1");
		StartProfilesById.Add(StartProfile.StartProfileId, StartProfile);
	}

	FStarSystemDefinition SystemDefinition;
	if (FFrontierTestFixtureProvider::ResolveSystemDefinition(FFrontierTestFixtureProvider::FrontierSystemId, SystemDefinition))
	{
		if (SystemDefinition.Bodies.Num() > 0)
		{
			SystemDefinition.Bodies[0].VisualProfileId = FPrimaryAssetId(UBodyVisualProfileAsset::AssetType, TEXT("ember_visual"));
		}
		if (SystemDefinition.Stations.Num() > 0)
		{
			SystemDefinition.Stations[0].StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
		}
		if (SystemDefinition.Gates.Num() > 0)
		{
			SystemDefinition.Gates[0].GateProfileId = FPrimaryAssetId(UGateProfileAsset::AssetType, TEXT("frontier_gate_basic"));
		}

		FGravityWellDefinition Well;
		Well.WellId = TEXT("ember_gravity_well");
		Well.AnchorBodyId = TEXT("ember");
		Well.SlowdownRadiusCm = 120000.0;
		Well.LockoutRadiusCm = 90000.0;
		Well.DropoutRadiusCm = 45000.0;
		SystemDefinition.GravityWells = { Well };

		for (const FBodyDefinition& Body : SystemDefinition.Bodies)
		{
			FMapEntryDefinition Entry;
			Entry.MapEntryId = Body.BodyId;
			Entry.SourceId = Body.BodyId;
			Entry.EntryType = TEXT("body");
			SystemDefinition.MapEntries.Add(Entry);
		}
		for (const FStationDefinition& Station : SystemDefinition.Stations)
		{
			FMapEntryDefinition Entry;
			Entry.MapEntryId = Station.StationId;
			Entry.SourceId = Station.StationId;
			Entry.EntryType = TEXT("station");
			SystemDefinition.MapEntries.Add(Entry);
		}
		for (const FGateDefinition& Gate : SystemDefinition.Gates)
		{
			FMapEntryDefinition Entry;
			Entry.MapEntryId = Gate.GateId;
			Entry.SourceId = Gate.GateId;
			Entry.EntryType = TEXT("gate");
			SystemDefinition.MapEntries.Add(Entry);
		}

		SystemsById.Add(SystemDefinition.SystemId, SystemDefinition);

		FStarCatalogEntry CatalogEntry;
		CatalogEntry.SystemId = SystemDefinition.SystemId;
		CatalogEntry.DisplayName = SystemDefinition.DisplayName;
		CatalogEntry.StellarClass = TEXT("test_fixture");
		CatalogEntry.CatalogSource = TEXT("native_fixture");
		CatalogEntry.SystemDefinitionAsset = FPrimaryAssetId(UStarSystemDefinitionAsset::AssetType, SystemDefinition.SystemId);
		CatalogEntries.Add(CatalogEntry);
	}

	FShipMovementProfileDefinition Movement;
	Movement.MovementProfileId = TEXT("wayfarer_movement_basic");
	Movement.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Normal"), false));
	Movement.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Supercruise"), false));
	Movement.NormalFlightTuningId = TEXT("normal_basic");
	Movement.SupercruiseTuningId = TEXT("supercruise_basic");
	Movement.MaxLocalSpeedCmPerSec = 50000.0;
	Movement.MaxAngularSpeedDegPerSec = 80.0;
	MovementProfilesById.Add(Movement.MovementProfileId, Movement);

	FShipDurabilityProfileDefinition Durability;
	Durability.DurabilityProfileId = TEXT("wayfarer_durability_basic");
	Durability.MaxHull = 100.0;
	Durability.MaxShield = 50.0;
	Durability.DisabledHullFraction = 0.15;
	Durability.DestroyedHullFraction = 0.0;
	DurabilityProfilesById.Add(Durability.DurabilityProfileId, Durability);

	FShipResourceProfileDefinition Resources;
	Resources.ResourceProfileId = TEXT("wayfarer_resources_basic");
	FShipResourceCapacityDefinition FuelCapacity;
	FuelCapacity.ResourceId = TEXT("fuel");
	FuelCapacity.Capacity = 100.0;
	Resources.ResourceCapacities = { FuelCapacity };
	Resources.DefaultFillPolicy = TEXT("full");
	ResourceProfilesById.Add(Resources.ResourceProfileId, Resources);

	FShipLoadoutProfileDefinition Loadout;
	Loadout.LoadoutProfileId = TEXT("wayfarer_loadout_basic");
	Loadout.DefaultInstalledSlotIds = { TEXT("utility_empty") };
	Loadout.RequiredResourceProfileId = Resources.ResourceProfileId;
	LoadoutProfilesById.Add(Loadout.LoadoutProfileId, Loadout);

	FShipArchetypeDefinition Ship;
	Ship.ShipArchetypeId = TEXT("wayfarer_mk1");
	Ship.DisplayName = FText::FromString(TEXT("Wayfarer Mk1"));
	Ship.ShipClassTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Light"), false));
	Ship.PawnClass = ASpaceFlightPawn::StaticClass();
	Ship.MovementProfileId = Movement.MovementProfileId;
	Ship.DefaultCargoCapacityMassKg = 1000.0;
	Ship.DefaultCargoCapacityVolumeM3 = 12.0;
	Ship.DefaultDurabilityProfileId = Durability.DurabilityProfileId;
	Ship.DefaultLoadoutProfileId = Loadout.LoadoutProfileId;
	Ship.DefaultResourceProfileId = Resources.ResourceProfileId;
	Ship.DockingSizeClass = TEXT("small");
	ShipArchetypesById.Add(Ship.ShipArchetypeId, Ship);
}

bool UStarCatalogSubsystem::BuildAssetCatalogCache(bool bAllowNativeFallback)
{
	ResetCatalogCache();

	UAssetManager& AssetManager = UAssetManager::Get();
	auto LoadAsset = [&AssetManager](const FPrimaryAssetId& AssetId) -> UObject*
	{
		const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
		return AssetPath.IsValid() ? AssetPath.TryLoad() : nullptr;
	};

	TArray<FPrimaryAssetId> AssetIds;
	AssetManager.GetPrimaryAssetIdList(UStartProfileAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UStartProfileAsset* Asset = Cast<UStartProfileAsset>(LoadAsset(AssetId)))
		{
			StartProfilesById.Add(Asset->Definition.StartProfileId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UStarSystemDefinitionAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UStarSystemDefinitionAsset* Asset = Cast<UStarSystemDefinitionAsset>(LoadAsset(AssetId)))
		{
			SystemsById.Add(Asset->Definition.SystemId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UStarCatalogAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UStarCatalogAsset* Asset = Cast<UStarCatalogAsset>(LoadAsset(AssetId)))
		{
			CatalogEntries.Append(Asset->Systems);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UShipMovementProfileAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UShipMovementProfileAsset* Asset = Cast<UShipMovementProfileAsset>(LoadAsset(AssetId)))
		{
			MovementProfilesById.Add(Asset->Definition.MovementProfileId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UShipDurabilityProfileAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UShipDurabilityProfileAsset* Asset = Cast<UShipDurabilityProfileAsset>(LoadAsset(AssetId)))
		{
			DurabilityProfilesById.Add(Asset->Definition.DurabilityProfileId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UShipLoadoutProfileAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UShipLoadoutProfileAsset* Asset = Cast<UShipLoadoutProfileAsset>(LoadAsset(AssetId)))
		{
			LoadoutProfilesById.Add(Asset->Definition.LoadoutProfileId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UShipResourceProfileAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UShipResourceProfileAsset* Asset = Cast<UShipResourceProfileAsset>(LoadAsset(AssetId)))
		{
			ResourceProfilesById.Add(Asset->Definition.ResourceProfileId, Asset->Definition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UShipArchetypeDataAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UShipArchetypeDataAsset* Asset = Cast<UShipArchetypeDataAsset>(LoadAsset(AssetId)))
		{
			ShipArchetypesById.Add(Asset->Definition.ShipArchetypeId, Asset->Definition);
		}
	}

	bUsingAssetCatalog = StartProfilesById.Num() > 0 && SystemsById.Num() > 0;
	if (!bUsingAssetCatalog && bAllowNativeFallback)
	{
		BuildNativeFixtureCache();
	}

	return bUsingAssetCatalog;
}

bool UStarCatalogSubsystem::ResolveStartProfile(FName StartProfileId, FStartProfileDefinition& OutStartProfile) const
{
	if (const FStartProfileDefinition* Found = StartProfilesById.Find(StartProfileId))
	{
		OutStartProfile = *Found;
		return true;
	}
	return bUsingAssetCatalog ? false : FFrontierTestFixtureProvider::ResolveStartProfile(StartProfileId, OutStartProfile);
}

bool UStarCatalogSubsystem::ResolveSystemDefinition(FName SystemId, FStarSystemDefinition& OutSystemDefinition) const
{
	if (const FStarSystemDefinition* Found = SystemsById.Find(SystemId))
	{
		OutSystemDefinition = *Found;
		return true;
	}
	return bUsingAssetCatalog ? false : FFrontierTestFixtureProvider::ResolveSystemDefinition(SystemId, OutSystemDefinition);
}

bool UStarCatalogSubsystem::ResolveShipArchetype(FName ShipArchetypeId, FShipArchetypeDefinition& OutDefinition) const
{
	if (const FShipArchetypeDefinition* Found = ShipArchetypesById.Find(ShipArchetypeId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM1Fixture(FName SystemId) const
{
	FStargameValidationReport Report;
	Report.Profile = EStargameValidationProfile::M1;
	Report.SystemId = SystemId;

	if (!bUsingAssetCatalog)
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("asset_catalog_not_loaded"), SystemId, TEXT("M1 validation requires Asset Manager-discovered primary data assets."));
		return Report;
	}

	ValidateAssetManagerCoverage(Report);

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	const FPrimaryAssetId ExpectedSystemAssetId(UStarSystemDefinitionAsset::AssetType, SystemId);
	const FStarCatalogEntry* CatalogEntry = CatalogEntries.FindByPredicate([SystemId](const FStarCatalogEntry& Entry)
	{
		return Entry.SystemId == SystemId;
	});
	if (!CatalogEntry)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_catalog_entry"), SystemId, TEXT("System is missing from the authored star catalog."));
	}
	else if (CatalogEntry->SystemDefinitionAsset != ExpectedSystemAssetId || !IsPrimaryAssetReferenceResolved(CatalogEntry->SystemDefinitionAsset, UStarSystemDefinitionAsset::AssetType))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_catalog_system_asset"), SystemId, TEXT("Catalog entry system definition primary asset reference does not resolve."));
	}

	ValidateSystemDefinition(SystemDefinition, Report);

	FStartProfileDefinition StartProfile;
	if (!ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_start_profile"), FFrontierTestFixtureProvider::DefaultStartProfileId, TEXT("Required start profile could not be resolved."));
	}
	else
	{
		ValidateStartProfile(StartProfile, SystemDefinition, Report);
		ValidateShipArchetype(StartProfile, Report);
	}

	return Report;
}

void UStarCatalogSubsystem::GetKnownSystemIds(TArray<FName>& OutSystemIds) const
{
	SystemsById.GenerateKeyArray(OutSystemIds);
	OutSystemIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

FString UStarCatalogSubsystem::GetCatalogDebugSummary() const
{
	TArray<FName> SystemIds;
	GetKnownSystemIds(SystemIds);

	TArray<FString> SystemLines;
	for (const FName SystemId : SystemIds)
	{
		SystemLines.Add(SystemId.ToString());
	}

	return FString::Printf(TEXT("CatalogSystems=%s\nStartProfiles=%d\nShipArchetypes=%d"),
		*FString::Join(SystemLines, TEXT(",")),
		StartProfilesById.Num(),
		ShipArchetypesById.Num());
}

void UStarCatalogSubsystem::AddIssue(FStargameValidationReport& Report, EStargameValidationSeverity Severity, FName RuleId, FName ObjectId, const FString& Message) const
{
	FStargameValidationIssue Issue;
	Issue.Severity = Severity;
	Issue.RuleId = RuleId;
	Issue.ObjectId = ObjectId;
	Issue.Message = Message;
	Report.Issues.Add(Issue);
}

bool UStarCatalogSubsystem::ValidateAssetManagerCoverage(FStargameValidationReport& Report) const
{
	UAssetManager& AssetManager = UAssetManager::Get();
	bool bValid = true;

	for (const FM1RequiredAssetType& RequiredType : GetM1RequiredAssetTypes())
	{
		if (!AssetManagerSettingsScanRoot(RequiredType.AssetType, RequiredType.RequiredRoot))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("asset_manager_scan_root_missing"), FName(*RequiredType.AssetType.ToString()), FString::Printf(TEXT("Asset Manager settings do not scan '%s' under '%s'."), *RequiredType.AssetType.ToString(), *RequiredType.RequiredRoot));
			bValid = false;
		}

		TArray<FPrimaryAssetId> AssetIds;
		AssetManager.GetPrimaryAssetIdList(RequiredType.AssetType, AssetIds);
		if (AssetIds.IsEmpty())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("asset_manager_type_empty"), FName(*RequiredType.AssetType.ToString()), FString::Printf(TEXT("Asset Manager did not discover required primary asset type '%s'."), *RequiredType.AssetType.ToString()));
			bValid = false;
			continue;
		}

		TSet<FName> AssetNames;
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			if (AssetId.PrimaryAssetType != RequiredType.AssetType || AssetId.PrimaryAssetName.IsNone())
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_primary_asset_id"), FName(*AssetId.ToString()), TEXT("Discovered primary asset has an invalid or unstable ID."));
				bValid = false;
			}
			if (AssetNames.Contains(AssetId.PrimaryAssetName))
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("duplicate_primary_asset_id"), FName(*AssetId.ToString()), TEXT("Duplicate primary asset ID discovered."));
				bValid = false;
			}
			AssetNames.Add(AssetId.PrimaryAssetName);

			const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
			const FString PackageName = AssetPath.GetLongPackageName();
			const bool bInRequiredRoot = PackageName == RequiredType.RequiredRoot || PackageName.StartsWith(RequiredType.RequiredRoot + TEXT("/"));
			if (!AssetPath.IsValid() || !bInRequiredRoot)
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("primary_asset_path_outside_root"), FName(*AssetId.ToString()), FString::Printf(TEXT("Primary asset '%s' is outside required root '%s'."), *AssetId.ToString(), *RequiredType.RequiredRoot));
				bValid = false;
			}
			const bool bLoadable = AssetPath.IsValid() && (AssetPath.ResolveObject() || AssetPath.TryLoad());
			if (!bLoadable)
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("primary_asset_not_loadable"), FName(*AssetId.ToString()), TEXT("Primary asset was discovered by Asset Manager but could not be loaded."));
				bValid = false;
			}
		}
	}

	return bValid;
}

bool UStarCatalogSubsystem::ValidateStartProfile(const FStartProfileDefinition& StartProfile, const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;

	if (StartProfile.StartProfileId != FFrontierTestFixtureProvider::DefaultStartProfileId)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_start_profile_id"), StartProfile.StartProfileId, TEXT("M1 fixture must validate frontier_test_start."));
		bValid = false;
	}
	if (StartProfile.SystemId != SystemDefinition.SystemId)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("start_profile_system_mismatch"), StartProfile.StartProfileId, TEXT("Start profile system ID does not match the validated system."));
		bValid = false;
	}
	if (StartProfile.SpawnZoneId != FFrontierTestFixtureProvider::DeepSpaceSpawnZoneId)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_start_spawn_zone"), StartProfile.SpawnZoneId, TEXT("M1 fixture must start at spawn_deep_space."));
		bValid = false;
	}

	const bool bSpawnZoneExists = SystemDefinition.SpawnZones.ContainsByPredicate([&StartProfile](const FSpawnZoneDefinition& SpawnZone)
	{
		return SpawnZone.SpawnZoneId == StartProfile.SpawnZoneId;
	});
	if (!bSpawnZoneExists)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("start_spawn_zone_missing"), StartProfile.SpawnZoneId, TEXT("Start profile spawn zone does not exist in the system definition."));
		bValid = false;
	}

	if (!StartProfile.DockedStationId.IsNone())
	{
		const FStationDefinition* DockedStation = SystemDefinition.Stations.FindByPredicate([&StartProfile](const FStationDefinition& Station)
		{
			return Station.StationId == StartProfile.DockedStationId;
		});
		if (!DockedStation)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("start_docked_station_missing"), StartProfile.DockedStationId, TEXT("Start profile docked station does not exist in the system definition."));
			bValid = false;
		}
		else if (!StartProfile.DockingPortId.IsNone() && !DockedStation->DockingPorts.ContainsByPredicate([&StartProfile](const FDockingPortDefinition& Port)
		{
			return Port.PortId == StartProfile.DockingPortId;
		}))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("start_docking_port_missing"), StartProfile.DockingPortId, TEXT("Start profile docking port does not exist on the docked station."));
			bValid = false;
		}
	}

	return bValid;
}

bool UStarCatalogSubsystem::ValidateSystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	if (SystemDefinition.SystemId.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("empty_system_id"), NAME_None, TEXT("System ID is empty."));
		return false;
	}

	TSet<FName> EntityIds;
	auto AddUniqueId = [this, &EntityIds, &Report](FName Id, FName RuleId, const TCHAR* Label)
	{
		if (Id.IsNone())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, Id, FString::Printf(TEXT("%s ID is empty."), Label));
			return false;
		}
		if (EntityIds.Contains(Id))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("duplicate_entity_id"), Id, FString::Printf(TEXT("Duplicate entity ID '%s'."), *Id.ToString()));
			return false;
		}
		EntityIds.Add(Id);
		return true;
	};

	auto EntityExists = [&EntityIds](FName Id)
	{
		return !Id.IsNone() && EntityIds.Contains(Id);
	};

	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		AddUniqueId(Body.BodyId, TEXT("invalid_body_id"), TEXT("Body"));
		if (!IsPrimaryAssetReferenceResolved(Body.VisualProfileId, UBodyVisualProfileAsset::AssetType))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_body_visual_profile"), Body.BodyId, TEXT("M1 body visual profile primary asset reference does not resolve."));
		}
		if (Body.AtmosphereProfileId.IsValid() && !IsPrimaryAssetReferenceResolved(Body.AtmosphereProfileId, UAtmosphereProfileAsset::AssetType))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_body_atmosphere_profile"), Body.BodyId, TEXT("M1 body atmosphere profile primary asset reference does not resolve."));
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		AddUniqueId(Station.StationId, TEXT("invalid_station_id"), TEXT("Station"));
		if (!Station.AnchorId.IsNone() && !EntityExists(Station.AnchorId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_station_anchor"), Station.StationId, TEXT("Station anchor ID does not resolve to a known system entity."));
		}
		if (!IsPrimaryAssetReferenceResolved(Station.StationProfileId, UStationProfileAsset::AssetType))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_station_profile"), Station.StationId, TEXT("M1 station profile primary asset reference does not resolve."));
		}
		if (Station.DockingPorts.IsEmpty())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_station_docking_ports"), Station.StationId, TEXT("M1 station definitions must author docking ports for the docking port registry."));
		}

		TSet<FName> DockingPortIds;
		for (const FDockingPortDefinition& DockingPort : Station.DockingPorts)
		{
			if (DockingPort.PortId.IsNone() || DockingPortIds.Contains(DockingPort.PortId))
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_docking_port"), Station.StationId, TEXT("Docking port IDs must be non-empty and unique per station."));
			}
			if (DockingPort.AllowedShipClasses.IsEmpty())
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_docking_port_ship_classes"), DockingPort.PortId, TEXT("Docking ports must declare allowed ship classes."));
			}
			DockingPortIds.Add(DockingPort.PortId);
		}
	}

	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		AddUniqueId(Gate.GateId, TEXT("invalid_gate_id"), TEXT("Gate"));
		if (!Gate.AnchorId.IsNone() && !EntityExists(Gate.AnchorId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_gate_anchor"), Gate.GateId, TEXT("Gate anchor ID does not resolve to a known system entity."));
		}
		if (!IsPrimaryAssetReferenceResolved(Gate.GateProfileId, UGateProfileAsset::AssetType))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_gate_profile"), Gate.GateId, TEXT("M1 gate profile primary asset reference does not resolve."));
		}
		if (Gate.ActivationRangeCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_gate_activation_range"), Gate.GateId, TEXT("Gate activation range must be positive."));
		}
	}

	TSet<FName> SpawnZoneIds;
	for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
	{
		if (SpawnZone.SpawnZoneId.IsNone() || SpawnZoneIds.Contains(SpawnZone.SpawnZoneId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_spawn_zone"), SpawnZone.SpawnZoneId, TEXT("Spawn zone IDs must be non-empty and unique."));
		}
		if (!SpawnZone.AnchorId.IsNone() && !EntityExists(SpawnZone.AnchorId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_spawn_zone_anchor"), SpawnZone.SpawnZoneId, TEXT("Spawn zone anchor ID does not resolve to a known system entity."));
		}
		if (SpawnZone.RadiusCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_spawn_zone_radius"), SpawnZone.SpawnZoneId, TEXT("Spawn zone radius must be positive."));
		}
		SpawnZoneIds.Add(SpawnZone.SpawnZoneId);
	}

	TSet<FName> GravityWellIds;
	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (GravityWell.WellId.IsNone() || GravityWellIds.Contains(GravityWell.WellId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_gravity_well"), GravityWell.WellId, TEXT("Gravity well IDs must be non-empty and unique."));
		}
		if (!EntityExists(GravityWell.AnchorBodyId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_gravity_well_anchor"), GravityWell.WellId, TEXT("Gravity well anchor body ID does not resolve to a known body/entity."));
		}
		if (GravityWell.SlowdownRadiusCm <= 0.0 || GravityWell.LockoutRadiusCm <= 0.0 || GravityWell.DropoutRadiusCm <= 0.0 || GravityWell.Strength <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_gravity_well_values"), GravityWell.WellId, TEXT("Gravity well radii and strength must be positive."));
		}
		GravityWellIds.Add(GravityWell.WellId);
	}

	TSet<FName> NavigationTargetIds;
	auto AddNavigationTarget = [this, &NavigationTargetIds, &Report, &EntityIds](const FNavigationTargetDefinition& Target, FName OwnerId)
	{
		if (!Target.bCanTarget)
		{
			return;
		}
		if (Target.TargetId.IsNone() || NavigationTargetIds.Contains(Target.TargetId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_navigation_target"), OwnerId, TEXT("Navigation target IDs must be non-empty and unique."));
		}
		if (Target.TargetId != OwnerId)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("navigation_target_source_mismatch"), OwnerId, TEXT("M1 fixture navigation target ID must match the owning entity ID."));
		}
		if (!EntityIds.Contains(Target.TargetId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_navigation_target"), OwnerId, TEXT("Navigation target ID does not resolve to a system entity."));
		}
		NavigationTargetIds.Add(Target.TargetId);
	};

	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		AddNavigationTarget(Body.NavigationTarget, Body.BodyId);
	}
	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		AddNavigationTarget(Station.NavigationTarget, Station.StationId);
	}
	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		AddNavigationTarget(Gate.NavigationTarget, Gate.GateId);
	}

	TSet<FName> MapEntryIds;
	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (MapEntry.MapEntryId.IsNone() || MapEntryIds.Contains(MapEntry.MapEntryId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_map_entry"), MapEntry.MapEntryId, TEXT("Map entry IDs must be non-empty and unique."));
		}
		if (!EntityExists(MapEntry.SourceId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_map_entry_source"), MapEntry.MapEntryId, TEXT("Map entry source ID does not resolve to a system entity."));
		}
		MapEntryIds.Add(MapEntry.MapEntryId);
	}

	return !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateShipArchetype(const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const
{
	const FShipArchetypeDefinition* Ship = ShipArchetypesById.Find(StartProfile.ShipArchetypeId);
	if (!Ship)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_ship_archetype"), StartProfile.ShipArchetypeId, TEXT("Start profile ship archetype does not resolve."));
		return false;
	}

	if (Ship->PawnClass.IsNull())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_ship_pawn_class"), Ship->ShipArchetypeId, TEXT("Ship archetype is missing a pawn class."));
	}
	else if (!Ship->PawnClass.LoadSynchronous())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_ship_pawn_class"), Ship->ShipArchetypeId, TEXT("Ship pawn class reference does not resolve."));
	}
	if (Ship->DefaultCargoCapacityMassKg <= 0.0 || Ship->DefaultCargoCapacityVolumeM3 <= 0.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_ship_cargo_capacity"), Ship->ShipArchetypeId, TEXT("Ship cargo capacities must be positive."));
	}
	if (Ship->ShipClassTags.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_ship_class_tags"), Ship->ShipArchetypeId, TEXT("Ship archetype is missing ship class tags."));
	}
	if (Ship->DockingSizeClass.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_docking_size"), Ship->ShipArchetypeId, TEXT("Ship archetype is missing a docking size class."));
	}

	const FShipMovementProfileDefinition* Movement = MovementProfilesById.Find(Ship->MovementProfileId);
	if (!Movement || Movement->MaxLocalSpeedCmPerSec <= 0.0 || Movement->MaxAngularSpeedDegPerSec <= 0.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_movement_profile"), Ship->MovementProfileId, TEXT("Ship movement profile is missing or has invalid positive values."));
	}
	else if (Movement->SupportedFlightModeTags.IsEmpty() || Movement->NormalFlightTuningId.IsNone() || Movement->SupercruiseTuningId.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("incomplete_movement_profile"), Ship->MovementProfileId, TEXT("Ship movement profile is missing flight mode tags or tuning IDs."));
	}

	const FShipDurabilityProfileDefinition* Durability = DurabilityProfilesById.Find(Ship->DefaultDurabilityProfileId);
	if (!Durability || Durability->MaxHull <= 0.0 || Durability->MaxShield < 0.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_durability_profile"), Ship->DefaultDurabilityProfileId, TEXT("Ship durability profile is missing or invalid."));
	}
	else if (Durability->DisabledHullFraction <= Durability->DestroyedHullFraction || Durability->DisabledHullFraction > 1.0 || Durability->DestroyedHullFraction < 0.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_durability_thresholds"), Ship->DefaultDurabilityProfileId, TEXT("Ship durability profile thresholds are not canonical."));
	}

	const FShipLoadoutProfileDefinition* Loadout = LoadoutProfilesById.Find(Ship->DefaultLoadoutProfileId);
	if (!Loadout)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_loadout_profile"), Ship->DefaultLoadoutProfileId, TEXT("Ship loadout profile is missing."));
	}
	else if (Loadout->RequiredResourceProfileId != Ship->DefaultResourceProfileId)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("loadout_resource_profile_mismatch"), Loadout->LoadoutProfileId, TEXT("Ship loadout required resource profile does not match the archetype default resource profile."));
	}

	const FShipResourceProfileDefinition* Resources = ResourceProfilesById.Find(Ship->DefaultResourceProfileId);
	if (!Resources || Resources->ResourceCapacities.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_resource_profile"), Ship->DefaultResourceProfileId, TEXT("Ship resource profile is missing or empty."));
	}
	else if (Resources->DefaultFillPolicy.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_resource_fill_policy"), Resources->ResourceProfileId, TEXT("Ship resource profile is missing default fill policy."));
	}
	else
	{
		for (const FShipResourceCapacityDefinition& Capacity : Resources->ResourceCapacities)
		{
			if (Capacity.ResourceId.IsNone() || Capacity.Capacity <= 0.0)
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_resource_capacity"), Resources->ResourceProfileId, TEXT("Resource capacities must have IDs and positive capacity."));
			}
		}
	}

	return !Report.HasBlockingIssues();
}
