#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UBFPlayerData.generated.h"

/** Result returned by the local, one-use gift-code catalogue. */
USTRUCT(BlueprintType)
struct FUBFGiftCodeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Gift Codes")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Gift Codes")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Gift Codes")
	FString RewardSummary;
};

USTRUCT(BlueprintType)
struct FUBFMatchRewardResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="UBF|Player Data")
	bool bSaved = false;

	UPROPERTY(BlueprintReadOnly, Category="UBF|Player Data")
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category="UBF|Player Data")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category="UBF|Player Data")
	int32 EventPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category="UBF|Player Data")
	int32 NewAccountLevel = 1;
};

USTRUCT(BlueprintType)
struct FUBFShopPurchaseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Shop")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Shop")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "UBF|Shop")
	FString ItemName;
};

/**
 * Local profile used during the open-beta phase.  Widgets never write this object directly;
 * UUBFPlayerDataSubsystem owns validation, persistence and all mutations.
 */
UCLASS()
class UBF_API UUBFPlayerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile")
	FString PlayerName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile")
	FName SelectedCharacterId = TEXT("Wizz");

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Controls")
	TMap<FName, FKey> InputBindings;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile")
	int32 AccountLevel = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile")
	int32 Experience = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 MatchesPlayed = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 MatchesWon = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 MatchesLost = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 MatchesDrawn = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	float TotalDamageDealt = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	float TotalDamageReceived = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 TotalKnockouts = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 TotalDeaths = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Profile|Stats")
	int32 BestComboHits = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Currency")
	int32 Gold = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Currency")
	int32 PremiumCurrency = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Currency")
	int32 EventPoints = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Gacha")
	int32 GachaTickets = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Inventory")
	TArray<FString> InventoryItems;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "UBF|Gift Codes")
	TSet<FString> RedeemedCodes;
};

DECLARE_MULTICAST_DELEGATE(FUBFPlayerDataChanged);

/**
 * Central local data service for the first playable build.
 * It is intentionally independent from UI and exposes controlled operations only.
 */
UCLASS()
class UBF_API UUBFPlayerDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "UBF|Player Data")
	const UUBFPlayerSaveGame* GetPlayerData() const { return PlayerData; }

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	bool LoadPlayerData();

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	bool SavePlayerData();

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void EnsurePlayerName(const FString& SuggestedName);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void SyncPlayerNameFromLauncher(const FString& LauncherName);

	UFUNCTION(BlueprintPure, Category="UBF|Player Data")
	FName GetSelectedCharacterId() const;

	UFUNCTION(BlueprintCallable, Category="UBF|Player Data")
	void SetSelectedCharacterId(FName CharacterId);

	UFUNCTION(BlueprintPure, Category="UBF|Controls")
	FKey GetInputBinding(FName ActionId) const;

	UFUNCTION(BlueprintCallable, Category="UBF|Controls")
	bool SetInputBinding(FName ActionId, FKey Key, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category="UBF|Controls")
	bool ResetInputBindings();

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void AddGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	FUBFMatchRewardResult GrantMatchRewards(float DamageDealt, float DamageReceived,
		int32 Knockouts, int32 Deaths, int32 MaxComboHits, bool bWon, bool bDraw);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	bool RemoveGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void AddEventPoints(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	bool RemoveEventPoints(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void AddGachaTickets(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UBF|Player Data")
	void AddItem(const FString& ItemName);

	UFUNCTION(BlueprintCallable, Category = "UBF|Shop")
	FUBFShopPurchaseResult PurchasePrototypeShopItem(const FString& ItemId);

	UFUNCTION(BlueprintCallable, Category = "UBF|Gift Codes")
	FUBFGiftCodeResult RedeemGiftCode(const FString& RawCode);

	FUBFPlayerDataChanged OnPlayerDataChanged;

private:
	void NotifyDataChanged();
	UUBFPlayerSaveGame* EnsureData();

	UPROPERTY()
	TObjectPtr<UUBFPlayerSaveGame> PlayerData;
};
