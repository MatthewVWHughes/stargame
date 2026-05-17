#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataAssets.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StarCatalogSubsystem.generated.h"

UCLASS()
class STARGAME_API UStarCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Catalog")
	void BuildNativeFixtureCache();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Catalog")
	bool BuildAssetCatalogCache(bool bAllowNativeFallback = true);

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	bool IsUsingAssetCatalog() const { return bUsingAssetCatalog; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	bool ResolveStartProfile(FName StartProfileId, FStartProfileDefinition& OutStartProfile) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	bool ResolveSystemDefinition(FName SystemId, FStarSystemDefinition& OutSystemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	bool ResolveShipArchetype(FName ShipArchetypeId, FShipArchetypeDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM1Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM2Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM3Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM4Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM5Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM7Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM8Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM9Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM10Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM11Fixture(FName SystemId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FStargameValidationReport ValidateM6GeneratedSystemDefinition(const FStarSystemDefinition& SystemDefinition) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Catalog")
	bool GenerateSeededPhysicalSystem(int32 Seed, FStarSystemDefinition& OutSystemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	void GetKnownSystemIds(TArray<FName>& OutSystemIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Catalog")
	FString GetCatalogDebugSummary() const;

private:
	void AddIssue(FStargameValidationReport& Report, EStargameValidationSeverity Severity, FName RuleId, FName ObjectId, const FString& Message) const;
	bool ValidateAssetManagerCoverage(FStargameValidationReport& Report) const;
	bool ValidateStartProfile(const FStartProfileDefinition& StartProfile, const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateSystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM2SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM3SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM4SystemDefinition(const FStarSystemDefinition& SystemDefinition, const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const;
	bool ValidateM5SystemDefinition(const FStarSystemDefinition& SystemDefinition, const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const;
	bool ValidateM6SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM7SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM8SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM9SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM10SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateM11SystemDefinition(const FStarSystemDefinition& SystemDefinition, FStargameValidationReport& Report) const;
	bool ValidateShipArchetype(const FStartProfileDefinition& StartProfile, FStargameValidationReport& Report) const;
	void ResetCatalogCache();

	UPROPERTY()
	TArray<FStarCatalogEntry> CatalogEntries;

	UPROPERTY()
	TMap<FName, FStartProfileDefinition> StartProfilesById;

	UPROPERTY()
	TMap<FName, FStarSystemDefinition> SystemsById;

	UPROPERTY()
	TMap<FName, FShipArchetypeDefinition> ShipArchetypesById;

	UPROPERTY()
	TMap<FName, FShipMovementProfileDefinition> MovementProfilesById;

	UPROPERTY()
	TMap<FName, FShipDurabilityProfileDefinition> DurabilityProfilesById;

	UPROPERTY()
	TMap<FName, FShipLoadoutProfileDefinition> LoadoutProfilesById;

	UPROPERTY()
	TMap<FName, FShipResourceProfileDefinition> ResourceProfilesById;

	bool bUsingAssetCatalog = false;
};
