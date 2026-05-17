#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "StargameSessionSubsystem.generated.h"

class UStarCatalogSubsystem;
class UWorld;

UENUM(BlueprintType)
enum class EGateTransitionResultCode : uint8
{
	Success,
	MissingActiveSystem,
	MissingSourceGate,
	MissingDestinationSystem,
	MissingDestinationGate,
	MissingDestinationArrival,
	InvalidArrivalFrame,
	SystemBuildFailed,
	PlayerSpawnFailed
};

USTRUCT(BlueprintType)
struct STARGAME_API FGateTransitionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Transition")
	FName SourceGateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Transition")
	FName ShipInstanceId = TEXT("player_ship");
};

USTRUCT(BlueprintType)
struct STARGAME_API FGateTransitionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	EGateTransitionResultCode ResultCode = EGateTransitionResultCode::MissingActiveSystem;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName SourceSystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName SourceGateId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationSystemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationGateId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName DestinationArrivalId;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FName ArrivalFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	double ElapsedTransitionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gate Transition")
	FShipSaveLocation ArrivalLocation;
};

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

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	bool RequestGateTransition(const FGateTransitionRequest& Request, FGateTransitionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	bool HasPendingGateArrival() const { return bHasPendingGateArrival; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FShipSaveLocation GetPendingGateArrivalLocation() const { return PendingGateArrivalLocation; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FGateTransitionResult GetLastGateTransitionResult() const { return LastGateTransitionResult; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void CompleteGateArrivalSafetyWindow();

	bool SaveDevelopmentSlot(const FStargameM0SaveState& State);
	bool LoadDevelopmentSlot(FStargameM0SaveState& OutState);

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FStargameM0SaveState MakeCurrentM0SaveState() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FActiveTrafficSimulationState GetActiveTrafficState() const { return ActiveTrafficState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetActiveTrafficState(const FActiveTrafficSimulationState& NewTrafficState) { ActiveTrafficState = NewTrafficState; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FSystemicGameplayState GetSystemicGameplayState() const { return SystemicGameplayState; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Session")
	void SetSystemicGameplayState(const FSystemicGameplayState& NewSystemicState) { SystemicGameplayState = NewSystemicState; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Session")
	FString GetM0DebugSummary() const;

	static constexpr const TCHAR* DevelopmentSlotName = TEXT("m0_development");

#if WITH_DEV_AUTOMATION_TESTS
	void ConfigureAutomationTestContext(UWorld* InWorld, UStarCatalogSubsystem* InCatalog);
#endif

private:
	UWorld* ResolveSessionWorld() const;
	UStarCatalogSubsystem* ResolveCatalogSubsystem() const;
	bool BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile);
	void ClearSessionState();
	void ReportStartupError(const FString& Error);
	void ReportSessionFailure(const FString& Error);
	bool ValidateLoadedTrafficState(const FStarSystemDefinition& SystemDefinition, const FActiveTrafficSimulationState& TrafficState, FString& OutError) const;
	bool ValidateLoadedSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& LoadedSystemicState, FString& OutError) const;
	bool ValidateLoadedSaveHeader(const FStargameM0SaveState& LoadedState, FString& OutError) const;
	bool ResolveRuntimeSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState* SavedSystemicState, FSystemicGameplayState& OutSystemicState, FString& OutError) const;
	bool RestoreSavedShipLocation(const FShipSaveLocation& ShipLocation, APlayerController* PlayerController, FString& OutError);
	bool ResolveGateArrivalLocation(const FStarSystemDefinition& SystemDefinition, const FShipSaveLocation& GateArrivalLocation, FTransform& OutTransform, FVector& OutVelocityCmPerSec, FString& OutError) const;

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

	UPROPERTY()
	FSystemicGameplayState SystemicGameplayState;

	UPROPERTY()
	bool bHasPendingGateArrival = false;

	UPROPERTY()
	FShipSaveLocation PendingGateArrivalLocation;

	UPROPERTY()
	FGateTransitionResult LastGateTransitionResult;

	UPROPERTY()
	TSubclassOf<APawn> ActivePlayerPawnClass;

#if WITH_DEV_AUTOMATION_TESTS
	UWorld* AutomationTestWorld = nullptr;

	UStarCatalogSubsystem* AutomationTestCatalog = nullptr;
#endif

	EStartSessionResult LastStartSessionResult = EStartSessionResult::ValidationFailed;
	FString LastSessionError;
};
