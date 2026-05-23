#include "Station/StationInteriorRoomActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Station/StationInteriorHostileActor.h"
#include "Station/StationInteriorInteractableActor.h"
#include "Station/StationInteriorPawn.h"
#include "Station/StationMissionContactActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool TryGetStationServiceTagValue(const TArray<FName>& Tags, const TCHAR* Prefix, FString& OutValue)
	{
		const FString PrefixString(Prefix);
		for (const FName Tag : Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(PrefixString))
			{
				OutValue = TagString.RightChop(PrefixString.Len());
				return !OutValue.IsEmpty();
			}
		}
		return false;
	}
}

AStationInteriorRoomActor::AStationInteriorRoomActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetupAttachment(SceneRoot);
	FloorMesh->SetRelativeLocation(FVector(0.0, 0.0, -5.0));
	FloorMesh->SetRelativeScale3D(FVector(10.0, 8.0, 0.1));
	FloorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	CeilingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CeilingMesh"));
	CeilingMesh->SetupAttachment(SceneRoot);
	CeilingMesh->SetRelativeLocation(FVector(0.0, 0.0, 235.0));
	CeilingMesh->SetRelativeScale3D(FVector(10.0, 8.0, 0.1));
	CeilingMesh->SetCollisionProfileName(TEXT("BlockAll"));

	NorthWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthWallMesh"));
	NorthWallMesh->SetupAttachment(SceneRoot);
	NorthWallMesh->SetRelativeLocation(FVector(0.0, 500.0, 115.0));
	NorthWallMesh->SetRelativeScale3D(FVector(10.0, 0.1, 2.3));
	NorthWallMesh->SetCollisionProfileName(TEXT("BlockAll"));

	SouthWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthWallMesh"));
	SouthWallMesh->SetupAttachment(SceneRoot);
	SouthWallMesh->SetRelativeLocation(FVector(0.0, -500.0, 115.0));
	SouthWallMesh->SetRelativeScale3D(FVector(10.0, 0.1, 2.3));
	SouthWallMesh->SetCollisionProfileName(TEXT("BlockAll"));

	EastWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EastWallMesh"));
	EastWallMesh->SetupAttachment(SceneRoot);
	EastWallMesh->SetRelativeLocation(FVector(620.0, 0.0, 115.0));
	EastWallMesh->SetRelativeScale3D(FVector(0.1, 8.0, 2.3));
	EastWallMesh->SetCollisionProfileName(TEXT("BlockAll"));

	WestWallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WestWallMesh"));
	WestWallMesh->SetupAttachment(SceneRoot);
	WestWallMesh->SetRelativeLocation(FVector(-620.0, 0.0, 115.0));
	WestWallMesh->SetRelativeScale3D(FVector(0.1, 8.0, 2.3));
	WestWallMesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FloorMesh->SetStaticMesh(CubeMesh.Object);
		CeilingMesh->SetStaticMesh(CubeMesh.Object);
		NorthWallMesh->SetStaticMesh(CubeMesh.Object);
		SouthWallMesh->SetStaticMesh(CubeMesh.Object);
		EastWallMesh->SetStaticMesh(CubeMesh.Object);
		WestWallMesh->SetStaticMesh(CubeMesh.Object);
	}

	MissionContactActorClass = AStationMissionContactActor::StaticClass();
	InteractableActorClass = AStationInteriorInteractableActor::StaticClass();
	HostileActorClass = AStationInteriorHostileActor::StaticClass();
	HostileSpawnLocalTransforms = {
		FTransform(FRotator::ZeroRotator, FVector(260.0, 260.0, 0.0)),
		FTransform(FRotator::ZeroRotator, FVector(260.0, -260.0, 0.0)),
		FTransform(FRotator::ZeroRotator, FVector(-120.0, 360.0, 0.0))
	};
}

FTransform AStationInteriorRoomActor::GetPlayerStartTransform() const
{
	return PlayerStartLocalTransform * GetActorTransform();
}

void AStationInteriorRoomActor::ConfigureStationInterior(FName InStationId, const TArray<FStationQuestGiverDefinition>& InQuestGivers, bool bInHostileBoarding, UStargameSessionSubsystem* InSession)
{
	StationId = InStationId;
	bHostileBoarding = bInHostileBoarding;
	OwningSession = InSession;
	ClearMissionContacts();
	ClearInteractables();
	ClearHostiles();

	UWorld* World = GetWorld();
	if (MissionContactActorClass && World)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (const FStationQuestGiverDefinition& QuestGiver : InQuestGivers)
		{
			if (QuestGiver.NpcId.IsNone())
			{
				continue;
			}

			const FVector WorldLocation = GetActorTransform().TransformPosition(QuestGiver.LocalPositionCm);
			AStationMissionContactActor* Contact = World->SpawnActor<AStationMissionContactActor>(
				MissionContactActorClass,
				FTransform(GetActorRotation(), WorldLocation),
				SpawnParameters);
			if (!Contact)
			{
				continue;
			}

			Contact->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Contact->ConfigureContact(StationId, QuestGiver);
			MissionContactActors.Add(Contact);
		}
	}

	RegisterAuthoredInteractables();
	OnStationInteriorConfigured(StationId, InQuestGivers, bHostileBoarding);
}

