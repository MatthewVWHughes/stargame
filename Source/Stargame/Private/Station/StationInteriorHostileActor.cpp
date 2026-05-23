#include "Station/StationInteriorHostileActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Station/StationInteriorPawn.h"
#include "Station/StationInteriorRoomActor.h"
#include "UObject/ConstructorHelpers.h"

AStationInteriorHostileActor::AStationInteriorHostileActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->SetupAttachment(SceneRoot);
	CollisionCapsule->SetRelativeLocation(FVector(0.0, 0.0, 95.0));
	CollisionCapsule->SetCapsuleRadius(45.0f);
	CollisionCapsule->SetCapsuleHalfHeight(100.0f);
	CollisionCapsule->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	HostileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HostileMesh"));
	HostileMesh->SetupAttachment(SceneRoot);
	HostileMesh->SetRelativeLocation(FVector(0.0, 0.0, 95.0));
	HostileMesh->SetRelativeScale3D(FVector(0.55, 0.55, 1.9));
	HostileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		HostileMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AStationInteriorHostileActor::ConfigureHostile(AStationInteriorRoomActor* InRoom, AStationInteriorPawn* InPlayer, FName InHostileId)
{
	OwningRoom = InRoom;
	PlayerPawn = InPlayer;
	HostileId = InHostileId;
	Health = MaxHealth;
	bAlive = true;
	OnHostileConfigured(HostileId);
	OnHostileHealthChanged(Health, MaxHealth);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("StationHostile_%s"), *HostileId.ToString()));
#endif
}

void AStationInteriorHostileActor::ApplyCombatProfile(const FStationInteriorCombatProfileDefinition& Profile)
{
	MaxHealth = FMath::Max(1.0f, static_cast<float>(Profile.HostileMaxHealth));
	FireDamage = FMath::Max(0.0f, static_cast<float>(Profile.HostileFireDamage));
	DetectionRangeCm = FMath::Max(0.0f, static_cast<float>(Profile.HostileDetectionRangeCm));
	FireRangeCm = FMath::Max(0.0f, static_cast<float>(Profile.HostileFireRangeCm));
	FireCooldownSeconds = FMath::Max(0.0f, static_cast<float>(Profile.HostileFireCooldownSeconds));
	InaccuracyDegrees = FMath::Max(0.0f, static_cast<float>(Profile.HostileInaccuracyDegrees));
	FireCooldownRemainingSeconds = FMath::Max(0.0f, static_cast<float>(Profile.HostileInitialFireDelaySeconds));
	Health = MaxHealth;
	bAlive = true;
	OnHostileHealthChanged(Health, MaxHealth);
}

FStationInteriorHostileCombatView AStationInteriorHostileActor::GetCombatView() const
{
	FStationInteriorHostileCombatView View;
	View.HostileId = HostileId;
	View.bAlive = bAlive;
	View.State = !bAlive ? FName(TEXT("neutralized")) : (FireCooldownRemainingSeconds <= 0.0f ? FName(TEXT("ready")) : FName(TEXT("cooldown")));
	View.Health = Health;
	View.MaxHealth = MaxHealth;
	View.FireDamage = FireDamage;
	View.DetectionRangeCm = DetectionRangeCm;
	View.FireRangeCm = FireRangeCm;
	View.FireCooldownSeconds = FireCooldownSeconds;
	View.FireCooldownRemainingSeconds = FireCooldownRemainingSeconds;
	return View;
}

void AStationInteriorHostileActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAlive || !PlayerPawn || !PlayerPawn->IsOnFootAlive())
	{
		return;
	}

	FireCooldownRemainingSeconds = FMath::Max(0.0f, FireCooldownRemainingSeconds - DeltaSeconds);

	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const double DistanceCm = ToPlayer.Length();
	if (DistanceCm <= KINDA_SMALL_NUMBER || DistanceCm > DetectionRangeCm || !HasLineOfSightToPlayer())
	{
		return;
	}

	const FVector FlatDirection(ToPlayer.X, ToPlayer.Y, 0.0);
	if (!FlatDirection.IsNearlyZero())
	{
		SetActorRotation(FlatDirection.Rotation());
	}

	if (DistanceCm <= FireRangeCm && FireCooldownRemainingSeconds <= 0.0f)
	{
		FireAtPlayer();
		FireCooldownRemainingSeconds = FireCooldownSeconds;
	}
}

bool AStationInteriorHostileActor::ApplyOnFootDamage(float Amount)
{
	if (!bAlive)
	{
		return false;
	}

	Health = FMath::Max(0.0f, Health - FMath::Max(0.0f, Amount));
	OnHostileHealthChanged(Health, MaxHealth);

	if (Health <= 0.0f)
	{
		Die();
	}
	return true;
}

bool AStationInteriorHostileActor::HasLineOfSightToPlayer() const
{
	if (!PlayerPawn)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StationHostileLineOfSight), true);
	QueryParams.AddIgnoredActor(this);

	const FVector Start = GetActorLocation() + FVector(0.0, 0.0, 140.0);
	const FVector End = PlayerPawn->GetActorLocation() + FVector(0.0, 0.0, 80.0);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		return true;
	}

	return Hit.GetActor() == PlayerPawn;
}

void AStationInteriorHostileActor::FireAtPlayer()
{
	if (!PlayerPawn)
	{
		return;
	}

	const FVector Start = GetActorLocation() + FVector(0.0, 0.0, 140.0);
	FVector Direction = (PlayerPawn->GetActorLocation() + FVector(0.0, 0.0, 80.0) - Start).GetSafeNormal();
	if (InaccuracyDegrees > 0.0f)
	{
		Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(InaccuracyDegrees));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StationHostileFire), true);
	QueryParams.AddIgnoredActor(this);
	const FVector End = Start + Direction * FireRangeCm * 1.2f;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams) && Hit.GetActor() == PlayerPawn)
	{
		PlayerPawn->TakeOnFootDamage(FireDamage);
	}
}

void AStationInteriorHostileActor::Die()
{
	bAlive = false;
	SetActorTickEnabled(false);
	if (CollisionCapsule)
	{
		CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (HostileMesh)
	{
		HostileMesh->SetVisibility(false, true);
	}
	OnHostileDied();

	if (OwningRoom)
	{
		OwningRoom->NotifyHostileKilled(this);
	}
}
