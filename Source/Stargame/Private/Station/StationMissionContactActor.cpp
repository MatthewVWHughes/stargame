#include "Station/StationMissionContactActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"

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
	SetActorLabel(FString::Printf(TEXT("NPC_%s"), *ContactDefinition.NpcId.ToString()));
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