void AStationInteriorRoomActor::ArmHostileBoarding(AStationInteriorPawn* PlayerPawn)
{
	if (!bHostileBoarding || !PlayerPawn || !HostileActors.IsEmpty())
	{
		return;
	}

	FStationInteriorCombatProfileDefinition CombatProfile;
	if (OwningSession)
	{
		OwningSession->ResolveStationInteriorCombatProfile(CombatProfileId, CombatProfile);
	}
	PlayerPawn->ApplyCombatProfile(CombatProfile);
	SpawnDefaultHostiles(PlayerPawn, CombatProfile);
}

AStationMissionContactActor* AStationInteriorRoomActor::FindMissionContact(FName NpcId) const
{
	for (AStationMissionContactActor* Contact : MissionContactActors)
	{
		if (Contact && Contact->GetNpcId() == NpcId)
		{
			return Contact;
		}
	}
	return nullptr;
}

AStationMissionContactActor* AStationInteriorRoomActor::FindNearestMissionContact(const FVector& WorldLocation, float MaxDistanceCm) const
{
	AStationMissionContactActor* BestContact = nullptr;
	double BestDistanceSquared = FMath::Square(FMath::Max(0.0f, MaxDistanceCm));
	for (AStationMissionContactActor* Contact : MissionContactActors)
	{
		if (!IsValid(Contact))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(WorldLocation, Contact->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestContact = Contact;
		}
	}
	return BestContact;
}

AStationInteriorInteractableActor* AStationInteriorRoomActor::FindNearestInteractable(const FVector& WorldLocation, float MaxDistanceCm) const
{
	AStationInteriorInteractableActor* BestInteractable = nullptr;
	double BestDistanceSquared = FMath::Square(FMath::Max(0.0f, MaxDistanceCm));
	for (AStationInteriorInteractableActor* Interactable : InteractableActors)
	{
		if (!IsValid(Interactable))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(WorldLocation, Interactable->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestInteractable = Interactable;
		}
	}
	return BestInteractable;
}

AStationInteriorHostileActor* AStationInteriorRoomActor::FindNearestHostile(const FVector& WorldLocation, float MaxDistanceCm) const
{
	AStationInteriorHostileActor* BestHostile = nullptr;
	double BestDistanceSquared = FMath::Square(FMath::Max(0.0f, MaxDistanceCm));
	for (AStationInteriorHostileActor* Hostile : HostileActors)
	{
		if (!IsValid(Hostile) || !Hostile->IsAlive())
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(WorldLocation, Hostile->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestHostile = Hostile;
		}
	}
	return BestHostile;
}

int32 AStationInteriorRoomActor::GetLiveHostileCount() const
{
	int32 Count = 0;
	for (AStationInteriorHostileActor* Hostile : HostileActors)
	{
		if (Hostile && Hostile->IsAlive())
		{
			++Count;
		}
	}
	return Count;
}

bool AStationInteriorRoomActor::AreHostilesCleared() const
{
	return !bHostileBoarding || GetLiveHostileCount() == 0;
}

void AStationInteriorRoomActor::BuildHostileCombatViews(TArray<FStationInteriorHostileCombatView>& OutHostiles) const
{
	OutHostiles.Reset();
	for (AStationInteriorHostileActor* Hostile : HostileActors)
	{
		if (Hostile)
		{
			OutHostiles.Add(Hostile->GetCombatView());
		}
	}
}

void AStationInteriorRoomActor::NotifyHostileKilled(AStationInteriorHostileActor* Hostile)
{
	if (!Hostile || !HostileActors.Contains(Hostile) || GetLiveHostileCount() > 0)
	{
		return;
	}

	if (OwningSession)
	{
		OwningSession->TryCompleteActiveHostileBoardingObjective(StationId);
	}
}

void AStationInteriorRoomActor::RegisterMissionContact(AStationMissionContactActor* Contact)
{
	if (Contact)
	{
		MissionContactActors.AddUnique(Contact);
	}
}

void AStationInteriorRoomActor::RegisterInteractable(AStationInteriorInteractableActor* Interactable)
{
	if (Interactable)
	{
		InteractableActors.AddUnique(Interactable);
	}
}

void AStationInteriorRoomActor::RegisterHostile(AStationInteriorHostileActor* Hostile)
{
	if (Hostile)
	{
		HostileActors.AddUnique(Hostile);
	}
}

void AStationInteriorRoomActor::ClearMissionContacts()
{
	for (AStationMissionContactActor* Contact : MissionContactActors)
	{
		if (Contact)
		{
			Contact->Destroy();
		}
	}
	MissionContactActors.Reset();
}

void AStationInteriorRoomActor::ClearInteractables()
{
	for (AStationInteriorInteractableActor* Interactable : InteractableActors)
	{
		if (Interactable)
		{
			if (Interactable->GetParentComponent() == nullptr)
			{
				Interactable->Destroy();
			}
		}
	}
	InteractableActors.Reset();
}

