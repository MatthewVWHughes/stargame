#include "Data/FrontierTestFixtureProvider.h"

const FName FFrontierTestFixtureProvider::DefaultStartProfileId(TEXT("frontier_test_start"));
const FName FFrontierTestFixtureProvider::FrontierSystemId(TEXT("frontier_test_01"));
const FName FFrontierTestFixtureProvider::ArrivalSystemId(TEXT("frontier_arrival_test_01"));
const FName FFrontierTestFixtureProvider::DeepSpaceSpawnZoneId(TEXT("spawn_deep_space"));

namespace
{
	const FName SystemFrameType(TEXT("system_barycentric"));
	constexpr double GameCmPerAu = 1000000000.0;
	constexpr double SolarRadiusAu = 0.00465047;
	constexpr double EarthRadiusAu = 0.0000426348;
	constexpr double JupiterRadiusAu = 0.000477895;
	constexpr double OrbitPeriodCompression = 1000.0;

	double AuToGameCm(double Au)
	{
		return Au * GameCmPerAu;
	}

	double SolarRadiiToGameCm(double SolarRadii)
	{
		return SolarRadii * SolarRadiusAu * GameCmPerAu;
	}

	double EarthRadiiToGameCm(double EarthRadii)
	{
		return EarthRadii * EarthRadiusAu * GameCmPerAu;
	}

	double JupiterRadiiToGameCm(double JupiterRadii)
	{
		return JupiterRadii * JupiterRadiusAu * GameCmPerAu;
	}

	double DaysToGameSeconds(double Days)
	{
		return Days * 86400.0 / OrbitPeriodCompression;
	}

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

	FBodyDefinition MakeBody(FName BodyId, const TCHAR* DisplayName, FName BodyType, double RadiusCm, FName ParentId, double SemiMajorAxisCm, double PeriodSeconds, double PhaseOffsetRadians, double Eccentricity = 0.0, double InclinationDegrees = 0.0, FName StellarClass = NAME_None)
	{
		FBodyDefinition Body;
		Body.BodyId = BodyId;
		Body.DisplayName = FText::FromString(DisplayName);
		Body.BodyType = BodyType;
		Body.StellarClass = StellarClass;
		Body.FrameType = ParentId.IsNone() ? SystemFrameType : FName(TEXT("body_relative"));
		Body.AnchorId = ParentId;
		Body.VisualRadiusCm = RadiusCm;
		Body.PhysicalReferenceRadiusCm = RadiusCm;
		Body.CollisionRadiusCm = RadiusCm;
		Body.NavigationTarget = MakeNavigationTarget(BodyId, DisplayName, TEXT("body"));
		Body.NavigationTarget.FrameType = Body.FrameType;
		Body.NavigationTarget.AnchorId = ParentId;
		Body.Orbit.ParentId = ParentId;
		Body.Orbit.SemiMajorAxisCm = SemiMajorAxisCm;
		Body.Orbit.PeriodSeconds = PeriodSeconds;
		Body.Orbit.PhaseOffsetRadians = PhaseOffsetRadians;
		Body.Orbit.Eccentricity = Eccentricity;
		Body.Orbit.InclinationDegrees = InclinationDegrees;
		return Body;
	}

	FGravityWellDefinition MakeWell(FName WellId, FName AnchorBodyId, double SlowdownRadiusCm, double LockoutRadiusCm, double DropoutRadiusCm, double Strength)
	{
		FGravityWellDefinition Well;
		Well.WellId = WellId;
		Well.AnchorBodyId = AnchorBodyId;
		Well.SlowdownRadiusCm = SlowdownRadiusCm;
		Well.LockoutRadiusCm = LockoutRadiusCm;
		Well.DropoutRadiusCm = DropoutRadiusCm;
		Well.Strength = Strength;
		return Well;
	}

