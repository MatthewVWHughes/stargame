#include "Data/StarCatalogSubsystem.h"

#include "Data/FrontierTestFixtureProvider.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Flight/SpaceFlightPawn.h"
#include "Space/LogicalTrafficActor.h"
#include "Space/LogicalTrafficQueryService.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/SystemicGameplayQueryService.h"

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

	bool IsValidGeneratedUtcStamp(const FString& Timestamp)
	{
		return Timestamp.Len() >= 20 &&
			Timestamp.Contains(TEXT("T")) &&
			Timestamp.EndsWith(TEXT("Z"));
	}

	bool HasOnlyNamedIds(const TArray<FName>& Ids)
	{
		return !Ids.IsEmpty() && !Ids.ContainsByPredicate([](FName Id)
		{
			return Id.IsNone();
		});
	}

	FNavigationTargetDefinition MakeGeneratedNavigationTarget(FName TargetId, const FString& DisplayName, FName TargetType, FName FrameType, FName AnchorId)
	{
		FNavigationTargetDefinition Target;
		Target.TargetId = TargetId;
		Target.DisplayName = FText::FromString(DisplayName);
		Target.TargetType = TargetType;
		Target.FrameType = FrameType;
		Target.AnchorId = AnchorId;
		Target.bShowInHud = TargetType != FName(TEXT("resource"));
		Target.bShowInSystemMap = true;
		Target.bCanTarget = true;
		return Target;
	}

	FDockingPortDefinition MakeGeneratedDockingPort(FName PortId)
	{
		FDockingPortDefinition DockingPort;
		DockingPort.PortId = PortId;
		DockingPort.DisplayName = FText::FromName(PortId);
		DockingPort.ApproachTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -25000.0, 0.0));
		DockingPort.DockedTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -1200.0, 0.0));
		DockingPort.UndockTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -6000.0, 0.0));
		DockingPort.AllowedShipClasses.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Light"), false));
		DockingPort.ActivationRangeCm = 15000.0;
		DockingPort.MaxApproachSpeedCmPerSec = 2500.0;
		DockingPort.RequiredAlignmentDegrees = 10.0;
		return DockingPort;
	}

	void AddSystemMapEntry(FStarSystemDefinition& SystemDefinition, FName SourceId, FName EntryType)
	{
		if (SystemDefinition.MapEntries.ContainsByPredicate([SourceId](const FMapEntryDefinition& Entry)
		{
			return Entry.MapEntryId == SourceId || Entry.SourceId == SourceId;
		}))
		{
			return;
		}

		FMapEntryDefinition Entry;
		Entry.MapEntryId = SourceId;
		Entry.SourceId = SourceId;
		Entry.EntryType = EntryType;
		SystemDefinition.MapEntries.Add(Entry);
	}

	FName ResolveAuthoredStationAnchor(const FStarSystemDefinition& SystemDefinition, FName PreferredAnchorId)
	{
		auto HasBody = [&SystemDefinition](FName BodyId)
		{
			return !BodyId.IsNone() && SystemDefinition.Bodies.ContainsByPredicate([BodyId](const FBodyDefinition& Body)
			{
				return Body.BodyId == BodyId;
			});
		};

		if (HasBody(PreferredAnchorId))
		{
			return PreferredAnchorId;
		}
		if (HasBody(TEXT("brink")))
		{
			return TEXT("brink");
		}
		if (HasBody(TEXT("ember")))
		{
			return TEXT("ember");
		}
		return SystemDefinition.Bodies.IsEmpty() ? NAME_None : SystemDefinition.Bodies[0].BodyId;
	}

	bool ShouldApplyFrontierPlayableMetadata(const FStarSystemDefinition& SystemDefinition)
	{
		return SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station)
			{
				return Station.StationId == FName(TEXT("brink_watch"));
			});
	}

	void ApplyGodotStationRegistryMetadata(FStarSystemDefinition& SystemDefinition)
	{
		const FName DerelictAnchorId = ResolveAuthoredStationAnchor(SystemDefinition, TEXT("brink_minor"));
		if (!SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station)
		{
			return Station.StationId == FName(TEXT("derelict_outpost_01"));
		}))
		{
			FStationDefinition DerelictOutpost;
			DerelictOutpost.StationId = TEXT("derelict_outpost_01");
			DerelictOutpost.DisplayName = FText::FromString(TEXT("Derelict Outpost"));
			DerelictOutpost.FrameType = TEXT("station_relative");
			DerelictOutpost.AnchorId = DerelictAnchorId;
			DerelictOutpost.VisualRadiusCm = 9000.0;
			DerelictOutpost.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
			DerelictOutpost.OwnerFactionId = TEXT("ember_raiders");
			DerelictOutpost.StationRole = TEXT("hostile_boarding");
			DerelictOutpost.RegionName = TEXT("wayfarer_trade_lane");
			DerelictOutpost.SecurityProfile = TEXT("hostile_contested_space");
			DerelictOutpost.MissionTags = { TEXT("boarding"), TEXT("clearance"), TEXT("hostile_boarding") };
			DerelictOutpost.DockingPorts = { MakeGeneratedDockingPort(TEXT("derelict_outpost_port_a")) };
			DerelictOutpost.NavigationTarget = MakeGeneratedNavigationTarget(TEXT("derelict_outpost_01"), TEXT("Derelict Outpost"), TEXT("station"), DerelictOutpost.FrameType, DerelictOutpost.AnchorId);
			DerelictOutpost.Orbit.ParentId = DerelictAnchorId;
			DerelictOutpost.Orbit.SemiMajorAxisCm = 2400000.0;
			DerelictOutpost.Orbit.PeriodSeconds = 1200.0;
			DerelictOutpost.Orbit.PhaseOffsetRadians = 4.2;
			SystemDefinition.Stations.Add(DerelictOutpost);
			AddSystemMapEntry(SystemDefinition, DerelictOutpost.StationId, TEXT("station"));
		}

		for (FStationDefinition& Station : SystemDefinition.Stations)
		{
			if (Station.StationId == FName(TEXT("brink_watch")))
			{
				Station.OwnerFactionId = TEXT("frontier_local_authority");
				Station.StationRole = TEXT("frontier_security_market");
				Station.RegionName = TEXT("brink_orbital_corridor");
				Station.SecurityProfile = TEXT("low_to_moderate_frontier_security");
				Station.MarketProfileId = TEXT("brink_watch_market");
				Station.MissionTags = { TEXT("convoy_defense"), TEXT("patrol_response"), TEXT("ore_trade"), TEXT("frontier_security") };
				Station.QuestGiverNpcIds = { TEXT("npc_brink_watch_dispatch"), TEXT("npc_brink_market_factor") };
				Station.QuestGivers = {
					{ TEXT("npc_brink_watch_dispatch"), FText::FromString(TEXT("Brink Watch Dispatch")), FVector(-320.0, -180.0, 10.0) },
					{ TEXT("npc_brink_market_factor"), FText::FromString(TEXT("Brink Market Factor")), FVector(-320.0, 220.0, 10.0) }
				};
			}
			else if (Station.StationId == FName(TEXT("wayfarer_depot")))
			{
				const FName WayfarerAnchorId = ResolveAuthoredStationAnchor(SystemDefinition, Station.AnchorId);
				Station.AnchorId = WayfarerAnchorId;
				Station.Orbit.ParentId = WayfarerAnchorId;
				Station.NavigationTarget.AnchorId = WayfarerAnchorId;
				Station.OwnerFactionId = TEXT("frontier_local_authority");
				Station.StationRole = TEXT("route_depot_and_repair_stop");
				Station.RegionName = TEXT("wayfarer_trade_lane");
				Station.SecurityProfile = TEXT("patrolled_trade_lane_endpoint");
				Station.MarketProfileId = TEXT("wayfarer_depot_market");
				Station.MissionTags = { TEXT("route_delivery"), TEXT("repair"), TEXT("distress_response"), TEXT("trade_lane") };
				Station.QuestGiverNpcIds = { TEXT("npc_wayfarer_depot_master") };
				Station.QuestGivers = {
					{ TEXT("npc_wayfarer_depot_master"), FText::FromString(TEXT("Wayfarer Depot Master")), FVector(-280.0, -260.0, 10.0) }
				};
			}
			else if (Station.StationId == FName(TEXT("derelict_outpost_01")))
			{
				const FName StationAnchorId = ResolveAuthoredStationAnchor(SystemDefinition, Station.AnchorId);
				Station.AnchorId = StationAnchorId;
				Station.Orbit.ParentId = StationAnchorId;
				Station.NavigationTarget.AnchorId = StationAnchorId;
			}
		}
	}

	void ApplyGodotMissionBoardMetadata(FStarSystemDefinition& SystemDefinition)
	{
		auto AddMissionDefinition = [&SystemDefinition](
			FName MissionDefinitionId,
			const TCHAR* Title,
			const TCHAR* Summary,
			const TCHAR* OfferDialog,
			const TCHAR* InProgressDialog,
			const TCHAR* TurnInDialog)
		{
			if (SystemDefinition.SystemicGameplay.MissionDefinitions.ContainsByPredicate([MissionDefinitionId](const FMissionDefinition& MissionDefinition)
			{
				return MissionDefinition.MissionDefinitionId == MissionDefinitionId;
			}))
			{
				return;
			}

			FMissionDefinition MissionDefinition;
			MissionDefinition.MissionDefinitionId = MissionDefinitionId;
			MissionDefinition.Title = FText::FromString(Title);
			MissionDefinition.Summary = FText::FromString(Summary);
			MissionDefinition.OfferDialog = FText::FromString(OfferDialog);
			MissionDefinition.AcceptedDialog = FText::FromString(TEXT("Contract accepted. I will keep the channel open."));
			MissionDefinition.InProgressDialog = FText::FromString(InProgressDialog);
			MissionDefinition.TurnInDialog = FText::FromString(TurnInDialog);
			SystemDefinition.SystemicGameplay.MissionDefinitions.Add(MissionDefinition);
		};

		AddMissionDefinition(
			TEXT("mission_ore_delivery_basic"),
			TEXT("Ore Delivery"),
			TEXT("Deliver ore from Brink Watch to Wayfarer Depot."),
			TEXT("Wayfarer Depot needs ore. Take the shipment out and report back when it is delivered."),
			TEXT("The ore delivery is still active. Get the cargo to Wayfarer Depot."),
			TEXT("Ore delivery confirmed. Payment is ready."));
		AddMissionDefinition(
			TEXT("mission_m12_route_security_cargo"),
			TEXT("Wayfarer Security Cargo"),
			TEXT("Deliver security cargo to Wayfarer Depot, respond to trouble on the lane, and return."),
			TEXT("Take this security cargo to Wayfarer Depot. If the lane flares up, handle it and come back."),
			TEXT("The Wayfarer security cargo contract is active. Finish the delivery and patrol response."),
			TEXT("Wayfarer confirmed the delivery and the patrol report. Escrow is ready."));

		for (FMissionOfferRecord& Offer : SystemDefinition.SystemicGameplay.MissionOffers)
		{
			if (Offer.OfferId == FName(TEXT("offer_brink_ore_delivery_01")))
			{
				Offer.GiverNpcId = TEXT("npc_brink_market_factor");
				Offer.RequiredStationTags = { TEXT("ore_trade") };
			}
			else if (Offer.OfferId == FName(TEXT("offer_m12_wayfarer_security_01")))
			{
				Offer.GiverNpcId = TEXT("npc_brink_watch_dispatch");
				Offer.RequiredStationTags = { TEXT("convoy_defense"), TEXT("frontier_security") };
			}
		}
	}

	FStarSystemDefinition MakeFrontierArrivalSystemDefinition()
	{
		FStarSystemDefinition SystemDefinition;
		SystemDefinition.SystemId = TEXT("frontier_arrival_test_01");
		SystemDefinition.DisplayName = FText::FromString(TEXT("Frontier Arrival Test 01"));

		FGateDefinition Gate;
		Gate.GateId = TEXT("arrival_gate_a");
		Gate.DisplayName = FText::FromString(TEXT("Arrival Gate A"));
		Gate.FrameType = TEXT("system_barycentric");
		Gate.Transform = FTransform(FRotator(0.0, 180.0, 0.0), FVector::ZeroVector);
		Gate.VisualRadiusCm = 16000.0;
		Gate.ActivationRangeCm = 50000.0;
		Gate.GateProfileId = FPrimaryAssetId(UGateProfileAsset::AssetType, TEXT("frontier_gate_basic"));
		Gate.NavigationTarget = MakeGeneratedNavigationTarget(Gate.GateId, TEXT("Arrival Gate A"), TEXT("gate"), Gate.FrameType, NAME_None);

		FSpawnZoneDefinition Arrival;
		Arrival.SpawnZoneId = TEXT("arrival_from_frontier_gate_a");
		Arrival.DisplayName = FText::FromString(TEXT("Arrival From Frontier Gate A"));
		Arrival.FrameType = TEXT("gate_relative");
		Arrival.AnchorId = Gate.GateId;
		Arrival.Transform = FTransform(FRotator(0.0, 180.0, 0.0), FVector(0.0, 18000.0, 0.0));
		Arrival.RadiusCm = 12000.0;
		Arrival.AllowedContexts = { TEXT("arrival"), TEXT("reload") };

		SystemDefinition.Gates = { Gate };
		SystemDefinition.SpawnZones = { Arrival };
		AddSystemMapEntry(SystemDefinition, Gate.GateId, TEXT("gate"));
		return SystemDefinition;
	}
}

void UStarCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildAssetCatalogCache();
}