void AStationInteriorRoomActor::ClearHostiles()
{
	for (AStationInteriorHostileActor* Hostile : HostileActors)
	{
		if (Hostile)
		{
			Hostile->Destroy();
		}
	}
	HostileActors.Reset();
}

void AStationInteriorRoomActor::RegisterAuthoredInteractables()
{
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (!ChildActorComponent)
		{
			continue;
		}

		AStationInteriorInteractableActor* Interactable = Cast<AStationInteriorInteractableActor>(ChildActorComponent->GetChildActor());
		if (!Interactable || Interactable->GetInteractionType().IsNone())
		{
			if (Interactable)
			{
				FString InteractionTypeTag;
				FString DisplayNameTag;
				TryGetStationServiceTagValue(ChildActorComponent->ComponentTags, TEXT("InteractionType="), InteractionTypeTag);
				TryGetStationServiceTagValue(ChildActorComponent->ComponentTags, TEXT("DisplayName="), DisplayNameTag);
				if (!InteractionTypeTag.IsEmpty())
				{
					Interactable->ConfigureInteractable(
						StationId,
						FName(*InteractionTypeTag),
						DisplayNameTag.IsEmpty() ? FText() : FText::FromString(DisplayNameTag));
				}
				else
				{
					Interactable->ConfigureInteractableFromDefaults(StationId);
				}
			}
		}

		RegisterInteractable(Interactable);
	}

	ConfigureOrSpawnDefaultInteractable(TEXT("repair"), NSLOCTEXT("StationInterior", "RepairDesk", "Repair Service"), FVector(-180.0, -430.0, 0.0));
	ConfigureOrSpawnDefaultInteractable(TEXT("refuel"), NSLOCTEXT("StationInterior", "RefuelDesk", "Refuel Service"), FVector(120.0, -430.0, 0.0));
	ConfigureOrSpawnDefaultInteractable(TEXT("market"), NSLOCTEXT("StationInterior", "CommodityDesk", "Commodity Desk"), FVector(360.0, 0.0, 0.0));
	ConfigureOrSpawnDefaultInteractable(TEXT("mission_board"), NSLOCTEXT("StationInterior", "MissionBoard", "Mission Board"), FVector(360.0, 320.0, 0.0));
	ConfigureOrSpawnDefaultInteractable(TEXT("launch"), NSLOCTEXT("StationInterior", "LaunchPoint", "Airlock / Departure"), FVector(-430.0, 360.0, 0.0));
}

void AStationInteriorRoomActor::SpawnDefaultInteractable(FName InteractionType, const FText& DisplayName, const FVector& LocalPositionCm)
{
	UWorld* World = GetWorld();
	if (!World || !InteractableActorClass || InteractionType.IsNone())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStationInteriorInteractableActor* Interactable = World->SpawnActor<AStationInteriorInteractableActor>(
		InteractableActorClass,
		FTransform(GetActorRotation(), GetActorTransform().TransformPosition(LocalPositionCm)),
		SpawnParameters);
	if (!Interactable)
	{
		return;
	}

	Interactable->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Interactable->ConfigureInteractable(StationId, InteractionType, DisplayName);
	RegisterInteractable(Interactable);
}

void AStationInteriorRoomActor::ConfigureOrSpawnDefaultInteractable(FName InteractionType, const FText& DisplayName, const FVector& LocalPositionCm)
{
	if (InteractionType.IsNone())
	{
		return;
	}

	const FVector WorldPosition = GetActorTransform().TransformPosition(LocalPositionCm);
	for (AStationInteriorInteractableActor* Interactable : InteractableActors)
	{
		if (IsValid(Interactable) && Interactable->GetInteractionType() == InteractionType)
		{
			Interactable->SetActorLocation(WorldPosition);
			Interactable->ConfigureInteractable(StationId, InteractionType, DisplayName);
			return;
		}
	}

	SpawnDefaultInteractable(InteractionType, DisplayName, LocalPositionCm);
}

void AStationInteriorRoomActor::SpawnDefaultHostiles(AStationInteriorPawn* PlayerPawn, const FStationInteriorCombatProfileDefinition& CombatProfile)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerPawn || !HostileActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const int32 HostileCount = FMath::Clamp(CombatProfile.HostileCount, 0, HostileSpawnLocalTransforms.Num());
	for (int32 Index = 0; Index < HostileCount; ++Index)
	{
		const FTransform WorldTransform = HostileSpawnLocalTransforms[Index] * GetActorTransform();
		AStationInteriorHostileActor* Hostile = World->SpawnActor<AStationInteriorHostileActor>(
			HostileActorClass,
			WorldTransform,
			SpawnParameters);
		if (!Hostile)
		{
			continue;
		}

		Hostile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Hostile->ApplyCombatProfile(CombatProfile);
		Hostile->ConfigureHostile(this, PlayerPawn, FName(*FString::Printf(TEXT("%s_hostile_%d"), *StationId.ToString(), Index + 1)));
		HostileActors.Add(Hostile);
	}
}
