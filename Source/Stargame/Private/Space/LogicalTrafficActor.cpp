#include "Space/LogicalTrafficActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/StargameDataTypes.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void AddLogicalTrafficActorTagIfSet(AActor& Actor, FName Tag)
	{
		if (!Tag.IsNone())
		{
			Actor.Tags.AddUnique(Tag);
		}
	}
}

ALogicalTrafficActor::ALogicalTrafficActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeTrafficShipMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);
	VisualMesh->SetRelativeScale3D(FVector(1.8, 0.7, 0.35));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(ConeMesh.Object);
	}
}

void ALogicalTrafficActor::ConfigureTrafficActor(FName InShipInstanceId, FName InGroupId, EShipGoalKind InGoalKind)
{
	Tags.Remove(ShipInstanceId);
	Tags.Remove(GroupId);
	ShipInstanceId = InShipInstanceId;
	GroupId = InGroupId;
	GoalKind = InGoalKind;
	if (!bFreeRoamCombatPresentation)
	{
		bFreeRoamCombatPresentation = GoalKind == EShipGoalKind::Pirate || GoalKind == EShipGoalKind::Patrol;
	}
	bHostilePresentation = bHostilePresentation || GoalKind == EShipGoalKind::Pirate;
	const double RoleScale = GoalKind == EShipGoalKind::Pirate ? 1.15 : (GoalKind == EShipGoalKind::Patrol ? 1.05 : 1.0);
	VisualMesh->SetRelativeScale3D(FVector(1.8, 0.7, 0.35) * RoleScale);
	AddLogicalTrafficActorTagIfSet(*this, ShipInstanceId);
	AddLogicalTrafficActorTagIfSet(*this, GroupId);
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
	bHostilePresentation = bHostilePresentation ||
		GoalKind == EShipGoalKind::Pirate ||
		IntentType == FName(TEXT("attack")) ||
		BehaviorVariantId.ToString().Contains(TEXT("pirate"), ESearchCase::IgnoreCase);
	bFreeRoamCombatPresentation = bFreeRoamCombatPresentation ||
		bHostilePresentation ||
		LocalBehaviorStateId.ToString().Contains(TEXT("intercept"), ESearchCase::IgnoreCase);
	AddLogicalTrafficActorTagIfSet(*this, EncounterId);
	AddLogicalTrafficActorTagIfSet(*this, IntentType);
	AddLogicalTrafficActorTagIfSet(*this, BehaviorVariantId);
	AddLogicalTrafficActorTagIfSet(*this, CommsVariantId);
	AddLogicalTrafficActorTagIfSet(*this, LocalBehaviorStateId);
	OnEncounterBehaviorConfigured(EncounterId, IntentId, IntentType, ThreatId, TargetShipId, BehaviorVariantId, CommsVariantId, LocalBehaviorStateId, CommsLine);
}

void ALogicalTrafficActor::ConfigureCombatPresentation(FName InFactionId, double InShield, double InHull, bool bInHostile, bool bInFreeRoamCombat)
{
	FactionId = InFactionId;
	ShieldPresentation = FMath::Max(0.0, InShield);
	HullPresentation = FMath::Max(0.0, InHull);
	bHostilePresentation = bInHostile;
	bFreeRoamCombatPresentation = bInFreeRoamCombat;
	bDestroyedPresentation = HullPresentation <= 0.0;
	if (!FactionId.IsNone())
	{
		Tags.AddUnique(FactionId);
	}
	OnCombatPresentationConfigured(FactionId, ShieldPresentation, HullPresentation, bHostilePresentation, bFreeRoamCombatPresentation);
}

bool ALogicalTrafficActor::ApplyCombatPresentationDamage(double DamageAmount, FName DamageType)
{
	if (DamageAmount <= 0.0 || bDestroyedPresentation)
	{
		return false;
	}

	double RemainingDamage = DamageAmount;
	const double ShieldDamage = FMath::Min(ShieldPresentation, RemainingDamage);
	ShieldPresentation -= ShieldDamage;
	RemainingDamage -= ShieldDamage;
	if (RemainingDamage > 0.0)
	{
		HullPresentation = FMath::Max(0.0, HullPresentation - RemainingDamage);
	}

	bDestroyedPresentation = HullPresentation <= 0.0;
	if (bDestroyedPresentation)
	{
		VisualMesh->SetVisibility(false, true);
		Tags.AddUnique(TEXT("destroyed"));
	}

	OnCombatPresentationDamaged(DamageAmount, DamageType, ShieldPresentation, HullPresentation, bDestroyedPresentation);
	return true;
}

bool ALogicalTrafficActor::BuildShotPresentationToLocation(const FVector& TargetLocationCm, double MaxRangeCm, FVector& OutStartPositionCm, FVector& OutEndPositionCm) const
{
	OutStartPositionCm = FVector::ZeroVector;
	OutEndPositionCm = FVector::ZeroVector;
	if (bDestroyedPresentation)
	{
		return false;
	}

	OutStartPositionCm = VisualMesh ? VisualMesh->GetComponentLocation() : GetActorLocation();
	const FVector ToTarget = TargetLocationCm - OutStartPositionCm;
	const double DistanceCm = ToTarget.Size();
	if (DistanceCm <= UE_SMALL_NUMBER || (MaxRangeCm > 0.0 && DistanceCm > MaxRangeCm))
	{
		return false;
	}

	OutEndPositionCm = TargetLocationCm;
	return true;
}

void ALogicalTrafficActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!VisualMesh || bDestroyedPresentation)
	{
		return;
	}

	NativePresentationPhaseSeconds += FMath::Max(0.0f, DeltaSeconds);
	const double BobCm = bFreeRoamCombatPresentation ? FMath::Sin(NativePresentationPhaseSeconds * 2.7) * 85.0 : 0.0;
	const double RollDeg = bFreeRoamCombatPresentation ? FMath::Sin(NativePresentationPhaseSeconds * 1.8) * 10.0 : 0.0;
	VisualMesh->SetRelativeLocation(FVector(0.0, 0.0, BobCm));
	VisualMesh->SetRelativeRotation(FRotator(0.0, 0.0, RollDeg));
}