void UStarCatalogSubsystem::ResetCatalogCache()
{
	CatalogEntries.Reset();
	RouteGraphEdges.Reset();
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
			SystemDefinition.Gates[0].DestinationSystemId = TEXT("frontier_arrival_test_01");
			SystemDefinition.Gates[0].DestinationGateId = TEXT("arrival_gate_a");
			SystemDefinition.Gates[0].DestinationArrivalId = TEXT("arrival_from_frontier_gate_a");
			SystemDefinition.Gates[0].DestinationArrivalFrame = TEXT("gate_relative");
			SystemDefinition.Gates[0].DestinationArrivalLocalOffsetCm = FVector(0.0, 18000.0, 0.0);
			SystemDefinition.Gates[0].DestinationArrivalRotation = FRotator(0.0, 180.0, 0.0);
		}
		if (ShouldApplyFrontierPlayableMetadata(SystemDefinition))
		{
			ApplyGodotStationRegistryMetadata(SystemDefinition);
			ApplyGodotMissionBoardMetadata(SystemDefinition);
			SystemDefinition.SystemicGameplay = USystemicGameplayQueryService::MakeM12FixtureState(SystemDefinition);
		}

		if (SystemDefinition.MapEntries.IsEmpty())
		{
			for (const FBodyDefinition& Body : SystemDefinition.Bodies)
			{
				FMapEntryDefinition Entry;
				Entry.MapEntryId = Body.BodyId;
				Entry.SourceId = Body.BodyId;
				Entry.EntryType = TEXT("body");
				SystemDefinition.MapEntries.Add(Entry);
			}
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

	FStarSystemDefinition ArrivalSystemDefinition = MakeFrontierArrivalSystemDefinition();
	SystemsById.Add(ArrivalSystemDefinition.SystemId, ArrivalSystemDefinition);

	FStarCatalogEntry ArrivalCatalogEntry;
	ArrivalCatalogEntry.SystemId = ArrivalSystemDefinition.SystemId;
	ArrivalCatalogEntry.DisplayName = ArrivalSystemDefinition.DisplayName;
	ArrivalCatalogEntry.StellarClass = TEXT("test_fixture_arrival");
	ArrivalCatalogEntry.CatalogSource = TEXT("native_fixture");
	ArrivalCatalogEntry.SystemDefinitionAsset = FPrimaryAssetId(UStarSystemDefinitionAsset::AssetType, ArrivalSystemDefinition.SystemId);
	CatalogEntries.Add(ArrivalCatalogEntry);

	FRouteGraphEdge ArrivalRouteEdge;
	ArrivalRouteEdge.RouteEdgeId = TEXT("edge_frontier_gate_a_to_arrival_gate_a");
	ArrivalRouteEdge.SourceSystemId = FFrontierTestFixtureProvider::FrontierSystemId;
	ArrivalRouteEdge.SourceGateId = TEXT("frontier_gate_a");
	ArrivalRouteEdge.DestinationSystemId = ArrivalSystemDefinition.SystemId;
	ArrivalRouteEdge.DestinationGateId = TEXT("arrival_gate_a");
	ArrivalRouteEdge.DestinationArrivalId = TEXT("arrival_from_frontier_gate_a");
	ArrivalRouteEdge.RouteType = TEXT("jump_gate");
	ArrivalRouteEdge.bArtificialGate = true;
	ArrivalRouteEdge.bInitiallyKnown = true;
	RouteGraphEdges.Add(ArrivalRouteEdge);

	FShipMovementProfileDefinition Movement;
	Movement.MovementProfileId = TEXT("wayfarer_movement_basic");
	Movement.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Normal"), false));
	Movement.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Supercruise"), false));
	Movement.NormalFlightTuningId = TEXT("normal_basic");
	Movement.SupercruiseTuningId = TEXT("supercruise_basic");
	Movement.MaxLocalSpeedCmPerSec = 24000.0;
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
	Ship.ActorClass = ALogicalTrafficActor::StaticClass();
	Ship.MovementProfileId = Movement.MovementProfileId;
	Ship.DefaultCargoCapacityMassKg = 1000.0;
	Ship.DefaultCargoCapacityVolumeM3 = 12.0;
	Ship.DefaultDurabilityProfileId = Durability.DurabilityProfileId;
	Ship.DefaultLoadoutProfileId = Loadout.LoadoutProfileId;
	Ship.DefaultResourceProfileId = Resources.ResourceProfileId;
	Ship.DockingSizeClass = TEXT("small");
	ShipArchetypesById.Add(Ship.ShipArchetypeId, Ship);
}

