#include "Data/StargameM1FixtureAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/FrontierTestFixtureProvider.h"
#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Flight/SpaceFlightPawn.h"
#include "Misc/Parse.h"
#include "Curves/CurveFloat.h"
#include "Space/LogicalTrafficQueryService.h"
#include "Space/SystemicGameplayQueryService.h"
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

	void SetCurveKeys(UCurveFloat* Curve, const TArray<TPair<float, float>>& Keys)
	{
		if (!Curve)
		{
			return;
		}

		Curve->FloatCurve.Reset();
		for (const TPair<float, float>& Key : Keys)
		{
			Curve->FloatCurve.AddKey(Key.Key, Key.Value);
		}
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

		auto MakeDockingPort = [](FName PortId, const TCHAR* DisplayName)
		{
			FDockingPortDefinition DockingPort;
			DockingPort.PortId = PortId;
			DockingPort.DisplayName = FText::FromString(DisplayName);
			DockingPort.ApproachTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -25000.0, 0.0));
			DockingPort.DockedTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -1200.0, 0.0));
			DockingPort.UndockTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, -6000.0, 0.0));
			DockingPort.AllowedShipClasses.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Light"), false));
			DockingPort.ActivationRangeCm = 15000.0;
			DockingPort.MaxApproachSpeedCmPerSec = 2500.0;
			DockingPort.RequiredAlignmentDegrees = 10.0;
			return DockingPort;
		};

		FStationDefinition BrinkWatch;
		BrinkWatch.StationId = TEXT("brink_watch");
		BrinkWatch.DisplayName = FText::FromString(TEXT("Brink Watch"));
		BrinkWatch.FrameType = TEXT("station_relative");
		BrinkWatch.AnchorId = TEXT("brink");
		BrinkWatch.VisualRadiusCm = 12000.0;
		BrinkWatch.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
		BrinkWatch.DockingPorts = { MakeDockingPort(TEXT("brink_watch_port_a"), TEXT("Brink Watch Port A")) };
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
		WayfarerDepot.AnchorId = TEXT("brink_minor");
		WayfarerDepot.VisualRadiusCm = 14000.0;
		WayfarerDepot.StationProfileId = FPrimaryAssetId(UStationProfileAsset::AssetType, TEXT("frontier_station_basic"));
		WayfarerDepot.DockingPorts = { MakeDockingPort(TEXT("wayfarer_depot_port_a"), TEXT("Wayfarer Depot Port A")) };
		WayfarerDepot.NavigationTarget = MakeNavigationTarget(TEXT("wayfarer_depot"), TEXT("Wayfarer Depot"), TEXT("station"));
		WayfarerDepot.NavigationTarget.FrameType = WayfarerDepot.FrameType;
		WayfarerDepot.NavigationTarget.AnchorId = WayfarerDepot.AnchorId;
		WayfarerDepot.Orbit.ParentId = TEXT("brink_minor");
		WayfarerDepot.Orbit.SemiMajorAxisCm = 1800000.0;
		WayfarerDepot.Orbit.PeriodSeconds = 900.0;
		WayfarerDepot.Orbit.PhaseOffsetRadians = 3.0;
		SystemDefinition.Stations.Add(WayfarerDepot);

		if (SystemDefinition.Gates.Num() > 0)
		{
			SystemDefinition.Gates[0].GateProfileId = FPrimaryAssetId(UGateProfileAsset::AssetType, TEXT("frontier_gate_basic"));
			SystemDefinition.Gates[0].NavigationTarget = MakeNavigationTarget(TEXT("frontier_gate_a"), TEXT("Frontier Gate A"), TEXT("gate"));
		}

		FGravityWellDefinition EmberWell;
		EmberWell.WellId = TEXT("ember_well");
		EmberWell.AnchorBodyId = TEXT("ember");
		EmberWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
		EmberWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
		EmberWell.DropoutRadiusCm = 350000.0;
		EmberWell.Strength = 0.55;

		FGravityWellDefinition BrinkWell;
		BrinkWell.WellId = TEXT("brink_well");
		BrinkWell.AnchorBodyId = TEXT("brink");
		BrinkWell.SlowdownRadiusCm = SystemDefinition.Scale.GravitySlowdownRadiusCm;
		BrinkWell.LockoutRadiusCm = SystemDefinition.Scale.GravityLockoutRadiusCm;
		BrinkWell.DropoutRadiusCm = 350000.0;
		BrinkWell.Strength = 0.75;

		FGravityWellDefinition BrinkMinorWell;
		BrinkMinorWell.WellId = TEXT("brink_minor_well");
		BrinkMinorWell.AnchorBodyId = TEXT("brink_minor");
		BrinkMinorWell.SlowdownRadiusCm = 900000.0;
		BrinkMinorWell.LockoutRadiusCm = 250000.0;
		BrinkMinorWell.DropoutRadiusCm = 175000.0;
		BrinkMinorWell.Strength = 0.35;
		SystemDefinition.GravityWells = { EmberWell, BrinkWell, BrinkMinorWell };

		FTrafficRouteSegmentDefinition TradeRoute;
		TradeRoute.RouteSegmentId = TEXT("m7_brink_watch_wayfarer_trade");
		TradeRoute.SourceAnchorId = TEXT("brink_watch");
		TradeRoute.DestinationAnchorId = TEXT("wayfarer_depot");
		TradeRoute.SourceLocalOffsetCm = FVector(0.0, -6000.0, 0.0);
		TradeRoute.DestinationLocalOffsetCm = FVector(0.0, 8000.0, 0.0);
		TradeRoute.RoutePolicyId = TEXT("fixture_trade_lane_basic");
		TradeRoute.RouteGeometryPolicyId = TEXT("fixture_dynamic_arc_v1");
		TradeRoute.TravelModelId = TEXT("fixture_supercruise_lane_v1");
		TradeRoute.RouteProgressSemantic = TEXT("time_fraction");
		TradeRoute.AvoidanceAnchorIds = { TEXT("brink"), TEXT("brink_minor") };
		TradeRoute.ExclusionZoneIds = { TEXT("brink_well"), TEXT("brink_minor_well") };
		TradeRoute.RouteFrameBasisPolicy = TEXT("source_to_destination");
		TradeRoute.ControlData.ArcHeightCm = 600000.0;
		TradeRoute.ControlData.ArcNormalHint = FVector::UpVector;
		TradeRoute.JurisdictionId = TEXT("frontier_local_authority");
		TradeRoute.SecurityRating = 0.45;
		TradeRoute.RouteValue = 0.65;
		TradeRoute.AllowedShipClassTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Small"), false));
		TradeRoute.AllowedShipClassTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.ShipClass.Medium"), false));
		TradeRoute.RiskProfileId = TEXT("fixture_mixed_patrol_pirate_risk");
		TradeRoute.bSupportsPatrolCoverage = true;
		TradeRoute.bSupportsPirateAmbush = true;
		SystemDefinition.TrafficRoutes = { TradeRoute };

		const FActiveTrafficSimulationState M8TrafficState = ULogicalTrafficQueryService::MakeM8FixtureTrafficState(SystemDefinition, 0.0);
		SystemDefinition.LogicalTraffic = M8TrafficState.Ships;
		SystemDefinition.ShipGroups = M8TrafficState.Groups;
		if (FShipGroupState* TraderGroup = SystemDefinition.ShipGroups.FindByPredicate([](const FShipGroupState& Candidate)
		{
			return Candidate.GroupId == FName(TEXT("m8_brink_trade_group"));
		}))
		{
			TraderGroup->GroupId = TEXT("group_traders_m7_trade_lane");
			TraderGroup->LeaderShipInstanceId = TEXT("trader_brink_01");
			TraderGroup->MemberShipInstanceIds.Reset();
			TraderGroup->FormationSlots.Reset();

			SystemDefinition.LogicalTraffic[0].GroupId = TraderGroup->GroupId;
			TraderGroup->MemberShipInstanceIds.Add(SystemDefinition.LogicalTraffic[0].ShipInstanceId);
			FShipFormationSlot LeadSlot;
			LeadSlot.SlotId = SystemDefinition.LogicalTraffic[0].FormationSlotId;
			LeadSlot.ShipInstanceId = SystemDefinition.LogicalTraffic[0].ShipInstanceId;
			LeadSlot.SlotFrame = TEXT("leader_route_sample");
			TraderGroup->FormationSlots.Add(LeadSlot);

			FShipTrafficInstance Trader2 = SystemDefinition.LogicalTraffic[0];
			Trader2.ShipInstanceId = TEXT("trader_brink_02");
			Trader2.FormationSlotId = TEXT("slot_trade_wing_01");
			Trader2.CurrentGoal.GoalId = TEXT("m11_trade_route_brink_02");
			Trader2.CurrentGoal.RouteProgress01 = 0.12;
			Trader2.CurrentGoal.TargetFrame.RouteProgress01 = 0.12;
			Trader2.LastDecisionReason = TEXT("m11_actor_cap_trade_offset");
			SystemDefinition.LogicalTraffic.Add(Trader2);
			TraderGroup->MemberShipInstanceIds.Add(Trader2.ShipInstanceId);
			FShipFormationSlot Trader2Slot;
			Trader2Slot.SlotId = Trader2.FormationSlotId;
			Trader2Slot.ShipInstanceId = Trader2.ShipInstanceId;
			Trader2Slot.SlotFrame = TEXT("leader_route_sample");
			Trader2Slot.LocalOffsetCm = FVector(-2500.0, -8000.0, 0.0);
			Trader2Slot.RouteProgressOffset = -0.02;
			TraderGroup->FormationSlots.Add(Trader2Slot);

			FShipTrafficInstance ReverseTrader = SystemDefinition.LogicalTraffic[0];
			ReverseTrader.ShipInstanceId = TEXT("trader_wayfarer_01");
			ReverseTrader.FormationSlotId = TEXT("slot_trade_reverse_01");
			ReverseTrader.CurrentGoal.GoalId = TEXT("m11_trade_route_wayfarer_01");
			ReverseTrader.CurrentGoal.RouteProgress01 = 0.88;
			ReverseTrader.CurrentGoal.TargetFrame.RouteProgress01 = 0.88;
			ReverseTrader.CurrentGoal.DesiredRouteSpeedFractionPerSecond = -0.018;
			ReverseTrader.LastDecisionReason = TEXT("m11_reverse_trade_route");
			SystemDefinition.LogicalTraffic.Add(ReverseTrader);
			TraderGroup->MemberShipInstanceIds.Add(ReverseTrader.ShipInstanceId);
			FShipFormationSlot ReverseSlot;
			ReverseSlot.SlotId = ReverseTrader.FormationSlotId;
			ReverseSlot.ShipInstanceId = ReverseTrader.ShipInstanceId;
			ReverseSlot.SlotFrame = TEXT("leader_route_sample");
			ReverseSlot.LocalOffsetCm = FVector(2500.0, 9000.0, 0.0);
			ReverseSlot.RouteProgressOffset = 0.03;
			TraderGroup->FormationSlots.Add(ReverseSlot);
		}

		FShipGoalState PirateGoal;
		PirateGoal.GoalId = TEXT("m10_pirate_ambush_trade_lane");
		PirateGoal.GoalKind = EShipGoalKind::Pirate;
		PirateGoal.RouteSegmentId = TradeRoute.RouteSegmentId;
		PirateGoal.RouteProgress01 = 0.55;
		PirateGoal.TargetFrame.TargetId = TradeRoute.RouteSegmentId;
		PirateGoal.TargetFrame.TargetType = TEXT("route_sample");
		PirateGoal.TargetFrame.RouteSegmentId = TradeRoute.RouteSegmentId;
		PirateGoal.TargetFrame.RouteProgress01 = 0.55;
		PirateGoal.bAutonomousExecutionAllowed = false;
		PirateGoal.DecisionReason = TEXT("m10_logical_pirate_ambush_policy");

		FShipTrafficInstance PirateShip;
		PirateShip.ShipInstanceId = TEXT("pirate_raider_01");
		PirateShip.SystemId = SystemDefinition.SystemId;
		PirateShip.ShipArchetypeId = TEXT("wayfarer_mk1");
		PirateShip.CurrentGoal = PirateGoal;
		PirateShip.GroupId = TEXT("pirate_group_01");
		PirateShip.FormationSlotId = TEXT("slot_pirate_lead");
		PirateShip.LogicalLocation = PirateGoal.TargetFrame;
		PirateShip.LastDecisionReason = PirateGoal.DecisionReason;
		PirateShip.TrafficTier = ELogicalTrafficTier::Tier2Logical;
		SystemDefinition.LogicalTraffic.Add(PirateShip);

		FShipTrafficInstance PirateWing = PirateShip;
		PirateWing.ShipInstanceId = TEXT("pirate_raider_02");
		PirateWing.FormationSlotId = TEXT("slot_pirate_wing");
		PirateWing.CurrentGoal.GoalId = TEXT("m11_pirate_ambush_wing_trade_lane");
		PirateWing.CurrentGoal.RouteProgress01 = 0.57;
		PirateWing.CurrentGoal.TargetFrame.RouteProgress01 = 0.57;
		PirateWing.LastDecisionReason = TEXT("m11_pirate_group_wing");
		SystemDefinition.LogicalTraffic.Add(PirateWing);

		FShipGoalState PatrolGoal;
		PatrolGoal.GoalId = TEXT("m10_patrol_cover_trade_lane");
		PatrolGoal.GoalKind = EShipGoalKind::Patrol;
		PatrolGoal.RouteSegmentId = TradeRoute.RouteSegmentId;
		PatrolGoal.RouteProgress01 = 0.25;
		PatrolGoal.TargetFrame.TargetId = TradeRoute.RouteSegmentId;
		PatrolGoal.TargetFrame.TargetType = TEXT("route_sample");
		PatrolGoal.TargetFrame.RouteSegmentId = TradeRoute.RouteSegmentId;
		PatrolGoal.TargetFrame.RouteProgress01 = 0.25;
		PatrolGoal.bAutonomousExecutionAllowed = false;
		PatrolGoal.DecisionReason = TEXT("m10_logical_patrol_reservation_policy");

		FShipTrafficInstance PatrolShip;
		PatrolShip.ShipInstanceId = TEXT("patrol_frontier_local_01");
		PatrolShip.SystemId = SystemDefinition.SystemId;
		PatrolShip.ShipArchetypeId = TEXT("wayfarer_mk1");
		PatrolShip.CurrentGoal = PatrolGoal;
		PatrolShip.GroupId = TEXT("patrol_group_01");
		PatrolShip.FormationSlotId = TEXT("slot_patrol_lead");
		PatrolShip.LogicalLocation = PatrolGoal.TargetFrame;
		PatrolShip.LastDecisionReason = PatrolGoal.DecisionReason;
		PatrolShip.TrafficTier = ELogicalTrafficTier::Tier2Logical;
		SystemDefinition.LogicalTraffic.Add(PatrolShip);

		FShipTrafficInstance PatrolWing = PatrolShip;
		PatrolWing.ShipInstanceId = TEXT("patrol_frontier_local_02");
		PatrolWing.FormationSlotId = TEXT("slot_patrol_wing");
		PatrolWing.CurrentGoal.GoalId = TEXT("m11_patrol_wing_cover_trade_lane");
		PatrolWing.CurrentGoal.RouteProgress01 = 0.28;
		PatrolWing.CurrentGoal.TargetFrame.RouteProgress01 = 0.28;
		PatrolWing.LastDecisionReason = TEXT("m11_patrol_group_wing");
		SystemDefinition.LogicalTraffic.Add(PatrolWing);

		FShipGroupState PirateGroup;
		PirateGroup.GroupId = TEXT("pirate_group_01");
		PirateGroup.LeaderShipInstanceId = PirateShip.ShipInstanceId;
		PirateGroup.GroupRole = TEXT("pirate");
		PirateGroup.MemberShipInstanceIds = { PirateShip.ShipInstanceId, PirateWing.ShipInstanceId };
		FShipFormationSlot PirateLeadSlot;
		PirateLeadSlot.SlotId = PirateShip.FormationSlotId;
		PirateLeadSlot.ShipInstanceId = PirateShip.ShipInstanceId;
		PirateLeadSlot.SlotFrame = TEXT("leader_route_sample");
		FShipFormationSlot PirateWingSlot;
		PirateWingSlot.SlotId = PirateWing.FormationSlotId;
		PirateWingSlot.ShipInstanceId = PirateWing.ShipInstanceId;
		PirateWingSlot.SlotFrame = TEXT("leader_route_sample");
		PirateWingSlot.LocalOffsetCm = FVector(3000.0, -5000.0, 0.0);
		PirateWingSlot.RouteProgressOffset = 0.02;
		PirateGroup.FormationSlots = { PirateLeadSlot, PirateWingSlot };
		SystemDefinition.ShipGroups.Add(PirateGroup);

		FShipGroupState PatrolGroup;
		PatrolGroup.GroupId = TEXT("patrol_group_01");
		PatrolGroup.LeaderShipInstanceId = PatrolShip.ShipInstanceId;
		PatrolGroup.GroupRole = TEXT("patrol");
		PatrolGroup.MemberShipInstanceIds = { PatrolShip.ShipInstanceId, PatrolWing.ShipInstanceId };
		FShipFormationSlot PatrolLeadSlot;
		PatrolLeadSlot.SlotId = PatrolShip.FormationSlotId;
		PatrolLeadSlot.ShipInstanceId = PatrolShip.ShipInstanceId;
		PatrolLeadSlot.SlotFrame = TEXT("leader_route_sample");
		FShipFormationSlot PatrolWingSlot;
		PatrolWingSlot.SlotId = PatrolWing.FormationSlotId;
		PatrolWingSlot.ShipInstanceId = PatrolWing.ShipInstanceId;
		PatrolWingSlot.SlotFrame = TEXT("leader_route_sample");
		PatrolWingSlot.LocalOffsetCm = FVector(-3000.0, -6000.0, 0.0);
		PatrolWingSlot.RouteProgressOffset = 0.03;
		PatrolGroup.FormationSlots = { PatrolLeadSlot, PatrolWingSlot };
		SystemDefinition.ShipGroups.Add(PatrolGroup);

		SystemDefinition.SystemicGameplay = USystemicGameplayQueryService::MakeM11FixtureState(SystemDefinition);

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
	UCurveFloat* SupercruiseAccelerationCurve = CreateOrLoadAsset<UCurveFloat>(TEXT("/Game/Data/Ships/Curves/supercruise_accel_basic"), TEXT("supercruise_accel_basic"));
	SetCurveKeys(SupercruiseAccelerationCurve, {
		{ 0.0f, 0.0f },
		{ 0.35f, 0.55f },
		{ 1.0f, 1.0f }
	});
	AssetsToSave.Add(SupercruiseAccelerationCurve);

	UCurveFloat* SupercruiseDecelerationCurve = CreateOrLoadAsset<UCurveFloat>(TEXT("/Game/Data/Ships/Curves/supercruise_decel_basic"), TEXT("supercruise_decel_basic"));
	SetCurveKeys(SupercruiseDecelerationCurve, {
		{ 0.0f, 0.0f },
		{ 0.55f, 0.82f },
		{ 1.0f, 1.0f }
	});
	AssetsToSave.Add(SupercruiseDecelerationCurve);

	UCurveFloat* SupercruiseTurnResponseCurve = CreateOrLoadAsset<UCurveFloat>(TEXT("/Game/Data/Ships/Curves/supercruise_turn_basic"), TEXT("supercruise_turn_basic"));
	SetCurveKeys(SupercruiseTurnResponseCurve, {
		{ 0.0f, 1.0f },
		{ 0.5f, 0.42f },
		{ 1.0f, 0.18f }
	});
	AssetsToSave.Add(SupercruiseTurnResponseCurve);

	Movement->Definition.MovementProfileId = TEXT("wayfarer_movement_basic");
	Movement->Definition.SupportedFlightModeTags.Reset();
	Movement->Definition.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Normal"), false));
	Movement->Definition.SupportedFlightModeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Stargame.FlightMode.Supercruise"), false));
	Movement->Definition.NormalFlightTuningId = TEXT("normal_basic");
	Movement->Definition.SupercruiseTuningId = TEXT("supercruise_basic");
	Movement->Definition.MaxLocalSpeedCmPerSec = 24000.0;
	Movement->Definition.MaxAngularSpeedDegPerSec = 80.0;
	Movement->Definition.SupercruiseAccelerationCurve = SupercruiseAccelerationCurve;
	Movement->Definition.SupercruiseDecelerationCurve = SupercruiseDecelerationCurve;
	Movement->Definition.SupercruiseTurnResponseCurve = SupercruiseTurnResponseCurve;
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
	const bool bValidateM4 = ProfileString.Equals(TEXT("M4"), ESearchCase::IgnoreCase);
	const bool bValidateM5 = ProfileString.Equals(TEXT("M5"), ESearchCase::IgnoreCase);
	const bool bValidateM6 = ProfileString.Equals(TEXT("M6"), ESearchCase::IgnoreCase);
	const bool bValidateM7 = ProfileString.Equals(TEXT("M7"), ESearchCase::IgnoreCase);
	const bool bValidateM10 = ProfileString.Equals(TEXT("M10"), ESearchCase::IgnoreCase);
	const bool bValidateM11 = ProfileString.Equals(TEXT("M11"), ESearchCase::IgnoreCase);
	const bool bBuildAlias = ProfileString.Equals(TEXT("Build"), ESearchCase::IgnoreCase) ||
		ProfileString.Equals(TEXT("Cook"), ESearchCase::IgnoreCase) ||
		ProfileString.Equals(TEXT("MCP"), ESearchCase::IgnoreCase) ||
		ProfileString.Equals(TEXT("Editor"), ESearchCase::IgnoreCase);
	const bool bValidateM9 = ProfileString.Equals(TEXT("M9"), ESearchCase::IgnoreCase) || bBuildAlias;
	const bool bValidateM8 = ProfileString.Equals(TEXT("M8"), ESearchCase::IgnoreCase);
	const bool bValidateM0 = ProfileString.Equals(TEXT("M0"), ESearchCase::IgnoreCase);
	if (ProfileString.Equals(TEXT("M5.5"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Error, TEXT("Validation profile M5.5 is documented but not implemented in the current M0-M10 branch."));
		return 1;
	}
	if (!bValidateM0 && !bValidateM1 && !bValidateM2 && !bValidateM3 && !bValidateM4 && !bValidateM5 && !bValidateM6 && !bValidateM7 && !bValidateM8 && !bValidateM9 && !bValidateM10 && !bValidateM11)
	{
		UE_LOG(LogTemp, Error, TEXT("Unsupported validation profile '%s'."), *ProfileString);
		return 1;
	}

	UGameInstance* ValidationGameInstance = NewObject<UGameInstance>();
	UStarCatalogSubsystem* Catalog = NewObject<UStarCatalogSubsystem>(ValidationGameInstance);
	if (bValidateM0)
	{
		FStartProfileDefinition StartProfile;
		FStarSystemDefinition SystemDefinition;
		FString ValidationError;
		if (!FFrontierTestFixtureProvider::ResolveStartProfile(FFrontierTestFixtureProvider::DefaultStartProfileId, StartProfile) ||
			StartProfile.SystemId != FFrontierTestFixtureProvider::FrontierSystemId ||
			StartProfile.SpawnZoneId != FFrontierTestFixtureProvider::DeepSpaceSpawnZoneId ||
			!FFrontierTestFixtureProvider::ResolveSystemDefinition(SystemId, SystemDefinition) ||
			!FFrontierTestFixtureProvider::ValidateM0Fixture(SystemDefinition, ValidationError))
		{
			UE_LOG(LogTemp, Error, TEXT("M0 validation failed for system '%s': %s"), *SystemId.ToString(), *ValidationError);
			return 1;
		}

		UE_LOG(LogTemp, Display, TEXT("M0 validation passed for system '%s'."), *SystemId.ToString());
		return 0;
	}

	if (!Catalog->BuildAssetCatalogCache(false))
	{
		UE_LOG(LogTemp, Error, TEXT("%s validation could not build an Asset Manager-backed catalog."), bValidateM11 ? TEXT("M11") : (bValidateM10 ? TEXT("M10") : (bValidateM9 ? TEXT("M9") : (bValidateM8 ? TEXT("M8") : (bValidateM7 ? TEXT("M7") : (bValidateM6 ? TEXT("M6") : (bValidateM5 ? TEXT("M5") : (bValidateM4 ? TEXT("M4") : (bValidateM3 ? TEXT("M3") : (bValidateM2 ? TEXT("M2") : TEXT("M1")))))))))));
		return 1;
	}

	FStargameValidationReport Report;
	if (bValidateM6)
	{
		int32 Seed = 6001;
		FParse::Value(*Params, TEXT("Seed="), Seed);
		FStarSystemDefinition GeneratedSystem;
		if (!Catalog->GenerateSeededPhysicalSystem(Seed, GeneratedSystem))
		{
			UE_LOG(LogTemp, Error, TEXT("M6 validation could not generate a physical system for seed %d."), Seed);
			return 1;
		}
		Report = Catalog->ValidateM6GeneratedSystemDefinition(GeneratedSystem);
	}
	else
	{
		Report = bValidateM11
			? Catalog->ValidateM11Fixture(SystemId)
			: (bValidateM10
			? Catalog->ValidateM10Fixture(SystemId)
			: (bValidateM9
			? Catalog->ValidateM9Fixture(SystemId)
			: (bValidateM8
			? Catalog->ValidateM8Fixture(SystemId)
			: (bValidateM7
			? Catalog->ValidateM7Fixture(SystemId)
			: (bValidateM5
			? Catalog->ValidateM5Fixture(SystemId)
			: (bValidateM4 ? Catalog->ValidateM4Fixture(SystemId) : (bValidateM3 ? Catalog->ValidateM3Fixture(SystemId) : (bValidateM2 ? Catalog->ValidateM2Fixture(SystemId) : Catalog->ValidateM1Fixture(SystemId)))))))));
	}
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

	UE_LOG(LogTemp, Display, TEXT("%s validation passed for system '%s'."), bValidateM11 ? TEXT("M11") : (bValidateM10 ? TEXT("M10") : (bValidateM9 ? TEXT("M9") : (bValidateM8 ? TEXT("M8") : (bValidateM7 ? TEXT("M7") : (bValidateM6 ? TEXT("M6") : (bValidateM5 ? TEXT("M5") : (bValidateM4 ? TEXT("M4") : (bValidateM3 ? TEXT("M3") : (bValidateM2 ? TEXT("M2") : TEXT("M1")))))))))), *Report.SystemId.ToString());
	return 0;
}