	void AddBodyMapEntries(FStarSystemDefinition& SystemDefinition)
	{
		SystemDefinition.MapEntries.Reset();
		for (const FBodyDefinition& Body : SystemDefinition.Bodies)
		{
			FMapEntryDefinition Entry;
			Entry.MapEntryId = Body.BodyId;
			Entry.SourceId = Body.BodyId;
			Entry.EntryType = TEXT("body");
			Entry.bVisibleInSystemMap = true;
			SystemDefinition.MapEntries.Add(Entry);
		}
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

	FSpawnZoneDefinition SpawnZone;
	SpawnZone.SpawnZoneId = DeepSpaceSpawnZoneId;
	SpawnZone.DisplayName = FText::FromString(TEXT("HD 219134 Flyaround Spawn"));
	SpawnZone.FrameType = SystemFrameType;
	SpawnZone.Transform = FTransform(FRotator(4.0, 90.0, 0.0), FVector(0.0, -40000000.0, 5000000.0));
	SpawnZone.RadiusCm = 50000.0;
	SpawnZone.AllowedContexts = { TEXT("new_session"), TEXT("reload") };

	OutSystemDefinition = FStarSystemDefinition();
	OutSystemDefinition.SystemId = FrontierSystemId;
	OutSystemDefinition.DisplayName = FText::FromString(TEXT("HD 219134 Frontier"));
	OutSystemDefinition.Seed = 219134;
	OutSystemDefinition.Scale.LocalBubbleRadiusCm = 100000000.0;
	OutSystemDefinition.Scale.OriginShiftThresholdCm = 40000000.0;
	OutSystemDefinition.Scale.MapDistanceScaleCmPerUnit = GameCmPerAu;
	OutSystemDefinition.Bodies = {
		MakeBody(TEXT("hd219134"), TEXT("HD 219134"), TEXT("star"), SolarRadiiToGameCm(0.748), NAME_None, 0.0, 0.0, 0.0, 0.0, 0.0, TEXT("K3V")),
		MakeBody(TEXT("hd219134_b"), TEXT("HD 219134 b"), TEXT("venus_like_planet"), EarthRadiiToGameCm(1.602), TEXT("hd219134"), AuToGameCm(0.03876), DaysToGameSeconds(3.092926), 0.15, 0.0, 0.6),
		MakeBody(TEXT("hd219134_c"), TEXT("HD 219134 c"), TEXT("rocky_planet"), EarthRadiiToGameCm(1.511), TEXT("hd219134"), AuToGameCm(0.0653), DaysToGameSeconds(6.76458), 1.2, 0.06, 1.1),
		MakeBody(TEXT("hd219134_f"), TEXT("HD 219134 f"), TEXT("rocky_planet"), EarthRadiiToGameCm(1.31), TEXT("hd219134"), AuToGameCm(0.1463), DaysToGameSeconds(22.717), 2.5, 0.15, 2.4),
		MakeBody(TEXT("hd219134_d"), TEXT("HD 219134 d"), TEXT("dense_rocky_planet"), EarthRadiiToGameCm(1.61), TEXT("hd219134"), AuToGameCm(0.237), DaysToGameSeconds(46.859), 3.4, 0.14, 3.0),
		MakeBody(TEXT("hd219134_g"), TEXT("HD 219134 g"), TEXT("ice_giant"), JupiterRadiiToGameCm(0.293), TEXT("hd219134"), AuToGameCm(0.3753), DaysToGameSeconds(94.2), 4.8, 0.0, 1.7),
		MakeBody(TEXT("hd219134_h"), TEXT("HD 219134 h"), TEXT("gas_giant"), JupiterRadiiToGameCm(1.14), TEXT("hd219134"), AuToGameCm(3.11), DaysToGameSeconds(2247.0), 5.6, 0.06, 4.1),
		MakeBody(TEXT("hd219134_g_i"), TEXT("HD 219134 g-i"), TEXT("moon"), EarthRadiiToGameCm(0.45), TEXT("hd219134_g"), 6000000.0, 1800.0, 2.2, 0.02, 4.0),
		MakeBody(TEXT("hd219134_g_ii"), TEXT("HD 219134 g-ii"), TEXT("moon"), EarthRadiiToGameCm(0.32), TEXT("hd219134_g"), 11000000.0, 3200.0, 5.1, 0.04, 11.0),
		MakeBody(TEXT("hd219134_h_i"), TEXT("HD 219134 h-i"), TEXT("moon"), EarthRadiiToGameCm(0.38), TEXT("hd219134_h"), 10000000.0, 2200.0, 0.8, 0.01, 2.0),
		MakeBody(TEXT("hd219134_h_ii"), TEXT("HD 219134 h-ii"), TEXT("moon"), EarthRadiiToGameCm(0.74), TEXT("hd219134_h"), 17000000.0, 4200.0, 2.8, 0.03, 6.0),
		MakeBody(TEXT("hd219134_h_iii"), TEXT("HD 219134 h-iii"), TEXT("moon"), EarthRadiiToGameCm(0.56), TEXT("hd219134_h"), 27000000.0, 7200.0, 4.1, 0.08, 14.0),
		MakeBody(TEXT("hd219134_h_iv"), TEXT("HD 219134 h-iv"), TEXT("moon"), EarthRadiiToGameCm(0.28), TEXT("hd219134_h"), 39000000.0, 11200.0, 5.6, 0.05, 9.0),
		MakeBody(TEXT("hd219134_h_v"), TEXT("HD 219134 h-v"), TEXT("moon"), EarthRadiiToGameCm(0.86), TEXT("hd219134_h"), 56000000.0, 18400.0, 1.9, 0.12, 21.0)
	};
	OutSystemDefinition.GravityWells = {
		MakeWell(TEXT("hd219134_well"), TEXT("hd219134"), 1000000000.0, 25000000.0, 12000000.0, 1.0),
		MakeWell(TEXT("hd219134_b_well"), TEXT("hd219134_b"), 3500000.0, 700000.0, 300000.0, 0.35),
		MakeWell(TEXT("hd219134_c_well"), TEXT("hd219134_c"), 3500000.0, 700000.0, 300000.0, 0.35),
		MakeWell(TEXT("hd219134_f_well"), TEXT("hd219134_f"), 3000000.0, 600000.0, 260000.0, 0.30),
		MakeWell(TEXT("hd219134_d_well"), TEXT("hd219134_d"), 4200000.0, 900000.0, 380000.0, 0.45),
		MakeWell(TEXT("hd219134_g_well"), TEXT("hd219134_g"), 30000000.0, 5000000.0, 2200000.0, 0.65),
		MakeWell(TEXT("hd219134_h_well"), TEXT("hd219134_h"), 80000000.0, 12000000.0, 6000000.0, 0.85)
	};
	OutSystemDefinition.SpawnZones = { SpawnZone };
	AddBodyMapEntries(OutSystemDefinition);
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

	if (SystemDefinition.Bodies.Num() < 14 || !SystemDefinition.Stations.IsEmpty() || !SystemDefinition.Gates.IsEmpty() || SystemDefinition.SpawnZones.Num() != 1)
	{
		OutError = TEXT("M0 fixture must contain the HD 219134 flyaround body set, no stations, no gates, and one spawn zone.");
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

	const TSet<FName> ExpectedTargets = {
		TEXT("hd219134"),
		TEXT("hd219134_b"),
		TEXT("hd219134_c"),
		TEXT("hd219134_f"),
		TEXT("hd219134_d"),
		TEXT("hd219134_g"),
		TEXT("hd219134_h")
	};
	TSet<FName> ActualTargets;
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		ActualTargets.Add(Body.NavigationTarget.TargetId);
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
