#include "Station/StationMissionContactActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* TalkActionPrefix = TEXT("talk:");
}

AStationMissionContactActor::AStationMissionContactActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TalkArea = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TalkArea"));
	TalkArea->SetupAttachment(SceneRoot);
	TalkArea->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	TalkArea->SetCapsuleRadius(90.0f);
	TalkArea->SetCapsuleHalfHeight(110.0f);
	TalkArea->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	ContactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContactMesh"));
	ContactMesh->SetupAttachment(SceneRoot);
	ContactMesh->SetRelativeLocation(FVector(0.0, 0.0, 95.0));
	ContactMesh->SetRelativeScale3D(FVector(0.45, 0.45, 1.9));
	ContactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ContactMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AStationMissionContactActor::ConfigureContact(FName InStationId, const FStationQuestGiverDefinition& InDefinition)
{
	StationId = InStationId;
	ContactDefinition = InDefinition;
	if (ContactDefinition.DisplayName.IsEmpty() && !ContactDefinition.NpcId.IsNone())
	{
		ContactDefinition.DisplayName = FText::FromName(ContactDefinition.NpcId);
	}
	OnContactConfigured(StationId, ContactDefinition);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("NPC_%s"), *ContactDefinition.NpcId.ToString()));
#endif
}

FString AStationMissionContactActor::GetTalkActionId() const
{
	return BuildTalkActionId(ContactDefinition.NpcId);
}

void AStationMissionContactActor::SetFocusedByPlayer(bool bInFocused)
{
	if (bFocusedByPlayer == bInFocused)
	{
		return;
	}

	bFocusedByPlayer = bInFocused;
	OnFocusedByPlayerChanged(bFocusedByPlayer);
}

void AStationMissionContactActor::SetPromptText(const FText& InPromptText)
{
	OnPromptTextChanged(InPromptText);
}

FString AStationMissionContactActor::BuildTalkActionId(FName NpcId)
{
	return FString::Printf(TEXT("%s%s"), TalkActionPrefix, *NpcId.ToString());
}

bool AStationMissionContactActor::TryParseTalkActionId(const FString& ActionId, FName& OutNpcId)
{
	OutNpcId = NAME_None;
	if (ActionId.IsEmpty() || !ActionId.StartsWith(TalkActionPrefix))
	{
		return false;
	}

	const FString NpcIdString = ActionId.RightChop(FCString::Strlen(TalkActionPrefix));
	if (NpcIdString.IsEmpty())
	{
		return false;
	}

	OutNpcId = FName(*NpcIdString);
	return true;
}
