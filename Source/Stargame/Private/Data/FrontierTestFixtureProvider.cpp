#include "Data/FrontierTestFixtureProvider.h"

const FName FFrontierTestFixtureProvider::DefaultStartProfileId(TEXT("frontier_test_start"));
const FName FFrontierTestFixtureProvider::FrontierSystemId(TEXT("frontier_test_01"));
const FName FFrontierTestFixtureProvider::ArrivalSystemId(TEXT("frontier_arrival_test_01"));
const FName FFrontierTestFixtureProvider::DeepSpaceSpawnZoneId(TEXT("spawn_deep_space"));

namespace
{
	const FName SystemFrameType(TEXT("system_barycentric"));

	FNavigationTargetDefinition MakeNavigationTarget(FName TargetId, const TCHAR* DisplayName, FName TargetType)
	{
		FNavigationTargetDefinition Target;
		Target.TargetId = TargetId;
		Target.DisplayName = FText::FromString(DisplayName);
		Target.TargetType = TargetType;
		Target.FrameType = SystemFrameType;
		Target.bShowInHud = true;
		Target.bShowInSystemMap = true;
		Target.bCanTarget = true;
		return Target;
	}

	template <typename DefinitionType, typename IdGetterType>
	bool CheckUniqueIds(const TArray<DefinitionType>& Definitions, IdGetterType IdGetter, const TCHAR* Label, FString& OutError)
	{
		TSet<FName> SeenIds;
		for (const DefinitionType& Definition : Definitions)
		{
			const FName Id = IdGetter(Definition);
			if (Id.IsNone())
			{
				OutError = FString::Printf(TEXT("%s contains an empty ID."), Label);
				return false;
			}
			if (SeenIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("%s contains duplicate ID '%s'."), Label, *Id.ToString());
				return false;
			}
			SeenIds.Add(Id);
		}
		return true;
	}
}

bool FFrontierTestFixtureProvider::ResolveStartProfile(FName StartProfileId, FStartProfileDefinition& OutStartProfile)
{
	if (StartProfileId != DefaultStartProfileId)
	{
		return false;
	}

	OutStartProfile = FStartProfileDefinition();
	OutStartProfile.StartProfileId = DefaultStartProfileId;
	OutStartProfile.DisplayName = FText::FromString(TEXT("Frontier Test Start"));
	OutStartProfile.SystemId = FrontierSystemId;
	OutStartProfile.SpawnZoneId = DeepSpaceSpawnZoneId;
	return true;
}

bool FFrontierTestFixtureProvider::ResolveSystemDefinition(FName SystemId, FStarSystemDefinition& OutSystemDefinition)
{
	if (SystemId != FrontierSystemId && SystemId != ArrivalSystemId)
	{
		return false;
	}

	if (SystemId == ArrivalSystemId)
	{
		FGateDefinition ArrivalGate;
		ArrivalGate.GateId = TEXT("arrival_gate_a");
		ArrivalGate.DisplayName = FText::FromString(TEXT("Arrival Gate A"));
		ArrivalGate.FrameType = SystemFrameType;
		ArrivalGate.Transform = FTransform(FRotator(0.0, 180.0, 0.0), FVector::ZeroVector);
		ArrivalGate.VisualRadiusCm = 16000.0;
		ArrivalGate.ActivationRangeCm = 50000.0;
		ArrivalGate.NavigationTarget = MakeNavigationTarget(ArrivalGate.GateId, TEXT("Arrival Gate A"), TEXT("gate"));

		FSpawnZoneDefinition ArrivalMarker;
		ArrivalMarker.SpawnZoneId = TEXT("arrival_from_frontier_gate_a");
		ArrivalMarker.DisplayName = FText::FromString(TEXT("Arrival From Frontier Gate A"));
		ArrivalMarker.FrameType = TEXT("gate_relative");
		ArrivalMarker.AnchorId = ArrivalGate.GateId;
		ArrivalMarker.Transform = FTransform(FRotator(0.0, 180.0, 0.0), FVector(0.0, 18000.0, 0.0));
		ArrivalMarker.RadiusCm = 12000.0;
		ArrivalMarker.AllowedContexts = { TEXT("arrival"), TEXT("reload") };

		FMapEntryDefinition GateMapEntry;
		GateMapEntry.MapEntryId = ArrivalGate.GateId;
		GateMapEntry.SourceId = ArrivalGate.GateId;
		GateMapEntry.EntryType = TEXT("gate");

		OutSystemDefinition = FStarSystemDefinition();
		OutSystemDefinition.SystemId = ArrivalSystemId;
		OutSystemDefinition.DisplayName = FText::FromString(TEXT("Frontier Arrival Test 01"));
		OutSystemDefinition.Gates = { ArrivalGate };
		OutSystemDefinition.SpawnZones = { ArrivalMarker };
		OutSystemDefinition.MapEntries = { GateMapEntry };
		return true;
	}

	FBodyDefinition Body;
	Body.BodyId = TEXT("ember");
	Body.DisplayName = FText::FromString(TEXT("Ember"));
	Body.BodyType = TEXT("rocky_planet");
	Body.FrameType = SystemFrameType;
	Body.Transform = FTransform(FRotator::ZeroRotator, FVector(450000.0, -120000.0, 0.0));
	Body.VisualRadiusCm = 45000.0;
	Body.NavigationTarget = MakeNavigationTarget(Body.BodyId, TEXT("Ember"), TEXT("body"));

	FStationDefinition Station;
	Station.StationId = TEXT("brink_watch");
	Station.DisplayName = FText::FromString(TEXT("Brink Watch"));
	Station.FrameType = SystemFrameType;
	Station.Transform = FTransform(FRotator(0.0, 25.0, 0.0), FVector(230000.0, 180000.0, 25000.0));
	Station.VisualRadiusCm = 12000.0;
	Station.NavigationTarget = MakeNavigationTarget(Station.StationId, TEXT("Brink Watch"), TEXT("station"));

	FGateDefinition Gate;
	Gate.GateId = TEXT("frontier_gate_a");
	Gate.DisplayName = FText::FromString(TEXT("Frontier Gate A"));
	Gate.FrameType = SystemFrameType;
	Gate.Transform = FTransform(FRotator(0.0, -35.0, 0.0), FVector(-360000.0, -210000.0, 10000.0));
	Gate.VisualRadiusCm = 16000.0;
	Gate.ActivationRangeCm = 50000.0;
	Gate.DestinationSystemId = ArrivalSystemId;
	Gate.DestinationGateId = TEXT("arrival_gate_a");
	Gate.DestinationArrivalId = TEXT("arrival_from_frontier_gate_a");
	Gate.DestinationArrivalFrame = TEXT("gate_relative");
	Gate.DestinationArrivalLocalOffsetCm = FVector(0.0, 18000.0, 0.0);
	Gate.DestinationArrivalRotation = FRotator(0.0, 180.0, 0.0);
	Gate.NavigationTarget = MakeNavigationTarget(Gate.GateId, TEXT("Frontier Gate A"), TEXT("gate"));

	FSpawnZoneDefinition SpawnZone;
	SpawnZone.SpawnZoneId = DeepSpaceSpawnZoneId;
	SpawnZone.DisplayName = FText::FromString(TEXT("Deep Space Spawn"));
	SpawnZone.FrameType = SystemFrameType;
	SpawnZone.Transform = FTransform(FRotator(0.0, 18.0, 0.0), FVector(0.0, -320000.0, 40000.0));
	SpawnZone.RadiusCm = 15000.0;
	SpawnZone.AllowedContexts = { TEXT("new_session"), TEXT("reload") };

	OutSystemDefinition = FStarSystemDefinition();
	OutSystemDefinition.SystemId = FrontierSystemId;
	OutSystemDefinition.DisplayName = FText::FromString(TEXT("Frontier Test 01"));
	OutSystemDefinition.Bodies = { Body };
	OutSystemDefinition.Stations = { Station };
	OutSystemDefinition.Gates = { Gate };
	OutSystemDefinition.SpawnZones = { SpawnZone };
	return true;
}

