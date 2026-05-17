#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "StargameSessionSubsystem.generated.h"

UCLASS()
class STARGAME_API UStargameSessionSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	EStartSessionResult StartNewSession(FName InStartProfileId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetCurrentSystemId() const { return CurrentSystemId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetStartProfileId() const { return StartProfileId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetCurrentSpawnZoneId() const { return CurrentSpawnZoneId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FName GetSelectedTargetId() const { return SelectedTargetId; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetSelectedTargetId(FName TargetId) { SelectedTargetId = TargetId; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool SelectNavigationTargetById(FName TargetId);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool CycleNavigationTarget();

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	EStartSessionResult GetLastStartSessionResult() const { return LastStartSessionResult; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FString GetLastSessionError() const { return LastSessionError; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FSimulationClockSnapshot GetSimulationClockSnapshot() const { return ClockSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void AdvanceSimulationClock(double DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool SaveDevelopmentSlot();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool LoadDevelopmentSlot();

	bool SaveDevelopmentSlot(const FStargameM0SaveState& State);
	bool LoadDevelopmentSlot(FStargameM0SaveState& OutState);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FStargameM0SaveState MakeCurrentM0SaveState() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FActiveTrafficSimulationState GetActiveTrafficState() const { return ActiveTrafficState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetActiveTrafficState(const FActiveTrafficSimulationState& NewTrafficState) { ActiveTrafficState = NewTrafficState; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FString GetM0DebugSummary() const;

	static constexpr const TCHAR* DevelopmentSlotName = TEXT("m0_development");

private:
	bool BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile);
	void ClearSessionState();
	void ReportStartupError(const FString& Error);
	bool ValidateLoadedTrafficState(const FStarSystemDefinition& SystemDefinition, const FActiveTrafficSimulationState& TrafficState, FString& OutError) const;
	bool RestoreSavedShipLocation(const FShipSaveLocation& ShipLocation, APlayerController* PlayerController, FString& OutError);

	UPROPERTY()
	FName StartProfileId = TEXT("frontier_test_start");

	UPROPERTY()
	FName CurrentSystemId;

	UPROPERTY()
	FName CurrentSpawnZoneId;

	UPROPERTY()
	FName SelectedTargetId;

	UPROPERTY()
	FSimulationClockSnapshot ClockSnapshot;

	UPROPERTY()
	FActiveTrafficSimulationState ActiveTrafficState;

	EStartSessionResult LastStartSessionResult = EStartSessionResult::ValidationFailed;
	FString LastSessionError;
};
