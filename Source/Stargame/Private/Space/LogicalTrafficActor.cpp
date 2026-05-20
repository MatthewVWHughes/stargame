#include "Space/LogicalTrafficActor.h"

#include "Components/SceneComponent.h"
#include "Data/StargameDataTypes.h"

namespace
{
	void AddActorTagIfSet(AActor& Actor, FName Tag)
	{
		if (!Tag.IsNone())
		{
			Actor.Tags.AddUnique(Tag);
		}
	}
}

ALogicalTrafficActor::ALogicalTrafficActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ALogicalTrafficActor::ConfigureTrafficActor(FName InShipInstanceId, FName InGroupId, EShipGoalKind InGoalKind)
{
	Tags.Remove(ShipInstanceId);
	Tags.Remove(GroupId);
	ShipInstanceId = InShipInstanceId;
	GroupId = InGroupId;
	GoalKind = InGoalKind;
	AddActorTagIfSet(*this, ShipInstanceId);
	AddActorTagIfSet(*this, GroupId);
	OnTrafficActorConfigured(ShipInstanceId, GroupId, GoalKind);
}

void ALogicalTrafficActor::ConfigureEncounterBehavior(FName InEncounterId, FName InIntentId, FName InIntentType, FName InThreatId, FName InTargetShipId, FName InBehaviorVariantId, FName InCommsVariantId, FName InLocalBehaviorStateId, const FString& InCommsLine)
{
	Tags.Remove(EncounterId);
	Tags.Remove(IntentType);
	Tags.Remove(BehaviorVariantId);
	Tags.Remove(CommsVariantId);
	Tags.Remove(LocalBehaviorStateId);
	EncounterId = InEncounterId;
	IntentId = InIntentId;
	IntentType = InIntentType;
	ThreatId = InThreatId;
	TargetShipId = InTargetShipId;
	BehaviorVariantId = InBehaviorVariantId;
	CommsVariantId = InCommsVariantId;
	LocalBehaviorStateId = InLocalBehaviorStateId;
	CommsLine = InCommsLine;
	AddActorTagIfSet(*this, EncounterId);
	AddActorTagIfSet(*this, IntentType);
	AddActorTagIfSet(*this, BehaviorVariantId);
	AddActorTagIfSet(*this, CommsVariantId);
	AddActorTagIfSet(*this, LocalBehaviorStateId);
	OnEncounterBehaviorConfigured(EncounterId, IntentId, IntentType, ThreatId, TargetShipId, BehaviorVariantId, CommsVariantId, LocalBehaviorStateId, CommsLine);
}
