#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Actor.h"
#include "StationMissionContactActor.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

UCLASS()
class STARGAME_API AStationMissionContactActor : public AActor
{
	GENERATED_BODY()

public:
	AStationMissionContactActor();

	UFUNCTION(BlueprintCallable, Category = "Station|Contact")
	void ConfigureContact(FName InStationId, const FStationQuestGiverDefinition& InDefinition);

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	FName GetStationId() const { return StationId; }

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	FName GetNpcId() const { return ContactDefinition.NpcId; }

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	FText GetDisplayName() const { return ContactDefinition.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	FString GetTalkActionId() const;

	UFUNCTION(BlueprintCallable, Category = "Station|Contact")
	void SetFocusedByPlayer(bool bInFocused);

	UFUNCTION(BlueprintCallable, Category = "Station|Contact")
	void SetPromptText(const FText& InPromptText);

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	bool IsFocusedByPlayer() const { return bFocusedByPlayer; }

	UFUNCTION(BlueprintPure, Category = "Station|Contact")
	static FString BuildTalkActionId(FName NpcId);

	static bool TryParseTalkActionId(const FString& ActionId, FName& OutNpcId);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Contact")
	void OnContactConfigured(FName InStationId, const FStationQuestGiverDefinition& InDefinition);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Contact")
	void OnFocusedByPlayerChanged(bool bInFocused);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Contact")
	void OnPromptTextChanged(const FText& InPromptText);

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> TalkArea;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ContactMesh;

	UPROPERTY(VisibleAnywhere, Category = "Station|Contact")
	FName StationId;

	UPROPERTY(VisibleAnywhere, Category = "Station|Contact")
	FStationQuestGiverDefinition ContactDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Station|Contact")
	bool bFocusedByPlayer = false;
};
