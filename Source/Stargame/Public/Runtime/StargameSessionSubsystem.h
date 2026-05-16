#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StargameSessionSubsystem.generated.h"

UCLASS()
class STARGAME_API UStargameSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
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
	FString GetM0DebugSummary() const;

	static constexpr const TCHAR* DevelopmentSlotName = TEXT("m0_development");

private:
	bool BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile);
	void ReportStartupError(const FString& Error);

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

	EStartSessionResult LastStartSessionResult = EStartSessionResult::ValidationFailed;
	FString LastSessionError;
};
