#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"

class STARGAME_API FFrontierTestFixtureProvider
{
public:
	static const FName DefaultStartProfileId;
	static const FName FrontierSystemId;
	static const FName ArrivalSystemId;
	static const FName DeepSpaceSpawnZoneId;

	static bool ResolveStartProfile(FName StartProfileId, FStartProfileDefinition& OutStartProfile);
	static bool ResolveSystemDefinition(FName SystemId, FStarSystemDefinition& OutSystemDefinition);
	static bool ValidateM0Fixture(const FStarSystemDefinition& SystemDefinition, FString& OutError);
};