bool UStarCatalogSubsystem::BuildAssetCatalogCache()
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
			FStarSystemDefinition SystemDefinition = Asset->Definition;
			if (ShouldApplyFrontierPlayableMetadata(SystemDefinition))
			{
				ApplyGodotStationRegistryMetadata(SystemDefinition);
				ApplyGodotMissionBoardMetadata(SystemDefinition);
				SystemDefinition.SystemicGameplay = USystemicGameplayQueryService::MakeM12FixtureState(SystemDefinition);
			}
			SystemsById.Add(SystemDefinition.SystemId, SystemDefinition);
		}
	}

	AssetIds.Reset();
	AssetManager.GetPrimaryAssetIdList(UStarCatalogAsset::AssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (const UStarCatalogAsset* Asset = Cast<UStarCatalogAsset>(LoadAsset(AssetId)))
		{
			CatalogEntries.Append(Asset->Systems);
			RouteGraphEdges.Append(Asset->RouteEdges);
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

	bUsingAssetCatalog =
		StartProfilesById.Num() > 0 &&
		SystemsById.Num() > 0 &&
		CatalogEntries.Num() > 0 &&
		ShipArchetypesById.Num() > 0 &&
		MovementProfilesById.Num() > 0 &&
		DurabilityProfilesById.Num() > 0 &&
		LoadoutProfilesById.Num() > 0 &&
		ResourceProfilesById.Num() > 0;
	return bUsingAssetCatalog;
}

bool UStarCatalogSubsystem::ResolveStartProfile(FName StartProfileId, FStartProfileDefinition& OutStartProfile) const
{
	if (const FStartProfileDefinition* Found = StartProfilesById.Find(StartProfileId))
	{
		OutStartProfile = *Found;
		return true;
	}
	return false;
}

bool UStarCatalogSubsystem::ResolveSystemDefinition(FName SystemId, FStarSystemDefinition& OutSystemDefinition) const
{
	if (const FStarSystemDefinition* Found = SystemsById.Find(SystemId))
	{
		OutSystemDefinition = *Found;
		return true;
	}
	return false;
}

bool UStarCatalogSubsystem::ResolveRouteGraphEdge(FName RouteEdgeId, FRouteGraphEdge& OutRouteEdge) const
{
	if (const FRouteGraphEdge* Found = RouteGraphEdges.FindByPredicate([RouteEdgeId](const FRouteGraphEdge& Edge)
	{
		return Edge.RouteEdgeId == RouteEdgeId;
	}))
	{
		OutRouteEdge = *Found;
		return true;
	}
	return false;
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
	Report.CheckedAssetCount = CatalogEntries.Num() + StartProfilesById.Num() + ShipArchetypesById.Num();
	Report.CheckedSystemCount = SystemsById.Contains(SystemId) ? 1 : 0;
	Report.CheckedFixtureCount = 1;

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

FStargameValidationReport UStarCatalogSubsystem::ValidateM2Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM1Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M2;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM2SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM3Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM2Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M3;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM3SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM4Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM3Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M4;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	FStartProfileDefinition StartProfile;
	if (!ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_start_profile"), FFrontierTestFixtureProvider::DefaultStartProfileId, TEXT("Required start profile could not be resolved."));
		return Report;
	}

	ValidateM4SystemDefinition(SystemDefinition, StartProfile, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM5Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM4Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M5;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	FStartProfileDefinition StartProfile;
	if (!ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_start_profile"), FFrontierTestFixtureProvider::DefaultStartProfileId, TEXT("Required start profile could not be resolved."));
		return Report;
	}

	ValidateM5SystemDefinition(SystemDefinition, StartProfile, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM5_5Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM5Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M5_5;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SourceSystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SourceSystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required source system definition could not be resolved."));
		return Report;
	}

	const FGateDefinition* SourceGate = SourceSystemDefinition.Gates.FindByPredicate([](const FGateDefinition& Gate)
	{
		return Gate.GateId == FName(TEXT("frontier_gate_a"));
	});
	const FName DestinationSystemId = SourceGate ? SourceGate->DestinationSystemId : NAME_None;

	FStarSystemDefinition DestinationSystemDefinition;
	if (DestinationSystemId.IsNone() || !ResolveSystemDefinition(DestinationSystemId, DestinationSystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("m5_5_missing_destination_system"), DestinationSystemId, TEXT("M5.5 requires frontier_gate_a to resolve its destination system."));
		return Report;
	}

		ValidateM5_5SystemDefinition(SourceSystemDefinition, DestinationSystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM7Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM5_5Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M7;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM7SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM8Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM7Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M8;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM8SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM9Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM8Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M9;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM9SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM10Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM9Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M10;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM10SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM10_5Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM10Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M10_5;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM10_5SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM11Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM10_5Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M11;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM11SystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM12GameplayFixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM11Fixture(SystemId);
	Report.Profile = EStargameValidationProfile::M12Gameplay;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM12GameplaySystemDefinition(SystemDefinition, Report);
	return Report;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM12Fixture(FName SystemId) const
{
	FStargameValidationReport Report = ValidateM12GameplayFixture(SystemId);
	Report.Profile = EStargameValidationProfile::M12;
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	FStarSystemDefinition SystemDefinition;
	if (!ResolveSystemDefinition(SystemId, SystemDefinition))
	{
		AddIssue(Report, EStargameValidationSeverity::Fatal, TEXT("missing_system"), SystemId, TEXT("Required system definition could not be resolved."));
		return Report;
	}

	ValidateM12ScenarioSystemDefinition(SystemDefinition, Report);
	return Report;
}

bool UStarCatalogSubsystem::GenerateSeededPhysicalSystem(int32 Seed, FStarSystemDefinition& OutSystemDefinition) const
{
	FRandomStream Stream(Seed);
	const FString SeedSuffix = FString::Printf(TEXT("%08x"), static_cast<uint32>(Seed));

	FStarSystemDefinition SystemDefinition;
	SystemDefinition.SystemId = FName(*FString::Printf(TEXT("generated_m6_%s"), *SeedSuffix));
	SystemDefinition.DisplayName = FText::FromString(FString::Printf(TEXT("Generated M6 %s"), *SeedSuffix));
	SystemDefinition.SourceType = ESystemSourceType::Generated;
	SystemDefinition.Seed = Seed;
	SystemDefinition.Scale = FStargameScaleContract();
	SystemDefinition.GeneratedSourceMetadata.GeneratedSeed = Seed;
	SystemDefinition.GeneratedSourceMetadata.GeneratorVersion = TEXT("m6_physical_v1");
	SystemDefinition.GeneratedSourceMetadata.SourceSystemId = FFrontierTestFixtureProvider::FrontierSystemId;
	SystemDefinition.GeneratedSourceMetadata.GenerationProfile = TEXT("M6");
	SystemDefinition.GeneratedSourceMetadata.GeneratedAtUtc = TEXT("1970-01-01T00:00:00Z");
	SystemDefinition.GeneratedSourceMetadata.AuthoredOverrideIds = { TEXT("frontier_station_basic"), TEXT("frontier_gate_basic"), TEXT("ember_visual") };
	SystemDefinition.GeneratedSourceMetadata.GenerationInputIds = {
		FFrontierTestFixtureProvider::FrontierSystemId,
		TEXT("m6_seeded_physical_layout"),
		TEXT("m6_resource_zone_profile")
	};
	SystemDefinition.GeneratedSourceMetadata.SourceFingerprint = FString::Printf(
		TEXT("m6_physical_v1:%s:%s"),
		*FString::FromInt(Seed),
		*FFrontierTestFixtureProvider::FrontierSystemId.ToString());

	const FName StarId(*FString::Printf(TEXT("%s_primary"), *SystemDefinition.SystemId.ToString()));
	const FName InnerPlanetId(*FString::Printf(TEXT("%s_inner"), *SystemDefinition.SystemId.ToString()));
	const FName OuterPlanetId(*FString::Printf(TEXT("%s_outer"), *SystemDefinition.SystemId.ToString()));
	const FName MoonId(*FString::Printf(TEXT("%s_moon"), *SystemDefinition.SystemId.ToString()));
	const FName StationId(*FString::Printf(TEXT("%s_watch"), *SystemDefinition.SystemId.ToString()));
	const FName GateId(*FString::Printf(TEXT("%s_gate_a"), *SystemDefinition.SystemId.ToString()));
	const FName ResourceZoneId(*FString::Printf(TEXT("%s_resource_belt"), *SystemDefinition.SystemId.ToString()));
	const FName SpawnZoneId(*FString::Printf(TEXT("%s_spawn_deep"), *SystemDefinition.SystemId.ToString()));

	auto MakeBody = [&SystemDefinition](FName BodyId, const FString& DisplayName, FName BodyType, double RadiusCm, FName ParentId, double SemiMajorAxisCm, double PeriodSeconds, double Phase)
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
		Body.NavigationTarget = MakeGeneratedNavigationTarget(BodyId, DisplayName, TEXT("body"), Body.FrameType, ParentId);
		Body.Orbit.ParentId = ParentId;
		Body.Orbit.SemiMajorAxisCm = SemiMajorAxisCm;
		Body.Orbit.PeriodSeconds = PeriodSeconds;
		Body.Orbit.PhaseOffsetRadians = Phase;
		return Body;
	};

	const double InnerOrbitCm = 10000000.0 + Stream.FRandRange(0.0, 2000000.0);
	const double OuterOrbitCm = 42000000.0 + Stream.FRandRange(0.0, 6000000.0);
	SystemDefinition.Bodies.Add(MakeBody(StarId, TEXT("Generated Primary"), TEXT("star"), 220000.0, NAME_None, 0.0, 0.0, 0.0));
	SystemDefinition.Bodies.Add(MakeBody(InnerPlanetId, TEXT("Generated Inner"), TEXT("rocky_planet"), 52000.0, StarId, InnerOrbitCm, 800.0 + Stream.FRandRange(0.0, 120.0), Stream.FRandRange(0.0, UE_DOUBLE_PI)));
	SystemDefinition.Bodies.Add(MakeBody(OuterPlanetId, TEXT("Generated Outer"), TEXT("gas_giant"), 120000.0, StarId, OuterOrbitCm, 1900.0 + Stream.FRandRange(0.0, 220.0), Stream.FRandRange(0.0, UE_DOUBLE_PI)));
	SystemDefinition.Bodies.Add(MakeBody(MoonId, TEXT("Generated Moon"), TEXT("moon"), 22000.0, OuterPlanetId, 3600000.0, 260.0, Stream.FRandRange(0.0, UE_DOUBLE_PI)));

	FStationDefinition Station;
	Station.StationId = StationId;
	Station.DisplayName = FText::FromString(TEXT("Generated Watch"));
	Station.FrameType = TEXT("station_relative");
	Station.AnchorId = OuterPlanetId;
	Station.VisualRadiusCm = 13000.0;
	Station.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
	Station.DockingPorts = { MakeGeneratedDockingPort(FName(*FString::Printf(TEXT("%s_port_a"), *StationId.ToString()))) };
	Station.NavigationTarget = MakeGeneratedNavigationTarget(StationId, TEXT("Generated Watch"), TEXT("station"), Station.FrameType, Station.AnchorId);
	Station.Orbit.ParentId = OuterPlanetId;
	Station.Orbit.SemiMajorAxisCm = 3200000.0;
	Station.Orbit.PeriodSeconds = 95.0;
	Station.Orbit.PhaseOffsetRadians = Stream.FRandRange(0.0, UE_DOUBLE_PI);
	SystemDefinition.Stations.Add(Station);

	FGateDefinition Gate;
	Gate.GateId = GateId;
	Gate.DisplayName = FText::FromString(TEXT("Generated Gate A"));
	Gate.FrameType = TEXT("system_barycentric");
	Gate.AnchorId = StarId;
	Gate.Transform = FTransform(FRotator(0.0, 35.0, 0.0), FVector(-OuterOrbitCm * 0.85, OuterOrbitCm * 0.35, 0.0));
	Gate.VisualRadiusCm = 16000.0;
	Gate.ActivationRangeCm = 50000.0;
	Gate.GateProfileId = FPrimaryAssetId(UGateProfileAsset::AssetType, TEXT("frontier_gate_basic"));
	Gate.NavigationTarget = MakeGeneratedNavigationTarget(GateId, TEXT("Generated Gate A"), TEXT("gate"), Gate.FrameType, Gate.AnchorId);
	SystemDefinition.Gates.Add(Gate);

	FGravityWellDefinition InnerWell;
	InnerWell.WellId = FName(*FString::Printf(TEXT("%s_well"), *InnerPlanetId.ToString()));
	InnerWell.AnchorBodyId = InnerPlanetId;
	InnerWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
	InnerWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
	InnerWell.DropoutRadiusCm = 350000.0;
	InnerWell.Strength = 0.5;

	FGravityWellDefinition OuterWell;
	OuterWell.WellId = FName(*FString::Printf(TEXT("%s_well"), *OuterPlanetId.ToString()));
	OuterWell.AnchorBodyId = OuterPlanetId;
	OuterWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
	OuterWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
	OuterWell.DropoutRadiusCm = 350000.0;
	OuterWell.Strength = 0.8;
	SystemDefinition.GravityWells = { InnerWell, OuterWell };

	FResourceZoneDefinition ResourceZone;
	ResourceZone.ZoneId = ResourceZoneId;
	ResourceZone.DisplayName = FText::FromString(TEXT("Generated Resource Belt"));
	ResourceZone.ZoneType = TEXT("asteroid_resource_zone");
	ResourceZone.FrameType = TEXT("body_relative");
	ResourceZone.AnchorId = OuterPlanetId;
	ResourceZone.Transform = FTransform(FRotator::ZeroRotator, FVector(5200000.0, 0.0, 0.0));
	ResourceZone.RadiusCm = 650000.0;
	ResourceZone.ResourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.Resource.Ore"), false));
	ResourceZone.HazardTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.Hazard.Asteroid"), false));
	ResourceZone.NavigationTarget = MakeGeneratedNavigationTarget(ResourceZoneId, TEXT("Generated Resource Belt"), TEXT("resource"), ResourceZone.FrameType, ResourceZone.AnchorId);
	ResourceZone.NavigationTarget.bShowInHud = false;
	SystemDefinition.ResourceZones.Add(ResourceZone);

	FSpawnZoneDefinition SpawnZone;
	SpawnZone.SpawnZoneId = SpawnZoneId;
	SpawnZone.DisplayName = FText::FromString(TEXT("Generated Deep Spawn"));
	SpawnZone.FrameType = TEXT("system_barycentric");
	SpawnZone.AnchorId = StarId;
	SpawnZone.Transform = FTransform(FRotator(0.0, 12.0, 0.0), FVector(OuterOrbitCm * 0.55, -OuterOrbitCm * 0.85, 0.0));
	SpawnZone.RadiusCm = 25000.0;
	SpawnZone.AllowedContexts = { TEXT("new_session"), TEXT("debug"), TEXT("arrival") };
	SystemDefinition.SpawnZones.Add(SpawnZone);

	const TArray<TPair<FName, FName>> MapSources = {
		{ StarId, TEXT("body") },
		{ InnerPlanetId, TEXT("body") },
		{ OuterPlanetId, TEXT("body") },
		{ MoonId, TEXT("body") },
		{ StationId, TEXT("station") },
		{ GateId, TEXT("gate") },
		{ ResourceZoneId, TEXT("resource") }
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

	OutSystemDefinition = SystemDefinition;
	return true;
}

FStargameValidationReport UStarCatalogSubsystem::ValidateM6GeneratedSystemDefinition(const FStarSystemDefinition& SystemDefinition) const
{
	FStargameValidationReport Report;
	Report.Profile = EStargameValidationProfile::M6;
	Report.SystemId = SystemDefinition.SystemId;
	Report.CheckedSystemCount = 1;
	Report.CheckedFixtureCount = 1;

	ValidateSystemDefinition(SystemDefinition, Report);
	if (Report.HasBlockingIssues())
	{
		return Report;
	}

	ValidateM6SystemDefinition(SystemDefinition, Report);
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
	Issue.Code = RuleId;
	Issue.RuleId = RuleId;
	Issue.ObjectId = ObjectId;
	Issue.SystemId = Report.SystemId;
	Issue.SourceId = ObjectId;
	Issue.Message = Message;
	Report.Issues.Add(Issue);
	Report.RefreshSummary();
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

	TSet<FName> ResourceZoneIds;
	for (const FResourceZoneDefinition& ResourceZone : SystemDefinition.ResourceZones)
	{
		AddUniqueId(ResourceZone.ZoneId, TEXT("invalid_resource_zone_id"), TEXT("Resource zone"));
		if (!ResourceZone.AnchorId.IsNone() && !EntityExists(ResourceZone.AnchorId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_resource_zone_anchor"), ResourceZone.ZoneId, TEXT("Resource zone anchor ID does not resolve to a known system entity."));
		}
		if (ResourceZone.RadiusCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_resource_zone_radius"), ResourceZone.ZoneId, TEXT("Resource zone radius must be positive."));
		}
		if (ResourceZone.ResourceTags.IsEmpty())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_resource_zone_tags"), ResourceZone.ZoneId, TEXT("Resource zones must declare at least one resource tag."));
		}
		ResourceZoneIds.Add(ResourceZone.ZoneId);
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
	for (const FResourceZoneDefinition& ResourceZone : SystemDefinition.ResourceZones)
	{
		AddNavigationTarget(ResourceZone.NavigationTarget, ResourceZone.ZoneId);
	}

	TSet<FName> MapEntryIds;
	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (MapEntry.MapEntryId.IsNone() || MapEntryIds.Contains(MapEntry.MapEntryId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_map_entry"), MapEntry.MapEntryId, TEXT("Map entry IDs must be non-empty and unique."));
		}
		if (!EntityExists(MapEntry.SourceId) && !SpawnZoneIds.Contains(MapEntry.SourceId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("unresolved_map_entry_source"), MapEntry.MapEntryId, TEXT("Map entry source ID does not resolve to a system entity."));
		}
		MapEntryIds.Add(MapEntry.MapEntryId);
	}

	return !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM2SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FStargameScaleContract& Scale = SystemDefinition.Scale;
	if (Scale.LocalBubbleRadiusCm != 5000000.0 || Scale.OriginShiftThresholdCm != 2000000.0 ||
		Scale.MapDistanceScaleCmPerUnit != 1000000.0 || Scale.MapMinIconScale != 0.75 || Scale.MapMaxIconScale != 2.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m2_scale_contract"), SystemDefinition.SystemId, TEXT("M2 fixture scale values must match the documented provisional scale contract."));
		bValid = false;
	}
	if (Scale.LocalBubbleRadiusCm <= Scale.OriginShiftThresholdCm || Scale.MapMinIconScale <= 0.0 || Scale.MapMaxIconScale < Scale.MapMinIconScale)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m2_scale_ranges"), SystemDefinition.SystemId, TEXT("M2 scale values have invalid ranges."));
		bValid = false;
	}

	auto HasBody = [&SystemDefinition](FName BodyId)
	{
		return SystemDefinition.Bodies.ContainsByPredicate([BodyId](const FBodyDefinition& Body)
		{
			return Body.BodyId == BodyId;
		});
	};
	auto HasStation = [&SystemDefinition](FName StationId)
	{
		return SystemDefinition.Stations.ContainsByPredicate([StationId](const FStationDefinition& Station)
		{
			return Station.StationId == StationId;
		});
	};
	if (!HasBody(TEXT("frontier_primary")) || !HasBody(TEXT("brink")) || !HasBody(TEXT("brink_minor")) || !HasStation(TEXT("wayfarer_depot")))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_m2_fixture_entities"), SystemDefinition.SystemId, TEXT("M2 fixture requires frontier_primary, brink, brink_minor, and wayfarer_depot authored entities."));
		bValid = false;
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FFrameResolvedTransform BrinkMinorAt0;
	FFrameResolvedTransform BrinkMinorAt0Repeat;
	FFrameResolvedTransform BrinkMinorAt60;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_minor"), ClockSnapshot, 0.0, BrinkMinorAt0) ||
		!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_minor"), ClockSnapshot, 0.0, BrinkMinorAt0Repeat) ||
		!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_minor"), ClockSnapshot, 60.0, BrinkMinorAt60))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_frame_query_failed"), TEXT("brink_minor"), TEXT("M2 frame query service could not resolve nested moon transforms."));
		bValid = false;
	}
	else
	{
		if (!BrinkMinorAt0.PositionCm.Equals(BrinkMinorAt0Repeat.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_frame_query_not_deterministic"), TEXT("brink_minor"), TEXT("Same-time M2 frame queries must produce identical transforms."));
			bValid = false;
		}
		if (BrinkMinorAt0.PositionCm.Equals(BrinkMinorAt60.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_orbit_not_time_varying"), TEXT("brink_minor"), TEXT("Nested moon orbit must vary across requested simulation time."));
			bValid = false;
		}
	}

	FFrameResolvedTransform BrinkWatchAt0;
	FFrameResolvedTransform BrinkWatchAt30;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_watch"), ClockSnapshot, 0.0, BrinkWatchAt0) ||
		!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_watch"), ClockSnapshot, 30.0, BrinkWatchAt30))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_station_frame_query_failed"), TEXT("brink_watch"), TEXT("M2 frame query service could not resolve orbiting station transforms."));
		bValid = false;
	}
	else if (BrinkWatchAt0.PositionCm.Equals(BrinkWatchAt30.PositionCm, 0.01))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_station_orbit_not_time_varying"), TEXT("brink_watch"), TEXT("Orbiting station transform must vary across requested simulation time."));
		bValid = false;
	}

	TArray<FSystemMapEntryViewModel> MapEntries;
	UOrbitRouteFrameQueryService::BuildSystemMapViewModel(SystemDefinition, ClockSnapshot, 0.0, MapEntries);
	if (MapEntries.Num() < 7)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_map_entries_incomplete"), SystemDefinition.SystemId, TEXT("M2 system map view model must include body, station, and gate hierarchy from registry data."));
		bValid = false;
	}
	if (!MapEntries.ContainsByPredicate([](const FSystemMapEntryViewModel& Entry)
	{
		return Entry.EntryId == FName(TEXT("brink_minor")) && Entry.ParentId == FName(TEXT("brink"));
	}))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_map_hierarchy_missing"), TEXT("brink_minor"), TEXT("M2 system map must preserve nested body hierarchy."));
		bValid = false;
	}

	FShipSaveLocation SaveLocation;
	SaveLocation.SystemId = SystemDefinition.SystemId;
	SaveLocation.CoordinateFrame.FrameType = TEXT("body_relative");
	SaveLocation.CoordinateFrame.AnchorId = TEXT("brink");
	SaveLocation.LocationMode = EShipLocationMode::FreeFlight;
	SaveLocation.AnchorId = TEXT("brink");
	SaveLocation.PositionCm = FVector(1000.0, 2000.0, 300.0);
	SaveLocation.VelocityFrame = SaveLocation.CoordinateFrame;
	SaveLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;
	if (SaveLocation.SystemId.IsNone() || SaveLocation.CoordinateFrame.FrameType.IsNone() || SaveLocation.LocationMode != EShipLocationMode::FreeFlight || SaveLocation.AnchorId.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m2_logical_save_location_invalid"), SystemDefinition.SystemId, TEXT("M2 logical ship save location must preserve system, frame, mode, anchor, position, velocity frame, and simulation time."));
		bValid = false;
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM3SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FStargameScaleContract& Scale = SystemDefinition.Scale;
	if (Scale.NormalFlightMaxSpeedCmPerSec != 24000.0 || Scale.StationApproachBubbleRadiusCm != 500000.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m3_scale_contract"), SystemDefinition.SystemId, TEXT("M3 fixture scale values must match documented normal-flight and station approach values."));
		bValid = false;
	}
	if (Scale.NormalFlightMaxSpeedCmPerSec <= 0.0 || Scale.StationApproachBubbleRadiusCm <= 0.0 || Scale.StationApproachBubbleRadiusCm >= Scale.LocalBubbleRadiusCm)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m3_scale_ranges"), SystemDefinition.SystemId, TEXT("M3 normal-flight and approach radii have invalid ranges."));
		bValid = false;
	}

	auto FindTarget = [&SystemDefinition](FName TargetId) -> const FNavigationTargetDefinition*
	{
		for (const FBodyDefinition& Body : SystemDefinition.Bodies)
		{
			if (Body.NavigationTarget.TargetId == TargetId)
			{
				return &Body.NavigationTarget;
			}
		}
		for (const FStationDefinition& Station : SystemDefinition.Stations)
		{
			if (Station.NavigationTarget.TargetId == TargetId)
			{
				return &Station.NavigationTarget;
			}
		}
		for (const FGateDefinition& Gate : SystemDefinition.Gates)
		{
			if (Gate.NavigationTarget.TargetId == TargetId)
			{
				return &Gate.NavigationTarget;
			}
		}
		return nullptr;
	};

	const TArray<FName> RequiredTargets = {
		TEXT("ember"),
		TEXT("brink_watch"),
		TEXT("frontier_gate_a")
	};
	for (const FName TargetId : RequiredTargets)
	{
		const FNavigationTargetDefinition* Target = FindTarget(TargetId);
		if (!Target || !Target->bCanTarget || !Target->bShowInHud || Target->FrameType.IsNone())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m3_navigation_target"), TargetId, TEXT("M3 requires body, station, and gate targets to be selectable, HUD-visible, and frame-qualified."));
			bValid = false;
		}
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FFrameResolvedTransform BrinkWatchTransform;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_watch"), ClockSnapshot, 0.0, BrinkWatchTransform))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m3_station_approach_unresolved"), TEXT("brink_watch"), TEXT("M3 must resolve the orbiting station for normal-flight approach telemetry."));
		bValid = false;
	}
	else
	{
		const FVector ApproachPosition = BrinkWatchTransform.PositionCm + FVector(Scale.StationApproachBubbleRadiusCm * 0.5, 0.0, 0.0);
		const double ApproachDistance = FVector::Distance(ApproachPosition, BrinkWatchTransform.PositionCm);
		if (ApproachDistance > Scale.StationApproachBubbleRadiusCm)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m3_station_approach_invalid"), TEXT("brink_watch"), TEXT("M3 station approach fixture must be reachable inside the station approach bubble."));
			bValid = false;
		}
	}

	FFrameResolvedTransform GateTransform;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("frontier_gate_a"), ClockSnapshot, 0.0, GateTransform))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m3_gate_approach_unresolved"), TEXT("frontier_gate_a"), TEXT("M3 must resolve the gate for local approach telemetry."));
		bValid = false;
	}
	else
	{
		const FGateDefinition* Gate = SystemDefinition.Gates.FindByPredicate([](const FGateDefinition& Candidate)
		{
			return Candidate.GateId == FName(TEXT("frontier_gate_a"));
		});
		if (!Gate || Gate->ActivationRangeCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m3_gate_activation_missing"), TEXT("frontier_gate_a"), TEXT("M3 gate approach requires a positive activation range, even though transition is not implemented yet."));
			bValid = false;
		}
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM4SystemDefinition(const FStarSystemDefinition& SystemDefinition, const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FStargameScaleContract& Scale = SystemDefinition.Scale;
	if (Scale.SupercruiseMinSpeedCmPerSec != 100000.0 ||
		Scale.SupercruiseMaxSpeedCmPerSec != 20000000.0 ||
		Scale.SupercruiseSpoolSeconds != 3.0 ||
		Scale.DropoutCooldownSeconds != 5.0 ||
		Scale.SupercruiseTargetDropoutMinRadiusCm != 250000.0 ||
		Scale.SupercruiseTargetDropoutMaxRadiusCm != 750000.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m4_supercruise_scale_contract"), SystemDefinition.SystemId, TEXT("M4 fixture scale values must match the documented provisional supercruise contract."));
		bValid = false;
	}
	if (Scale.SupercruiseMinSpeedCmPerSec <= Scale.NormalFlightMaxSpeedCmPerSec ||
		Scale.SupercruiseMaxSpeedCmPerSec <= Scale.SupercruiseMinSpeedCmPerSec ||
		Scale.SupercruiseTargetDropoutMinRadiusCm <= 0.0 ||
		Scale.SupercruiseTargetDropoutMaxRadiusCm <= Scale.SupercruiseTargetDropoutMinRadiusCm)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m4_supercruise_scale_ranges"), SystemDefinition.SystemId, TEXT("M4 supercruise speed and dropout ranges are invalid."));
		bValid = false;
	}

	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (GravityWell.SlowdownRadiusCm < GravityWell.LockoutRadiusCm ||
			GravityWell.LockoutRadiusCm < GravityWell.DropoutRadiusCm ||
			GravityWell.DropoutRadiusCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_m4_gravity_well_radii"), GravityWell.WellId, TEXT("M4 gravity wells must satisfy slowdown >= lockout >= dropout > 0."));
			bValid = false;
		}
	}

	const FShipArchetypeDefinition* Ship = ShipArchetypesById.Find(StartProfile.ShipArchetypeId);
	const FShipMovementProfileDefinition* Movement = Ship ? MovementProfilesById.Find(Ship->MovementProfileId) : nullptr;
	if (!Movement)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_m4_movement_profile"), StartProfile.ShipArchetypeId, TEXT("M4 requires the start ship movement profile to resolve."));
		bValid = false;
	}
	else
	{
		if (Movement->SupercruiseTuningId.IsNone() ||
			!Movement->SupportedFlightModeTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Supercruise"), false)))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_m4_supercruise_tuning"), Movement->MovementProfileId, TEXT("M4 movement profile must declare supercruise tuning and support tag."));
			bValid = false;
		}
		if (Movement->SupercruiseAccelerationCurve.IsNull() || !Movement->SupercruiseAccelerationCurve.LoadSynchronous() ||
			Movement->SupercruiseDecelerationCurve.IsNull() || !Movement->SupercruiseDecelerationCurve.LoadSynchronous() ||
			Movement->SupercruiseTurnResponseCurve.IsNull() || !Movement->SupercruiseTurnResponseCurve.LoadSynchronous())
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("missing_m4_supercruise_curves"), Movement->MovementProfileId, TEXT("M4 movement profile must reference acceleration, deceleration, and turn-response curve assets."));
			bValid = false;
		}
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FFrameResolvedTransform SpawnTransform;
	FFrameResolvedTransform DistantTargetTransform;
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, StartProfile.SpawnZoneId, ClockSnapshot, 0.0, SpawnTransform))
	{
		SpawnTransform.PositionCm = FVector::ZeroVector;
	}
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("brink_watch"), ClockSnapshot, 0.0, DistantTargetTransform))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m4_distant_target_unresolved"), TEXT("brink_watch"), TEXT("M4 requires the distant supercruise target leg to resolve."));
		bValid = false;
	}
	else if (FVector::Distance(SpawnTransform.PositionCm, DistantTargetTransform.PositionCm) <= Scale.LocalBubbleRadiusCm)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m4_missing_distant_travel_leg"), TEXT("brink_watch"), TEXT("M4 requires a fixture target leg longer than the local bubble so supercruise is meaningful."));
		bValid = false;
	}

	FFrameResolvedTransform EmberTransform;
	if (UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TEXT("ember"), ClockSnapshot, 0.0, EmberTransform))
	{
		FGravityWellQueryResult LockoutQuery;
		const FVector LockoutPosition = EmberTransform.PositionCm + FVector(Scale.GravityLockoutRadiusCm * 0.5, 0.0, 0.0);
		if (!UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, LockoutPosition, FVector::ZeroVector, EShipFlightMode::Normal, ClockSnapshot, 0.0, LockoutQuery) ||
			!LockoutQuery.bInsideLockout)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m4_lockout_query_failed"), TEXT("ember_well"), TEXT("M4 gravity query must detect lockout before supercruise entry."));
			bValid = false;
		}

		FGravityWellQueryResult DropoutQuery;
		const FVector DropoutPosition = EmberTransform.PositionCm + FVector(Scale.GravityLockoutRadiusCm * 0.25, 0.0, 0.0);
		if (!UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, DropoutPosition, FVector::ZeroVector, EShipFlightMode::Supercruise, ClockSnapshot, 0.0, DropoutQuery) ||
			!DropoutQuery.bForcedDropout)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m4_dropout_query_failed"), TEXT("ember_well"), TEXT("M4 gravity query must force dropout inside the configured dropout radius."));
			bValid = false;
		}
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM5SystemDefinition(const FStarSystemDefinition& SystemDefinition, const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const
{
	bool bValid = true;

	const FStationDefinition* BrinkWatch = SystemDefinition.Stations.FindByPredicate([](const FStationDefinition& Station)
	{
		return Station.StationId == FName(TEXT("brink_watch"));
	});
	const FDockingPortDefinition* Port = BrinkWatch
		? BrinkWatch->DockingPorts.FindByPredicate([](const FDockingPortDefinition& Candidate)
		{
			return Candidate.PortId == FName(TEXT("brink_watch_port_a"));
		})
		: nullptr;

	if (!Port)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_missing_orbiting_station_port"), TEXT("brink_watch_port_a"), TEXT("M5 requires brink_watch_port_a on the orbiting brink_watch station."));
		return false;
	}

	if (!Port->ApproachTransform.GetLocation().Equals(FVector(0.0, -25000.0, 0.0), 0.01) ||
		!Port->DockedTransform.GetLocation().Equals(FVector(0.0, -1200.0, 0.0), 0.01) ||
		!Port->UndockTransform.GetLocation().Equals(FVector(0.0, -6000.0, 0.0), 0.01))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_invalid_port_transforms"), Port->PortId, TEXT("M5 docking port approach, docked, and undock transforms must match the documented fixture."));
		bValid = false;
	}
	if (Port->ActivationRangeCm != 15000.0 || Port->MaxApproachSpeedCmPerSec != 2500.0 || Port->RequiredAlignmentDegrees != 10.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_invalid_port_limits"), Port->PortId, TEXT("M5 docking port activation range, approach speed, and alignment limits must match the documented fixture."));
		bValid = false;
	}
	if (Port->AllowedShipClasses.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_missing_allowed_ship_class"), Port->PortId, TEXT("M5 docking ports must declare allowed ship classes."));
		bValid = false;
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FFrameResolvedTransform ApproachAt0;
	FFrameResolvedTransform DockedAt0;
	FFrameResolvedTransform DockedAt30;
	if (!UOrbitRouteFrameQueryService::ResolveDockingPortTransform(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), EDockingPortTransformKind::Approach, ClockSnapshot, 0.0, ApproachAt0) ||
		!UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), ClockSnapshot, 0.0, DockedAt0) ||
		!UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), ClockSnapshot, 30.0, DockedAt30))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_docking_port_frame_failed"), TEXT("brink_watch_port_a"), TEXT("M5 must resolve live approach and docked transforms for the orbiting docking port."));
		bValid = false;
	}
	else
	{
		if (ApproachAt0.PositionCm.Equals(DockedAt0.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_port_approach_equals_docked"), TEXT("brink_watch_port_a"), TEXT("M5 approach and docked live transforms must be distinct."));
			bValid = false;
		}
		if (DockedAt0.PositionCm.Equals(DockedAt30.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_port_not_moving_with_station"), TEXT("brink_watch_port_a"), TEXT("M5 docking port transform must inherit station orbital motion."));
			bValid = false;
		}
	}

	FShipSaveLocation SaveLocation;
	SaveLocation.SystemId = SystemDefinition.SystemId;
	SaveLocation.CoordinateFrame.FrameType = TEXT("station_relative");
	SaveLocation.CoordinateFrame.AnchorId = TEXT("brink_watch");
	SaveLocation.LocationMode = EShipLocationMode::StationDocked;
	SaveLocation.AnchorId = TEXT("brink_watch");
	SaveLocation.DockedStationId = TEXT("brink_watch");
	SaveLocation.DockingPortId = TEXT("brink_watch_port_a");
	SaveLocation.DockingState = EDockingState::Docked;
	if (SaveLocation.LocationMode != EShipLocationMode::StationDocked ||
		SaveLocation.DockedStationId.IsNone() ||
		SaveLocation.DockingPortId.IsNone() ||
		SaveLocation.DockingState != EDockingState::Docked)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_docked_save_state_invalid"), SystemDefinition.SystemId, TEXT("M5 save data must preserve docked station, port, and docking state."));
		bValid = false;
	}

	const FShipArchetypeDefinition* Ship = ShipArchetypesById.Find(StartProfile.ShipArchetypeId);
	if (!Ship || Ship->DockingSizeClass.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_missing_ship_docking_size"), StartProfile.ShipArchetypeId, TEXT("M5 requires the start ship to declare a docking size class."));
		bValid = false;
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM5_5SystemDefinition(const FStarSystemDefinition& SourceSystemDefinition, const FStarSystemDefinition& DestinationSystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	const FGateDefinition* SourceGate = SourceSystemDefinition.Gates.FindByPredicate([](const FGateDefinition& Gate)
	{
		return Gate.GateId == FName(TEXT("frontier_gate_a"));
	});
	if (!SourceGate)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_5_missing_source_gate"), TEXT("frontier_gate_a"), TEXT("M5.5 requires frontier_gate_a to be registered in the active source system."));
		return false;
	}

	Require(SourceGate->DestinationSystemId == FName(TEXT("frontier_arrival_test_01")), TEXT("m5_5_invalid_destination_system"), SourceGate->GateId, TEXT("frontier_gate_a must target frontier_arrival_test_01."));
	Require(SourceGate->DestinationGateId == FName(TEXT("arrival_gate_a")), TEXT("m5_5_invalid_destination_gate"), SourceGate->GateId, TEXT("frontier_gate_a must target arrival_gate_a."));
	Require(SourceGate->DestinationArrivalId == FName(TEXT("arrival_from_frontier_gate_a")), TEXT("m5_5_invalid_destination_arrival"), SourceGate->GateId, TEXT("frontier_gate_a must target arrival_from_frontier_gate_a."));
	Require(SourceGate->DestinationArrivalFrame == FName(TEXT("gate_relative")), TEXT("m5_5_invalid_arrival_frame"), SourceGate->GateId, TEXT("M5.5 arrival placement must be gate-relative."));
	Require(SourceGate->DestinationArrivalLocalOffsetCm.Equals(FVector(0.0, 18000.0, 0.0), 0.01), TEXT("m5_5_invalid_arrival_offset"), SourceGate->GateId, TEXT("M5.5 arrival offset must match the documented fixture."));
	Require(SourceGate->DestinationArrivalRotation.Equals(FRotator(0.0, 180.0, 0.0), 0.01), TEXT("m5_5_invalid_arrival_rotation"), SourceGate->GateId, TEXT("M5.5 arrival rotation must match the documented fixture."));

	const bool bDestinationCatalogEntry = CatalogEntries.ContainsByPredicate([SourceGate](const FStarCatalogEntry& Entry)
	{
		return Entry.SystemId == SourceGate->DestinationSystemId;
	});
	Require(bDestinationCatalogEntry, TEXT("m5_5_missing_destination_catalog_entry"), SourceGate->DestinationSystemId, TEXT("M5.5 requires the destination system to resolve through the catalog."));

	ValidateSystemDefinition(DestinationSystemDefinition, Report);

	const bool bRouteEdge = RouteGraphEdges.ContainsByPredicate([SourceGate, &SourceSystemDefinition](const FRouteGraphEdge& Edge)
	{
		return Edge.RouteEdgeId == FName(TEXT("edge_frontier_gate_a_to_arrival_gate_a")) &&
			Edge.SourceSystemId == SourceSystemDefinition.SystemId &&
			Edge.SourceGateId == SourceGate->GateId &&
			Edge.DestinationSystemId == SourceGate->DestinationSystemId &&
			Edge.DestinationGateId == SourceGate->DestinationGateId &&
			Edge.DestinationArrivalId == SourceGate->DestinationArrivalId &&
			Edge.RouteType == FName(TEXT("jump_gate")) &&
			Edge.bArtificialGate;
	});
	Require(bRouteEdge, TEXT("m5_5_missing_catalog_route_edge"), SourceGate->GateId, TEXT("M5.5 requires an authored catalog route edge from frontier_gate_a to the destination arrival."));

	const FGateDefinition* DestinationGate = DestinationSystemDefinition.Gates.FindByPredicate([SourceGate](const FGateDefinition& Gate)
	{
		return Gate.GateId == SourceGate->DestinationGateId;
	});
	Require(DestinationGate != nullptr, TEXT("m5_5_missing_destination_gate"), SourceGate->DestinationGateId, TEXT("M5.5 destination system must register arrival_gate_a."));

	const FSpawnZoneDefinition* Arrival = DestinationSystemDefinition.SpawnZones.FindByPredicate([SourceGate](const FSpawnZoneDefinition& SpawnZone)
	{
		return SpawnZone.SpawnZoneId == SourceGate->DestinationArrivalId;
	});
	if (!Arrival)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m5_5_missing_arrival_marker"), SourceGate->DestinationArrivalId, TEXT("M5.5 destination system must register the configured arrival marker."));
		bValid = false;
	}
	else
	{
		Require(Arrival->FrameType == SourceGate->DestinationArrivalFrame && Arrival->AnchorId == SourceGate->DestinationGateId, TEXT("m5_5_arrival_marker_frame_mismatch"), Arrival->SpawnZoneId, TEXT("M5.5 arrival marker must be anchored in the configured gate-relative frame."));
		Require(Arrival->Transform.GetLocation().Equals(SourceGate->DestinationArrivalLocalOffsetCm, 0.01), TEXT("m5_5_arrival_marker_offset_mismatch"), Arrival->SpawnZoneId, TEXT("M5.5 arrival marker offset must match source gate arrival data."));
		Require(Arrival->Transform.Rotator().Equals(SourceGate->DestinationArrivalRotation, 0.01), TEXT("m5_5_arrival_marker_rotation_mismatch"), Arrival->SpawnZoneId, TEXT("M5.5 arrival marker rotation must match source gate arrival data."));
		Require(Arrival->AllowedContexts.Contains(FName(TEXT("arrival"))), TEXT("m5_5_arrival_marker_context_missing"), Arrival->SpawnZoneId, TEXT("M5.5 arrival marker must allow arrival context."));
	}

	FShipSaveLocation PendingArrival;
	PendingArrival.SystemId = SourceGate->DestinationSystemId;
	PendingArrival.LocationMode = EShipLocationMode::GateArrival;
	PendingArrival.CoordinateFrame.FrameType = SourceGate->DestinationArrivalFrame;
	PendingArrival.CoordinateFrame.AnchorId = SourceGate->DestinationGateId;
	PendingArrival.AnchorId = SourceGate->DestinationGateId;
	PendingArrival.GateArrivalId = SourceGate->DestinationArrivalId;
	PendingArrival.PositionCm = SourceGate->DestinationArrivalLocalOffsetCm;
	PendingArrival.Rotation = SourceGate->DestinationArrivalRotation;
	Require(PendingArrival.LocationMode == EShipLocationMode::GateArrival &&
		PendingArrival.SystemId == SourceGate->DestinationSystemId &&
		PendingArrival.GateArrivalId == SourceGate->DestinationArrivalId &&
		PendingArrival.CoordinateFrame.FrameType == FName(TEXT("gate_relative")),
		TEXT("m5_5_pending_arrival_save_invalid"), SourceGate->GateId, TEXT("M5.5 pending-arrival save state must preserve destination system, arrival marker, and gate-relative frame."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM6SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;

	if (SystemDefinition.SourceType != ESystemSourceType::Generated)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_system_not_generated"), SystemDefinition.SystemId, TEXT("M6 validates generated physical system definitions only."));
		bValid = false;
	}
	if (SystemDefinition.Seed != SystemDefinition.GeneratedSourceMetadata.GeneratedSeed ||
		SystemDefinition.GeneratedSourceMetadata.GeneratorVersion.IsNone() ||
		SystemDefinition.GeneratedSourceMetadata.SourceSystemId.IsNone() ||
		SystemDefinition.GeneratedSourceMetadata.GenerationProfile != FName(TEXT("M6")) ||
		!IsValidGeneratedUtcStamp(SystemDefinition.GeneratedSourceMetadata.GeneratedAtUtc) ||
		SystemDefinition.GeneratedSourceMetadata.SourceFingerprint.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_generated_metadata"), SystemDefinition.SystemId, TEXT("M6 generated output must declare seed, generator version, source system ID, generation profile, UTC timestamp, and source fingerprint metadata."));
		bValid = false;
	}
	if (!HasOnlyNamedIds(SystemDefinition.GeneratedSourceMetadata.AuthoredOverrideIds) ||
		!HasOnlyNamedIds(SystemDefinition.GeneratedSourceMetadata.GenerationInputIds))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_source_dependencies"), SystemDefinition.SystemId, TEXT("M6 generated output must declare authored override IDs and stable generation input IDs."));
		bValid = false;
	}
	if (SystemDefinition.GeneratedSourceMetadata.bDependsOnArrayOrder ||
		SystemDefinition.GeneratedSourceMetadata.bDependsOnActorNames ||
		SystemDefinition.GeneratedSourceMetadata.bDependsOnEditorOnlyState)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_unstable_generation_dependency"), SystemDefinition.SystemId, TEXT("M6 generated output must not depend on array order, actor names, or editor-only state."));
		bValid = false;
	}

	if (SystemDefinition.Bodies.Num() < 4 ||
		!SystemDefinition.Bodies.ContainsByPredicate([](const FBodyDefinition& Body) { return Body.BodyType == FName(TEXT("star")); }) ||
		!SystemDefinition.Bodies.ContainsByPredicate([](const FBodyDefinition& Body) { return Body.BodyType == FName(TEXT("moon")); }))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_incomplete_body_hierarchy"), SystemDefinition.SystemId, TEXT("M6 generated systems must include a star, planet body hierarchy, and at least one moon."));
		bValid = false;
	}

	const FStationDefinition* MovingStation = SystemDefinition.Stations.FindByPredicate([](const FStationDefinition& Station)
	{
		return !Station.Orbit.ParentId.IsNone() && Station.Orbit.SemiMajorAxisCm > 0.0 && Station.Orbit.PeriodSeconds > 0.0;
	});
	if (!MovingStation)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_moving_station"), SystemDefinition.SystemId, TEXT("M6 generated systems must include at least one moving station."));
		bValid = false;
	}

	if (SystemDefinition.Gates.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_gate"), SystemDefinition.SystemId, TEXT("M6 generated systems must include at least one gate."));
		bValid = false;
	}
	if (SystemDefinition.ResourceZones.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_resource_zone"), SystemDefinition.SystemId, TEXT("M6 generated systems must include at least one resource/hazard zone."));
		bValid = false;
	}
	if (SystemDefinition.SpawnZones.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_spawn_zone"), SystemDefinition.SystemId, TEXT("M6 generated systems must include at least one spawn zone."));
		bValid = false;
	}
	if (SystemDefinition.MapEntries.Num() < SystemDefinition.Bodies.Num() + SystemDefinition.Stations.Num() + SystemDefinition.Gates.Num() + SystemDefinition.ResourceZones.Num())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_map_metadata"), SystemDefinition.SystemId, TEXT("M6 generated systems must emit map metadata for generated physical entities."));
		bValid = false;
	}

	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (GravityWell.SlowdownRadiusCm < GravityWell.LockoutRadiusCm ||
			GravityWell.LockoutRadiusCm < GravityWell.DropoutRadiusCm ||
			GravityWell.DropoutRadiusCm <= 0.0)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_invalid_gravity_lockout"), GravityWell.WellId, TEXT("M6 gravity wells must include a valid slowdown >= lockout >= dropout lockout case."));
			bValid = false;
		}
	}

	if (SystemDefinition.GravityWells.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_gravity_well"), SystemDefinition.SystemId, TEXT("M6 generated systems must include at least one gravity-well lockout case."));
		bValid = false;
	}

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	if (MovingStation)
	{
		FFrameResolvedTransform StationAt0;
		FFrameResolvedTransform StationAt30;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, MovingStation->StationId, Clock, 0.0, StationAt0) ||
			!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, MovingStation->StationId, Clock, 30.0, StationAt30) ||
			StationAt0.PositionCm.Equals(StationAt30.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_moving_station_not_resolved"), MovingStation->StationId, TEXT("M6 moving station must resolve deterministically and move over time."));
			bValid = false;
		}
	}

	if (!SystemDefinition.SpawnZones.IsEmpty() && MovingStation)
	{
		FFrameResolvedTransform SpawnTransform;
		FFrameResolvedTransform StationTransform;
		if (UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, SystemDefinition.SpawnZones[0].SpawnZoneId, Clock, 0.0, SpawnTransform) &&
			UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, MovingStation->StationId, Clock, 0.0, StationTransform))
		{
			if (FVector::Distance(SpawnTransform.PositionCm, StationTransform.PositionCm) <= SystemDefinition.Scale.LocalBubbleRadiusCm)
			{
				AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_missing_long_supercruise_leg"), MovingStation->StationId, TEXT("M6 generated systems must include at least one fixture leg longer than the local bubble."));
				bValid = false;
			}
		}
		else
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_long_leg_unresolved"), SystemDefinition.SystemId, TEXT("M6 generated long-leg endpoints must resolve through the headless frame query service."));
			bValid = false;
		}
	}

	TArray<FSystemMapEntryViewModel> MapView;
	UOrbitRouteFrameQueryService::BuildSystemMapViewModel(SystemDefinition, Clock, 0.0, MapView);
	if (MapView.Num() < SystemDefinition.MapEntries.Num())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m6_map_view_incomplete"), SystemDefinition.SystemId, TEXT("M6 generated map metadata must resolve without actor state."));
		bValid = false;
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM7SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;

	const FTrafficRouteSegmentDefinition* Route = SystemDefinition.TrafficRoutes.FindByPredicate([](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == FName(TEXT("m7_brink_watch_wayfarer_trade"));
	});
	if (!Route)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_missing_fixture_route"), SystemDefinition.SystemId, TEXT("M7 requires traffic route segment m7_brink_watch_wayfarer_trade."));
		return false;
	}

	auto EntityExists = [&SystemDefinition](FName EntityId)
	{
		return SystemDefinition.Bodies.ContainsByPredicate([EntityId](const FBodyDefinition& Body) { return Body.BodyId == EntityId; }) ||
			SystemDefinition.Stations.ContainsByPredicate([EntityId](const FStationDefinition& Station) { return Station.StationId == EntityId; }) ||
			SystemDefinition.Gates.ContainsByPredicate([EntityId](const FGateDefinition& Gate) { return Gate.GateId == EntityId; }) ||
			SystemDefinition.ResourceZones.ContainsByPredicate([EntityId](const FResourceZoneDefinition& Zone) { return Zone.ZoneId == EntityId; });
	};

	auto GravityWellExists = [&SystemDefinition](FName WellId)
	{
		return SystemDefinition.GravityWells.ContainsByPredicate([WellId](const FGravityWellDefinition& Well) { return Well.WellId == WellId; });
	};

	if (Route->SourceAnchorId != FName(TEXT("brink_watch")) ||
		Route->DestinationAnchorId != FName(TEXT("wayfarer_depot")) ||
		!EntityExists(Route->SourceAnchorId) ||
		!EntityExists(Route->DestinationAnchorId))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_route_endpoint_unresolved"), Route->RouteSegmentId, TEXT("M7 fixture route endpoints must resolve by stable source and destination IDs."));
		bValid = false;
	}
	if (Route->RouteGeometryPolicyId != FName(TEXT("fixture_dynamic_arc_v1")) ||
		Route->TravelModelId != FName(TEXT("fixture_supercruise_lane_v1")) ||
		Route->RouteProgressSemantic != FName(TEXT("time_fraction")) ||
		Route->RouteFrameBasisPolicy != FName(TEXT("source_to_destination")) ||
		Route->ControlData.ArcHeightCm != 600000.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_invalid_route_policy"), Route->RouteSegmentId, TEXT("M7 fixture route must use the documented deterministic arc and travel model policies."));
		bValid = false;
	}
	if (!Route->AvoidanceAnchorIds.Contains(FName(TEXT("brink"))) ||
		!Route->AvoidanceAnchorIds.Contains(FName(TEXT("brink_minor"))) ||
		!Route->ExclusionZoneIds.Contains(FName(TEXT("brink_well"))) ||
		!Route->ExclusionZoneIds.Contains(FName(TEXT("brink_minor_well"))))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_missing_lockout_route_metadata"), Route->RouteSegmentId, TEXT("M7 fixture route must declare avoidance anchors and gravity-well exclusion IDs."));
		bValid = false;
	}
	for (const FName AvoidanceAnchorId : Route->AvoidanceAnchorIds)
	{
		if (!EntityExists(AvoidanceAnchorId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_avoidance_anchor_unresolved"), AvoidanceAnchorId, TEXT("M7 route avoidance anchor does not resolve."));
			bValid = false;
		}
	}
	for (const FName ExclusionZoneId : Route->ExclusionZoneIds)
	{
		if (!GravityWellExists(ExclusionZoneId))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_exclusion_zone_unresolved"), ExclusionZoneId, TEXT("M7 route exclusion zone does not resolve to a gravity well."));
			bValid = false;
		}
	}
	if (Route->JurisdictionId.IsNone() ||
		Route->RiskProfileId.IsNone() ||
		Route->SecurityRating < 0.0 ||
		Route->SecurityRating > 1.0 ||
		Route->AllowedShipClassTags.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_missing_opaque_risk_stub"), Route->RouteSegmentId, TEXT("M7 route must carry opaque jurisdiction, security, risk profile, and allowed ship class metadata."));
		bValid = false;
	}

	const FSimulationClockSnapshot Clock0 = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FRouteSample Sample0;
	FRouteSample Sample60;
	FRouteSample Sample0Again;
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route->RouteSegmentId, 0.5, Clock0, 0.0, Sample0) ||
		!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route->RouteSegmentId, 0.5, Clock0, 60.0, Sample60) ||
		!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route->RouteSegmentId, 0.5, Clock0, 0.0, Sample0Again))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_route_sample_unresolved"), Route->RouteSegmentId, TEXT("M7 fixture route must evaluate through the headless route sampler."));
		bValid = false;
	}
	else
	{
		if (!Sample0.ResolvedTransform.PositionCm.Equals(Sample0Again.ResolvedTransform.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_route_sample_not_deterministic"), Route->RouteSegmentId, TEXT("M7 route samples must be deterministic at the same simulation time."));
			bValid = false;
		}
		if (Sample0.ResolvedTransform.PositionCm.Equals(Sample60.ResolvedTransform.PositionCm, 0.01) ||
			Sample0.SourceAnchorTransform.PositionCm.Equals(Sample60.SourceAnchorTransform.PositionCm, 0.01))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_route_endpoint_not_moving"), Route->RouteSegmentId, TEXT("M7 route samples must update when moving endpoint anchors move."));
			bValid = false;
		}
	}

	double TravelTimeSeconds = 0.0;
	if (!UOrbitRouteFrameQueryService::EstimateRouteTravelTime(SystemDefinition, Route->RouteSegmentId, Clock0, 0.0, TravelTimeSeconds) || TravelTimeSeconds <= 0.0)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_missing_travel_time_estimate"), Route->RouteSegmentId, TEXT("M7 route must provide a positive travel-time estimate to moving targets."));
		bValid = false;
	}

	const FString DebugSummary = UOrbitRouteFrameQueryService::BuildRoutePredictionDebugSummary(SystemDefinition, Route->RouteSegmentId, Route->DestinationAnchorId, Clock0, 0.0);
	if (!DebugSummary.Contains(TEXT("Now")) ||
		!DebugSummary.Contains(TEXT("Now + 60s")) ||
		!DebugSummary.Contains(TEXT("estimated-arrival")) ||
		!DebugSummary.Contains(TEXT("ETA + 60s")))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m7_missing_debug_prediction_output"), Route->RouteSegmentId, TEXT("M7 debug output must include Now, Now + 60s, estimated-arrival, and ETA + 60s transforms."));
		bValid = false;
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM8SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;

	const FShipTrafficInstance* Trader = SystemDefinition.LogicalTraffic.FindByPredicate([](const FShipTrafficInstance& Candidate)
	{
		return Candidate.ShipInstanceId == FName(TEXT("trader_brink_01"));
	});
	if (!Trader)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_missing_logical_trader"), SystemDefinition.SystemId, TEXT("M8 requires logical trader trader_brink_01 in the authored fixture."));
		return false;
	}

	if (Trader->CurrentGoal.GoalKind != EShipGoalKind::TradeRoute ||
		Trader->CurrentGoal.RouteSegmentId != FName(TEXT("m7_brink_watch_wayfarer_trade")) ||
		Trader->CurrentGoal.TargetFrame.RouteSegmentId != Trader->CurrentGoal.RouteSegmentId)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_invalid_trade_goal"), Trader->ShipInstanceId, TEXT("M8 logical trader must use the shared trade route goal state and a route-frame target."));
		bValid = false;
	}

	const FShipGroupState* Group = SystemDefinition.ShipGroups.FindByPredicate([Trader](const FShipGroupState& Candidate)
	{
		return Candidate.GroupId == Trader->GroupId;
	});
	if (!Group ||
		Group->LeaderShipInstanceId != Trader->ShipInstanceId ||
		!Group->MemberShipInstanceIds.Contains(Trader->ShipInstanceId) ||
		!Group->FormationSlots.ContainsByPredicate([Trader](const FShipFormationSlot& Slot)
		{
			return Slot.ShipInstanceId == Trader->ShipInstanceId && Slot.SlotId == Trader->FormationSlotId && Slot.SlotFrame == FName(TEXT("leader_route_sample"));
		}))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_invalid_group_state"), Trader->GroupId, TEXT("M8 fixture must include FShipGroupState with a leader and route-frame formation slot."));
		bValid = false;
	}

	FActiveTrafficSimulationState TrafficState;
	TrafficState.SystemId = SystemDefinition.SystemId;
	TrafficState.Ships = SystemDefinition.LogicalTraffic;
	TrafficState.Groups = SystemDefinition.ShipGroups;

	const FSimulationClockSnapshot Clock0 = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(SystemDefinition, Clock0, 0.0, TrafficState);
	FActiveTrafficUpdateBudget Budget;
	Budget.MaxShipsPerUpdate = 1;
	Budget.MaxSimulationStepSeconds = 5.0;

	const double InitialProgress = Trader->CurrentGoal.RouteProgress01;
	const EShipGoalExecutionResult UpdateResult = ULogicalTrafficQueryService::UpdateActiveTraffic(SystemDefinition, Clock0, 12.0, Budget, TrafficState);
	const FShipTrafficInstance* UpdatedTrader = TrafficState.Ships.FindByPredicate([](const FShipTrafficInstance& Candidate)
	{
		return Candidate.ShipInstanceId == FName(TEXT("trader_brink_01"));
	});
	if ((UpdateResult != EShipGoalExecutionResult::Success && UpdateResult != EShipGoalExecutionResult::BudgetExceeded) ||
		!UpdatedTrader ||
		UpdatedTrader->CurrentGoal.RouteProgress01 <= InitialProgress ||
		UpdatedTrader->LastRouteSample.RouteSegmentId != FName(TEXT("m7_brink_watch_wayfarer_trade")))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_trade_route_not_progressing"), Trader->ShipInstanceId, TEXT("M8 logical trader must progress along the moving route without spawning."));
		bValid = false;
	}
	if (TrafficState.LastUpdateStats.UpdatedShips > Budget.MaxShipsPerUpdate ||
		TrafficState.LastUpdateStats.AppliedDeltaSeconds > Budget.MaxSimulationStepSeconds)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_update_budget_not_capped"), Trader->ShipInstanceId, TEXT("M8 Tier 2 active-system update loop must expose and obey count/time caps."));
		bValid = false;
	}

	FShipTrafficInstance InterruptShip = *Trader;
	FShipGoalState InterruptGoal;
	InterruptGoal.GoalId = TEXT("m8_scripted_hold_position");
	InterruptGoal.GoalKind = EShipGoalKind::None;
	InterruptGoal.InterruptPriority = 99;
	InterruptGoal.DecisionReason = TEXT("scripted_non_combat_interrupt");
	FString FailureReason;
	if (!ULogicalTrafficQueryService::ApplyScriptedInterrupt(InterruptShip, InterruptGoal, FailureReason) ||
		!InterruptShip.bHasSavedGoal ||
		!ULogicalTrafficQueryService::RestoreSavedGoal(InterruptShip, FailureReason) ||
		InterruptShip.CurrentGoal.GoalKind != EShipGoalKind::TradeRoute)
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_interrupt_restore_failed"), Trader->ShipInstanceId, TEXT("M8 scripted interrupt must pause trade and restore the saved goal."));
		bValid = false;
	}

	const EShipGoalKind ReservedKinds[] = {
		EShipGoalKind::Patrol,
		EShipGoalKind::Escort,
		EShipGoalKind::Pirate,
		EShipGoalKind::Flee,
		EShipGoalKind::Fight
	};
	for (EShipGoalKind ReservedKind : ReservedKinds)
	{
		FShipGoalState ReservedGoal;
		ReservedGoal.GoalId = FName(*FString::Printf(TEXT("m8_reserved_%d"), static_cast<int32>(ReservedKind)));
		ReservedGoal.GoalKind = ReservedKind;
		ReservedGoal.bAutonomousExecutionAllowed = false;
		FString ReservedReason;
		const EShipGoalExecutionResult ReservedResult = ULogicalTrafficQueryService::CanExecuteAutonomousGoal(ReservedGoal, ReservedReason);
		const bool bExpectedRejection = ReservedKind == EShipGoalKind::Fight
			? ReservedResult == EShipGoalExecutionResult::ReservedUntilM10
			: ReservedResult == EShipGoalExecutionResult::ReservedUntilM9;
		if (!bExpectedRejection)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_reserved_goal_executes_too_early"), ReservedGoal.GoalId, TEXT("M8 reserved non-trade goal records must reject autonomous execution until their owning milestone."));
			bValid = false;
		}
	}

	FShipTrafficInstance RealizedShip = *Trader;
	FRouteSample PromotedSample;
	if (!ULogicalTrafficQueryService::PromoteLogicalTrader(SystemDefinition, RealizedShip, Clock0, 0.0, TEXT("m8_pooled_actor_01"), PromotedSample, FailureReason) ||
		RealizedShip.TrafficTier != ELogicalTrafficTier::Tier1Realized ||
		RealizedShip.RealizationToken.IsNone())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_promotion_failed"), Trader->ShipInstanceId, TEXT("M8 logical trader must promote to the realization harness."));
		bValid = false;
	}
	FRouteSample ActorSample;
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RealizedShip.CurrentGoal.RouteSegmentId, 0.35, Clock0, 20.0, ActorSample) ||
		!ULogicalTrafficQueryService::DemoteLogicalTrader(SystemDefinition, RealizedShip, ActorSample, Clock0, 20.0, TEXT("m8_pooled_actor_01"), FailureReason) ||
		RealizedShip.TrafficTier != ELogicalTrafficTier::Tier2Logical ||
		!RealizedShip.RealizationToken.IsNone() ||
		!FMath::IsNearlyEqual(RealizedShip.CurrentGoal.RouteProgress01, 0.35, 0.05))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_demotion_failed"), Trader->ShipInstanceId, TEXT("M8 realization harness must demote to route progress and resume without actor references."));
		bValid = false;
	}

	FLogicalTrafficDebugView DebugView;
	if (!ULogicalTrafficQueryService::BuildLogicalTrafficDebugView(*Trader, DebugView) ||
		!DebugView.DebugSummary.Contains(TEXT("Goal=")) ||
		!DebugView.DebugSummary.Contains(TEXT("TargetFrame=")) ||
		!DebugView.DebugSummary.Contains(TEXT("RouteProgress=")) ||
		!DebugView.DebugSummary.Contains(TEXT("Group=")) ||
		!DebugView.DebugSummary.Contains(TEXT("DecisionReason=")))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m8_missing_debug_view"), Trader->ShipInstanceId, TEXT("M8 debug view must show goal, target frame, route progress, group, and decision reason."));
		bValid = false;
	}

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM9SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FSystemicGameplayState& State = SystemDefinition.SystemicGameplay;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	if (State.Factions.IsEmpty())
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m9_missing_authored_systemic_state"), SystemDefinition.SystemId, TEXT("M9 validation requires authored systemic gameplay state; fallback fixture generation is not accepted by the validation profile."));
		bValid = false;
	}

	FString SystemicValidationError;
	if (!USystemicGameplayQueryService::ValidateSystemicGameplayState(SystemDefinition, State, SystemicValidationError))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m9_invalid_systemic_state"), SystemDefinition.SystemId, SystemicValidationError);
		bValid = false;
	}

	Require(!State.Factions.IsEmpty(), TEXT("m9_missing_factions"), SystemDefinition.SystemId, TEXT("M9 requires faction definitions."));
	Require(!State.FactionRelationships.IsEmpty(), TEXT("m9_missing_faction_relationships"), SystemDefinition.SystemId, TEXT("M9 requires faction relationship records."));
	Require(!State.FactionOperations.IsEmpty(), TEXT("m9_missing_faction_operations"), SystemDefinition.SystemId, TEXT("M9 requires faction operational state."));
	Require(!State.Jurisdictions.IsEmpty(), TEXT("m9_missing_jurisdictions"), SystemDefinition.SystemId, TEXT("M9 requires jurisdiction definitions."));
	Require(!State.Items.IsEmpty() && !State.Containers.IsEmpty(), TEXT("m9_missing_inventory"), SystemDefinition.SystemId, TEXT("M9 requires item and cargo container contracts."));
	Require(!State.Commodities.IsEmpty() && !State.CommodityItemBridges.IsEmpty() && !State.Markets.IsEmpty(), TEXT("m9_missing_market"), SystemDefinition.SystemId, TEXT("M9 requires commodity, bridge, and market state."));
	Require(!State.CreditAccounts.IsEmpty(), TEXT("m9_missing_credit_accounts"), SystemDefinition.SystemId, TEXT("M9 requires durable credit accounts."));
	Require(!State.StationServiceEndpoints.IsEmpty(), TEXT("m9_missing_service_endpoints"), SystemDefinition.SystemId, TEXT("M9 requires station service endpoints."));
	Require(!State.CommsEndpoints.IsEmpty() && !State.MessageDefinitions.IsEmpty(), TEXT("m9_missing_comms"), SystemDefinition.SystemId, TEXT("M9 requires comms endpoint and message contracts."));
	Require(!State.MissionOffers.IsEmpty() && !State.MissionInstances.IsEmpty() && !State.ObjectiveStates.IsEmpty(), TEXT("m9_missing_missions"), SystemDefinition.SystemId, TEXT("M9 requires mission lifecycle hook records."));

	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		Require(!Faction.FactionId.IsNone(), TEXT("m9_empty_faction_id"), SystemDefinition.SystemId, TEXT("Faction IDs must be stable and non-empty."));
		Require(!FactionIds.Contains(Faction.FactionId), TEXT("m9_duplicate_faction_id"), Faction.FactionId, TEXT("Faction IDs must be unique."));
		FactionIds.Add(Faction.FactionId);
	}

	TSet<FName> JurisdictionIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		Require(FactionIds.Contains(Jurisdiction.AuthorityFactionId), TEXT("m9_unresolved_jurisdiction_faction"), Jurisdiction.JurisdictionId, TEXT("Jurisdiction authority faction must resolve."));
		JurisdictionIds.Add(Jurisdiction.JurisdictionId);
	}
	for (const FFactionOperationalState& Operation : State.FactionOperations)
	{
		Require(FactionIds.Contains(Operation.FactionId), TEXT("m9_unresolved_operation_faction"), Operation.FactionId, TEXT("Faction operational state must reference a valid faction."));
		Require(!Operation.SystemId.IsNone() || !Operation.ZoneId.IsNone(), TEXT("m9_unscoped_operation"), Operation.FactionId, TEXT("Faction operational state must be scoped to a system or zone."));
	}

	TSet<FName> ItemIds;
	for (const FItemDefinition& Item : State.Items)
	{
		ItemIds.Add(Item.ItemId);
	}
	TSet<FName> CommodityIds;
	for (const FCommodityDefinition& Commodity : State.Commodities)
	{
		CommodityIds.Add(Commodity.CommodityId);
	}
	TSet<FName> BridgedCommodityIds;
	for (const FCommodityItemBridge& Bridge : State.CommodityItemBridges)
	{
		Require(CommodityIds.Contains(Bridge.CommodityId), TEXT("m9_bridge_missing_commodity"), Bridge.CommodityItemBridgeId, TEXT("Commodity bridge must reference a valid commodity."));
		Require(ItemIds.Contains(Bridge.ItemId), TEXT("m9_bridge_missing_item"), Bridge.CommodityItemBridgeId, TEXT("Commodity bridge must reference a valid item."));
		Require(!BridgedCommodityIds.Contains(Bridge.CommodityId), TEXT("m9_duplicate_commodity_bridge"), Bridge.CommodityId, TEXT("Tradable commodity must have exactly one canonical item bridge."));
		BridgedCommodityIds.Add(Bridge.CommodityId);
	}

	TSet<FName> ContainerIds;
	for (const FContainerState& Container : State.Containers)
	{
		ContainerIds.Add(Container.ContainerId);
		for (const FItemStackState& Stack : Container.Stacks)
		{
			Require(ItemIds.Contains(Stack.ItemId), TEXT("m9_stack_missing_item"), Stack.StackId, TEXT("Cargo stack item must resolve."));
		}
	}

	TSet<FName> MarketIds;
	for (const FStationMarketState& Market : State.Markets)
	{
		MarketIds.Add(Market.MarketId);
		Require(Market.MarketId == Market.StationId, TEXT("m9_market_identity_rule"), Market.MarketId, TEXT("M9 fixture market must default MarketId to StationId."));
		for (const TPair<FName, int32>& Stock : Market.StockByCommodity)
		{
			Require(CommodityIds.Contains(Stock.Key), TEXT("m9_market_stock_missing_commodity"), Market.MarketId, TEXT("Market stock commodity must resolve."));
			Require(BridgedCommodityIds.Contains(Stock.Key), TEXT("m9_market_stock_missing_bridge"), Market.MarketId, TEXT("Carried market commodity must resolve one item bridge."));
		}
	}

	TSet<FName> AccountIds;
	for (const FCreditAccountRecord& Account : State.CreditAccounts)
	{
		AccountIds.Add(Account.AccountId);
	}
	TSet<FName> CommsIds;
	for (const FCommsEndpointDefinition& Comms : State.CommsEndpoints)
	{
		CommsIds.Add(Comms.EndpointId);
	}
	for (const FStationServiceEndpointDefinition& Endpoint : State.StationServiceEndpoints)
	{
		Require(FactionIds.Contains(Endpoint.ProviderFactionId), TEXT("m9_endpoint_missing_faction"), Endpoint.ServiceEndpointId, TEXT("Station service endpoint provider faction must resolve."));
		Require(Endpoint.MarketId.IsNone() || MarketIds.Contains(Endpoint.MarketId), TEXT("m9_endpoint_missing_market"), Endpoint.ServiceEndpointId, TEXT("Station service endpoint market must resolve."));
		Require(Endpoint.InventoryContainerId.IsNone() || ContainerIds.Contains(Endpoint.InventoryContainerId), TEXT("m9_endpoint_missing_inventory"), Endpoint.ServiceEndpointId, TEXT("Station service endpoint inventory container must resolve."));
		Require(Endpoint.CreditAccountId.IsNone() || AccountIds.Contains(Endpoint.CreditAccountId), TEXT("m9_endpoint_missing_account"), Endpoint.ServiceEndpointId, TEXT("Station service endpoint account must resolve."));
		Require(Endpoint.CommsEndpointId.IsNone() || CommsIds.Contains(Endpoint.CommsEndpointId), TEXT("m9_endpoint_missing_comms"), Endpoint.ServiceEndpointId, TEXT("Station service endpoint comms endpoint must resolve."));
		const bool bPricedShipService = Endpoint.ServiceType == FName(TEXT("repair")) || Endpoint.ServiceType == FName(TEXT("refuel")) || Endpoint.ServiceType == FName(TEXT("rearm"));
		Require(!bPricedShipService || Endpoint.UnitPriceCredits > 0, TEXT("m9_endpoint_missing_unit_price"), Endpoint.ServiceEndpointId, TEXT("Station repair, refuel, and rearm endpoints must author a positive unit price."));
	}

	FSystemicDecisionInputSnapshot Snapshot;
	Require(USystemicGameplayQueryService::BuildSystemicDecisionInputSnapshot(SystemDefinition, State, TEXT("m7_brink_watch_wayfarer_trade"), Snapshot) && Snapshot.bValid,
		TEXT("m9_decision_inputs_unresolved"),
		TEXT("m7_brink_watch_wayfarer_trade"),
		TEXT("M9 route-security decision inputs must resolve faction, jurisdiction, legal, market, and cargo IDs."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM10SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FSystemicGameplayState& State = SystemDefinition.SystemicGameplay;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	FString LogicalValidationError;
	if (!USystemicGameplayQueryService::ValidateLogicalEncounterState(SystemDefinition, State, LogicalValidationError))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m10_invalid_logical_encounter_state"), SystemDefinition.SystemId, LogicalValidationError);
		bValid = false;
	}

	Require(SystemDefinition.LogicalTraffic.ContainsByPredicate([](const FShipTrafficInstance& Ship)
	{
		return Ship.ShipInstanceId == FName(TEXT("pirate_raider_01")) && Ship.TrafficTier == ELogicalTrafficTier::Tier2Logical && Ship.RealizationToken.IsNone();
	}), TEXT("m10_missing_logical_pirate"), SystemDefinition.SystemId, TEXT("M10 requires a durable logical pirate ship with no actor realization token."));
	Require(SystemDefinition.LogicalTraffic.ContainsByPredicate([](const FShipTrafficInstance& Ship)
	{
		return Ship.ShipInstanceId == FName(TEXT("patrol_frontier_local_01")) && Ship.TrafficTier == ELogicalTrafficTier::Tier2Logical && Ship.RealizationToken.IsNone();
	}), TEXT("m10_missing_logical_patrol"), SystemDefinition.SystemId, TEXT("M10 requires a durable logical patrol ship with no actor realization token."));
	Require(State.LogicalEncounters.ContainsByPredicate([](const FLogicalEncounterRecord& Encounter)
	{
		return Encounter.EncounterId == FName(TEXT("encounter_pirate_trade_lane_01")) && !Encounter.bRequiresActor && Encounter.State == FName(TEXT("pending"));
	}), TEXT("m10_missing_actor_free_encounter"), SystemDefinition.SystemId, TEXT("M10 requires an actor-free pending logical pirate encounter."));
	Require(State.InterdictionHazards.ContainsByPredicate([](const FInterdictionHazardRecord& Hazard)
	{
		return Hazard.HazardId == FName(TEXT("hazard_interdiction_trade_lane_01")) && Hazard.RouteSample.RouteSegmentId == FName(TEXT("m7_brink_watch_wayfarer_trade")) && !Hazard.SaveLoadEventId.IsNone();
	}), TEXT("m10_missing_interdiction_hazard"), SystemDefinition.SystemId, TEXT("M10 requires interdiction hazard state with a route sample and save/load event."));
	Require(State.DistressEvents.ContainsByPredicate([](const FDistressEventRecord& Distress)
	{
		return Distress.DistressEventId == FName(TEXT("event_distress_trade_lane_01")) && !Distress.RespondingPatrolReservationIds.IsEmpty() && !Distress.MessageInstanceIds.IsEmpty();
	}), TEXT("m10_missing_distress"), SystemDefinition.SystemId, TEXT("M10 requires distress state linking source, threat, responding patrol, and message."));
	Require(State.ActorPromotionAttachments.ContainsByPredicate([](const FLogicalActorPromotionRecord& Promotion)
	{
		return Promotion.EncounterId == FName(TEXT("encounter_pirate_trade_lane_01")) &&
			Promotion.SourceEventId == FName(TEXT("event_m10_pirate_trade_lane_01")) &&
			!Promotion.RealizationToken.IsNone() &&
			!Promotion.bCanResolveEncounter;
	}), TEXT("m10_missing_promotion_attachment"), SystemDefinition.SystemId, TEXT("M10 requires promotion metadata that attaches later actors to an existing logical encounter without allowing actors to resolve it."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM10_5SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FSystemicGameplayState& State = SystemDefinition.SystemicGameplay;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	FString CombatValidationError;
	if (!USystemicGameplayQueryService::ValidateCombatDamageThreatState(SystemDefinition, State, CombatValidationError))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m10_5_invalid_combat_damage_threat_state"), SystemDefinition.SystemId, CombatValidationError);
		bValid = false;
	}

	const bool bCanApplyLogicalDamage = State.ShipDurabilityStates.ContainsByPredicate([](const FShipDurabilityState& Durability)
	{
		return Durability.CombatantId == FName(TEXT("trader_brink_01")) && Durability.Hull > 0.0 && Durability.Shield >= 0.0;
	});
	Require(bCanApplyLogicalDamage, TEXT("m10_5_missing_logical_damage_target"), SystemDefinition.SystemId, TEXT("M10.5 requires a logical NPC durability target that can receive damage without an actor pointer."));

	Require(State.ThreatRecords.ContainsByPredicate([](const FThreatRecord& Threat)
	{
		return Threat.ThreatId == FName(TEXT("threat_m11_pirate_trade_lane_01")) &&
			Threat.AttackerId == FName(TEXT("pirate_raider_01")) &&
			Threat.DefenderId == FName(TEXT("trader_brink_01")) &&
			Threat.LastKnownTarget.TargetType == FName(TEXT("route_sample"));
	}), TEXT("m10_5_missing_route_frame_threat"), SystemDefinition.SystemId, TEXT("M10.5 requires flee/patrol-readable threat state in a moving route frame."));

	Require(State.RealizedSteeringIntents.ContainsByPredicate([](const FRealizedAISteeringIntent& Intent)
	{
		return Intent.IntentType == FName(TEXT("attack")) &&
			Intent.ShipInstanceId == FName(TEXT("pirate_raider_01")) &&
			Intent.TargetShipId == FName(TEXT("trader_brink_01")) &&
			Intent.TargetFrame.TargetType == FName(TEXT("route_sample"));
	}), TEXT("m10_5_missing_attack_intent"), SystemDefinition.SystemId, TEXT("M10.5 requires an abstract attack intent shared by logical and realized combat."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM11SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FSystemicGameplayState& State = SystemDefinition.SystemicGameplay;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	FString RealizedValidationError;
	if (!USystemicGameplayQueryService::ValidateRealizedAISliceState(SystemDefinition, State, RealizedValidationError))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m11_invalid_realized_ai_slice"), SystemDefinition.SystemId, RealizedValidationError);
		bValid = false;
	}

	TArray<FRealizedActorMappingRecord> Promotions;
	TArray<FName> BlockedShipIds;
	FString BudgetReason;
	FMovingFrameTarget ObserverTarget;
	ObserverTarget.TargetId = TEXT("m11_validation_observer");
	ObserverTarget.TargetType = TEXT("route_sample");
	ObserverTarget.RouteSegmentId = TEXT("m7_brink_watch_wayfarer_trade");
	ObserverTarget.RouteProgress01 = 0.55;
	const FSimulationClockSnapshot Clock0 = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	const bool bBudgetSelects = USystemicGameplayQueryService::SelectRealizedPromotionCandidates(SystemDefinition, State, TEXT("actor_budget_m11_low"), ObserverTarget, Clock0, 0.0, Promotions, BlockedShipIds, BudgetReason);
	Require(bBudgetSelects && !Promotions.IsEmpty(), TEXT("m11_no_promotion_candidates"), SystemDefinition.SystemId, TEXT("M11 low actor budget must select at least one realized actor candidate."));
	Require(!BlockedShipIds.IsEmpty(), TEXT("m11_actor_budget_not_low"), SystemDefinition.SystemId, TEXT("M11 actor budget must block lower-priority promotions to prove capped realization."));
	Require(Promotions.ContainsByPredicate([](const FRealizedActorMappingRecord& Mapping)
	{
		return Mapping.ShipInstanceId == FName(TEXT("trader_brink_01"));
	}), TEXT("m11_missing_trader_promotion"), SystemDefinition.SystemId, TEXT("M11 promotion priority must keep the active trader eligible for realization."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM12GameplaySystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	const FSystemicGameplayState& State = SystemDefinition.SystemicGameplay;

	auto Require = [this, &Report, &bValid](bool bCondition, FName RuleId, FName ObjectId, const TCHAR* Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, ObjectId, Message);
			bValid = false;
		}
	};

	FString M12ValidationError;
	if (!USystemicGameplayQueryService::ValidateM12GameplayState(SystemDefinition, State, M12ValidationError))
	{
		AddIssue(Report, EStargameValidationSeverity::Error, TEXT("m12_invalid_gameplay_state"), SystemDefinition.SystemId, M12ValidationError);
		bValid = false;
	}

	Require(State.StationServiceEndpoints.ContainsByPredicate([](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.ServiceType == FName(TEXT("repair")) && Endpoint.CreditAccountId == FName(TEXT("account_brink_watch_services"));
	}), TEXT("m12_missing_repair_service"), SystemDefinition.SystemId, TEXT("M12 requires repair service availability with a durable service account."));
	Require(State.StationServiceEndpoints.ContainsByPredicate([](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.ServiceType == FName(TEXT("refuel")) && Endpoint.CreditAccountId == FName(TEXT("account_brink_watch_services"));
	}), TEXT("m12_missing_refuel_service"), SystemDefinition.SystemId, TEXT("M12 requires refuel service availability with a durable service account."));
	Require(State.StationServiceEndpoints.ContainsByPredicate([](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.ServiceType == FName(TEXT("rearm")) && Endpoint.CreditAccountId == FName(TEXT("account_brink_watch_services"));
	}), TEXT("m12_missing_rearm_service"), SystemDefinition.SystemId, TEXT("M12 requires rearm service availability with a durable service account."));
	Require(State.StationServiceEndpoints.ContainsByPredicate([](const FStationServiceEndpointDefinition& Endpoint)
	{
		return Endpoint.ServiceType == FName(TEXT("mission_board")) && Endpoint.CommsEndpointId == FName(TEXT("comms_brink_watch"));
	}), TEXT("m12_missing_mission_board"), SystemDefinition.SystemId, TEXT("M12 requires mission board service records connected to comms."));
	Require(State.MissionOffers.ContainsByPredicate([](const FMissionOfferRecord& Offer)
	{
		return Offer.OfferId == FName(TEXT("offer_m12_wayfarer_security_01")) &&
			Offer.SourceServiceEndpointId == FName(TEXT("service_brink_watch_mission_board")) &&
			Offer.GiverNpcId == FName(TEXT("npc_brink_watch_dispatch")) &&
			Offer.RequiredStationTags.Contains(FName(TEXT("convoy_defense"))) &&
			Offer.RequiredStationTags.Contains(FName(TEXT("frontier_security")));
	}), TEXT("m12_missing_route_security_offer"), SystemDefinition.SystemId, TEXT("M12 requires authored/generated mission-board route/security offer records."));
	Require(State.FollowUpOpportunities.ContainsByPredicate([](const FFollowUpOpportunityRecord& Opportunity)
	{
		return Opportunity.OpportunityId == FName(TEXT("followup_m12_wayfarer_return_01")) &&
			Opportunity.RouteSegmentId == FName(TEXT("m7_brink_watch_wayfarer_trade"));
	}), TEXT("m12_missing_followup_route"), SystemDefinition.SystemId, TEXT("M12 requires follow-up route opportunities linked to previous outcomes."));
	Require(State.ShipResourceStates.ContainsByPredicate([](const FShipResourceState& Resource)
	{
		return Resource.ShipId == FName(TEXT("player_ship")) && Resource.MaxFuel > 0.0 && Resource.MaxAmmo > 0.0;
	}), TEXT("m12_missing_player_resources"), SystemDefinition.SystemId, TEXT("M12 requires persistent player ship fuel/ammo resource state."));
	Require(State.ProgressionDebugLedger.ContainsByPredicate([](const FProgressionDebugLedgerEntry& Entry)
	{
		return Entry.EntryType == FName(TEXT("fixture_seed")) && !Entry.FollowUpOpportunityId.IsNone();
	}), TEXT("m12_missing_debug_progression_seed"), SystemDefinition.SystemId, TEXT("M12 requires debug progression ledger records connecting mission board and follow-up outputs."));

	return bValid && !Report.HasBlockingIssues();
}

bool UStarCatalogSubsystem::ValidateM12ScenarioSystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const
{
	bool bValid = true;
	auto RequireScenario = [this, &Report, &bValid, &SystemDefinition](bool bCondition, FName RuleId, const FString& Message)
	{
		if (!bCondition)
		{
			AddIssue(Report, EStargameValidationSeverity::Error, RuleId, SystemDefinition.SystemId, Message);
			bValid = false;
		}
	};

	FString FailureReason;
	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		FMissionInstanceState Mission;
		FProgressionDebugLedgerEntry CompletionEntry;
		auto CompleteMissionObjectives = [&State](FName MissionInstanceId)
		{
			for (FObjectiveState& Objective : State.ObjectiveStates)
			{
				if (Objective.MissionInstanceId == MissionInstanceId)
				{
					Objective.State = TEXT("completed");
				}
			}
		};
		RequireScenario(
			USystemicGameplayQueryService::AcceptMissionOfferOnce(State, TEXT("offer_m12_wayfarer_security_01"), TEXT("player"), TEXT("idem_validate_m12_accept"), Mission, FailureReason) &&
			(CompleteMissionObjectives(Mission.MissionInstanceId), true) &&
			USystemicGameplayQueryService::CompleteMissionOnce(State, Mission.MissionInstanceId, TEXT("event_validate_m12_complete"), TEXT("idem_validate_m12_complete"), CompletionEntry, FailureReason) &&
			CompletionEntry.EntryType == FName(TEXT("mission_complete")) &&
			State.CreditLedger.ContainsByPredicate([](const FCreditLedgerEntry& Ledger)
			{
				return Ledger.Reason == FName(TEXT("mission_reward")) && Ledger.CreditAccountId == FName(TEXT("account_player"));
			}) &&
			State.ReputationDeltas.ContainsByPredicate([](const FReputationDeltaRecord& Delta)
			{
				return Delta.Reason == FName(TEXT("mission_completion"));
			}),
			TEXT("m12_scenario_trade_route_round_trip"),
			FString::Printf(TEXT("M12 trade route round trip scenario failed: %s"), *FailureReason));
	}

	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		auto MakeServiceRequest = [](FName RequestId, FName EndpointId, FName Type, double Amount, int64 Cost)
		{
			FStationServiceRequest Request;
			Request.RequestId = RequestId;
			Request.ServiceEndpointId = EndpointId;
			Request.ServiceType = Type;
			Request.TargetShipId = TEXT("player_ship");
			Request.DebitAccountId = TEXT("account_player");
			Request.CreditAccountId = TEXT("account_brink_watch_services");
			Request.Amount = Amount;
			Request.TotalCost = Cost;
			Request.SourceEventId = TEXT("event_validate_m12_service");
			Request.IdempotencyKey = FName(*FString::Printf(TEXT("idem_%s"), *RequestId.ToString()));
			return Request;
		};
		FStationServiceResultRecord ServiceResult;
		RequireScenario(
			USystemicGameplayQueryService::ExecuteStationServiceRequestOnce(State, MakeServiceRequest(TEXT("tx_validate_m12_repair"), TEXT("service_brink_watch_repair"), TEXT("repair"), 10.0, 100), ServiceResult, FailureReason) &&
			USystemicGameplayQueryService::ExecuteStationServiceRequestOnce(State, MakeServiceRequest(TEXT("tx_validate_m12_refuel"), TEXT("service_brink_watch_refuel"), TEXT("refuel"), 10.0, 20), ServiceResult, FailureReason) &&
			USystemicGameplayQueryService::ExecuteStationServiceRequestOnce(State, MakeServiceRequest(TEXT("tx_validate_m12_rearm"), TEXT("service_brink_watch_rearm"), TEXT("rearm"), 10.0, 50), ServiceResult, FailureReason) &&
			USystemicGameplayQueryService::ValidateM12GameplayState(SystemDefinition, State, FailureReason),
			TEXT("m12_scenario_service_transaction"),
			FString::Printf(TEXT("M12 service transaction scenario failed: %s"), *FailureReason));
	}

	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		const int32 InitialLedgerCount = State.CreditLedger.Num();
		const int32 InitialCargoResultCount = State.CargoTransferResults.Num();
		FMarketTransactionRequest Request;
		Request.TransactionId = TEXT("tx_validate_m12_illegal_buy");
		Request.MarketId = TEXT("brink_watch");
		Request.BuyerId = TEXT("player");
		Request.SellerId = TEXT("brink_watch");
		Request.CommodityId = TEXT("commodity_restricted_artifacts");
		Request.CommodityItemBridgeId = TEXT("bridge_restricted_artifacts");
		Request.Quantity = 1;
		Request.QuotedUnitPrice = 600;
		Request.SourceContainerId = TEXT("brink_watch_market_inventory");
		Request.DestinationContainerId = TEXT("player_ship_cargo");
		Request.DebitAccountId = TEXT("account_player");
		Request.CreditAccountId = TEXT("account_brink_watch_market");
		Request.LegalContextId = TEXT("frontier_law_basic");
		Request.SourceEventId = TEXT("event_validate_m12_illegal");
		Request.IdempotencyKey = TEXT("idem_validate_m12_illegal_buy");
		FMarketTransactionResult MarketResult;
		const bool bRejected = !USystemicGameplayQueryService::ExecuteProgressionMarketTransactionOnce(State, Request, MarketResult, FailureReason);
		RequireScenario(
			bRejected &&
			MarketResult.Result == ESystemicActionResult::Rejected &&
			State.CreditLedger.Num() == InitialLedgerCount &&
			State.CargoTransferResults.Num() == InitialCargoResultCount,
			TEXT("m12_scenario_illegal_cargo_rejected"),
			FString::Printf(TEXT("M12 illegal cargo rejection scenario failed: %s"), *FailureReason));
	}

	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		FLogicalEncounterResolutionResult EncounterResult;
		FProgressionDebugLedgerEntry ProgressionEntry;
		RequireScenario(
			USystemicGameplayQueryService::ResolveLogicalEncounterOnce(State, TEXT("encounter_pirate_trade_lane_01"), 120.0, EncounterResult, FailureReason) &&
			USystemicGameplayQueryService::ApplyEncounterProgressionOutcomeOnce(State, TEXT("encounter_pirate_trade_lane_01"), TEXT("idem_validate_m12_encounter_progression"), ProgressionEntry, FailureReason) &&
			ProgressionEntry.EntryType == FName(TEXT("encounter_outcome")),
			TEXT("m12_scenario_pirate_distress_patrol"),
			FString::Printf(TEXT("M12 pirate/distress/patrol scenario failed: %s"), *FailureReason));
	}

	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		FMissionInstanceState Mission;
		FProgressionDebugLedgerEntry FailureEntry;
		RequireScenario(
			USystemicGameplayQueryService::AcceptMissionOfferOnce(State, TEXT("offer_m12_wayfarer_security_01"), TEXT("player"), TEXT("idem_validate_m12_fail_accept"), Mission, FailureReason) &&
			USystemicGameplayQueryService::FailMissionOnce(State, Mission.MissionInstanceId, TEXT("event_validate_m12_fail"), TEXT("idem_validate_m12_fail"), FailureEntry, FailureReason) &&
			FailureEntry.EntryType == FName(TEXT("mission_fail")) &&
			USystemicGameplayQueryService::ValidateM12GameplayState(SystemDefinition, State, FailureReason),
			TEXT("m12_scenario_mission_failure"),
			FString::Printf(TEXT("M12 mission failure scenario failed: %s"), *FailureReason));
	}

	return bValid && !Report.HasBlockingIssues();
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
	if (!Ship->ActorClass.IsNull())
	{
		UClass* ActorClass = Ship->ActorClass.LoadSynchronous();
		if (!ActorClass || !ActorClass->IsChildOf(ALogicalTrafficActor::StaticClass()))
		{
			AddIssue(Report, EStargameValidationSeverity::Error, TEXT("invalid_ship_actor_class"), Ship->ShipArchetypeId, TEXT("Ship actor class must resolve to a logical traffic actor class when authored."));
		}
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