bool FFrontierTestFixtureProvider::ValidateM0Fixture(const FStarSystemDefinition& SystemDefinition, FString& OutError)
{
	if (SystemDefinition.SystemId != FrontierSystemId)
	{
		OutError = FString::Printf(TEXT("Expected system '%s', got '%s'."),
			*FrontierSystemId.ToString(),
			*SystemDefinition.SystemId.ToString());
		return false;
	}

	if (SystemDefinition.Bodies.Num() != 1 || SystemDefinition.Stations.Num() != 1 || SystemDefinition.Gates.Num() != 1 || SystemDefinition.SpawnZones.Num() != 1)
	{
		OutError = TEXT("M0 fixture must contain exactly one body, one station, one gate, and one spawn zone.");
		return false;
	}

	if (!CheckUniqueIds(SystemDefinition.Bodies, [](const FBodyDefinition& Body) { return Body.BodyId; }, TEXT("Bodies"), OutError) ||
		!CheckUniqueIds(SystemDefinition.Stations, [](const FStationDefinition& Station) { return Station.StationId; }, TEXT("Stations"), OutError) ||
		!CheckUniqueIds(SystemDefinition.Gates, [](const FGateDefinition& Gate) { return Gate.GateId; }, TEXT("Gates"), OutError) ||
		!CheckUniqueIds(SystemDefinition.SpawnZones, [](const FSpawnZoneDefinition& SpawnZone) { return SpawnZone.SpawnZoneId; }, TEXT("Spawn zones"), OutError))
	{
		return false;
	}

	if (SystemDefinition.SpawnZones[0].SpawnZoneId != DeepSpaceSpawnZoneId)
	{
		OutError = FString::Printf(TEXT("M0 fixture missing spawn zone '%s'."), *DeepSpaceSpawnZoneId.ToString());
		return false;
	}

	const TSet<FName> ExpectedTargets = { TEXT("ember"), TEXT("brink_watch"), TEXT("frontier_gate_a") };
	TSet<FName> ActualTargets;
	ActualTargets.Add(SystemDefinition.Bodies[0].NavigationTarget.TargetId);
	ActualTargets.Add(SystemDefinition.Stations[0].NavigationTarget.TargetId);
	ActualTargets.Add(SystemDefinition.Gates[0].NavigationTarget.TargetId);

	if (ActualTargets.Num() != ExpectedTargets.Num())
	{
		OutError = TEXT("M0 fixture target IDs are not unique.");
		return false;
	}

	for (const FName ExpectedTarget : ExpectedTargets)
	{
		if (!ActualTargets.Contains(ExpectedTarget))
		{
			OutError = FString::Printf(TEXT("M0 fixture missing target '%s'."), *ExpectedTarget.ToString());
			return false;
		}
	}

	return true;
}
