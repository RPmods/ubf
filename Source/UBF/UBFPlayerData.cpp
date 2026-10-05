#include "UBFPlayerData.h"

#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Char.h"

namespace UBFPlayerData
{
	DEFINE_LOG_CATEGORY_STATIC(LogUBFPlayerData, Log, All);
	const FString SaveSlotName(TEXT("UBF_PlayerProfile_v1"));
	constexpr int32 SaveUserIndex = 0;

	FString NormalizeGiftCode(const FString& Value)
	{
		FString Result = Value.TrimStartAndEnd();
		Result.ToUpperInline();
		return Result;
	}

	FKey DefaultInputBinding(FName ActionId)
	{
		if (ActionId == TEXT("PrimarySkill")) return EKeys::Q;
		if (ActionId == TEXT("SecondarySkill")) return EKeys::E;
		if (ActionId == TEXT("Ultimate")) return EKeys::F;
		return EKeys::Invalid;
	}

	bool IsRemappableAction(FName ActionId)
	{
		return ActionId == TEXT("PrimarySkill") || ActionId == TEXT("SecondarySkill")
			|| ActionId == TEXT("Ultimate");
	}
}

void UUBFPlayerDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPlayerData();
}

void UUBFPlayerDataSubsystem::Deinitialize()
{
	SavePlayerData();
	Super::Deinitialize();
}

UUBFPlayerSaveGame* UUBFPlayerDataSubsystem::EnsureData()
{
	if (!PlayerData)
	{
		PlayerData = Cast<UUBFPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UUBFPlayerSaveGame::StaticClass()));
	}
	return PlayerData;
}

bool UUBFPlayerDataSubsystem::LoadPlayerData()
{
	if (UGameplayStatics::DoesSaveGameExist(UBFPlayerData::SaveSlotName, UBFPlayerData::SaveUserIndex))
	{
		PlayerData = Cast<UUBFPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(
			UBFPlayerData::SaveSlotName, UBFPlayerData::SaveUserIndex));
	}

	if (!EnsureData())
	{
		UE_LOG(UBFPlayerData::LogUBFPlayerData, Error, TEXT("No se pudo crear ni cargar el perfil local de UBF."));
		return false;
	}

	PlayerData->AccountLevel = FMath::Max(1, PlayerData->AccountLevel);
	PlayerData->Experience = FMath::Max(0, PlayerData->Experience);
	PlayerData->Gold = FMath::Max(0, PlayerData->Gold);
	PlayerData->PremiumCurrency = FMath::Max(0, PlayerData->PremiumCurrency);
	PlayerData->EventPoints = FMath::Max(0, PlayerData->EventPoints);
	PlayerData->GachaTickets = FMath::Max(0, PlayerData->GachaTickets);
	PlayerData->MatchesPlayed = FMath::Max(0, PlayerData->MatchesPlayed);
	PlayerData->MatchesWon = FMath::Max(0, PlayerData->MatchesWon);
	PlayerData->MatchesLost = FMath::Max(0, PlayerData->MatchesLost);
	PlayerData->MatchesDrawn = FMath::Max(0, PlayerData->MatchesDrawn);
	PlayerData->TotalDamageDealt = FMath::IsFinite(PlayerData->TotalDamageDealt)
		? FMath::Max(0.0f, PlayerData->TotalDamageDealt) : 0.0f;
	PlayerData->TotalDamageReceived = FMath::IsFinite(PlayerData->TotalDamageReceived)
		? FMath::Max(0.0f, PlayerData->TotalDamageReceived) : 0.0f;
	PlayerData->TotalKnockouts = FMath::Max(0, PlayerData->TotalKnockouts);
	PlayerData->TotalDeaths = FMath::Max(0, PlayerData->TotalDeaths);
	PlayerData->BestComboHits = FMath::Max(0, PlayerData->BestComboHits);
	if (PlayerData->SelectedCharacterId.IsNone())
	{
		PlayerData->SelectedCharacterId = TEXT("Wizz");
	}
	return true;
}

bool UUBFPlayerDataSubsystem::SavePlayerData()
{
	if (!PlayerData)
	{
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		PlayerData, UBFPlayerData::SaveSlotName, UBFPlayerData::SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(UBFPlayerData::LogUBFPlayerData, Error, TEXT("No se pudo guardar el perfil local de UBF."));
	}
	return bSaved;
}

void UUBFPlayerDataSubsystem::EnsurePlayerName(const FString& SuggestedName)
{
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || !Data->PlayerName.IsEmpty())
	{
		return;
	}

	Data->PlayerName = SuggestedName.TrimStartAndEnd();
	if (Data->PlayerName.IsEmpty())
	{
		Data->PlayerName = TEXT("JUGADOR");
	}
	SavePlayerData();
	NotifyDataChanged();
}

void UUBFPlayerDataSubsystem::SyncPlayerNameFromLauncher(const FString& LauncherName)
{
	const FString CleanName = LauncherName.TrimStartAndEnd().Left(24);
	if (CleanName.IsEmpty())
	{
		return;
	}

	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || Data->PlayerName == CleanName)
	{
		return;
	}

	Data->PlayerName = CleanName;
	SavePlayerData();
	NotifyDataChanged();
}

FName UUBFPlayerDataSubsystem::GetSelectedCharacterId() const
{
	return PlayerData && !PlayerData->SelectedCharacterId.IsNone()
		? PlayerData->SelectedCharacterId : FName(TEXT("Wizz"));
}

void UUBFPlayerDataSubsystem::SetSelectedCharacterId(FName CharacterId)
{
	if (CharacterId.IsNone())
	{
		return;
	}
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || Data->SelectedCharacterId == CharacterId)
	{
		return;
	}
	const FName PreviousCharacterId = Data->SelectedCharacterId;
	Data->SelectedCharacterId = CharacterId;
	if (!SavePlayerData())
	{
		Data->SelectedCharacterId = PreviousCharacterId;
		return;
	}
	NotifyDataChanged();
}

FKey UUBFPlayerDataSubsystem::GetInputBinding(FName ActionId) const
{
	if (!UBFPlayerData::IsRemappableAction(ActionId))
	{
		return EKeys::Invalid;
	}
	if (PlayerData)
	{
		if (const FKey* SavedKey = PlayerData->InputBindings.Find(ActionId); SavedKey && SavedKey->IsValid())
		{
			return *SavedKey;
		}
	}
	return UBFPlayerData::DefaultInputBinding(ActionId);
}

bool UUBFPlayerDataSubsystem::SetInputBinding(FName ActionId, FKey Key, FString& OutMessage)
{
	OutMessage.Reset();
	if (!UBFPlayerData::IsRemappableAction(ActionId) || !Key.IsValid() || Key.IsGamepadKey())
	{
		OutMessage = TEXT("Selecciona una tecla o botón de ratón válido.");
		return false;
	}
	const FKey ProtectedKeys[] = { EKeys::W, EKeys::A, EKeys::S, EKeys::D,
		EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::RightMouseButton,
		EKeys::LeftControl, EKeys::Escape };
	for (const FKey& ProtectedKey : ProtectedKeys)
	{
		if (ProtectedKey == Key)
		{
			OutMessage = FString::Printf(TEXT("%s ya está reservado para otra acción."),
				*Key.GetDisplayName().ToString());
			return false;
		}
	}
	for (const FName OtherAction : { FName(TEXT("PrimarySkill")), FName(TEXT("SecondarySkill")), FName(TEXT("Ultimate")) })
	{
		if (OtherAction != ActionId && GetInputBinding(OtherAction) == Key)
		{
			OutMessage = FString::Printf(TEXT("%s ya está asignado a otra habilidad."), *Key.GetDisplayName().ToString());
			return false;
		}
	}

	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data)
	{
		OutMessage = TEXT("No se pudo cargar el perfil local.");
		return false;
	}
	const TMap<FName, FKey> PreviousBindings = Data->InputBindings;
	Data->InputBindings.Add(ActionId, Key);
	if (!SavePlayerData())
	{
		Data->InputBindings = PreviousBindings;
		OutMessage = TEXT("No se pudo guardar el control.");
		return false;
	}
	NotifyDataChanged();
	OutMessage = FString::Printf(TEXT("Asignado: %s"), *Key.GetDisplayName().ToString());
	return true;
}

bool UUBFPlayerDataSubsystem::ResetInputBindings()
{
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || Data->InputBindings.IsEmpty())
	{
		return true;
	}
	const TMap<FName, FKey> PreviousBindings = Data->InputBindings;
	Data->InputBindings.Reset();
	if (!SavePlayerData())
	{
		Data->InputBindings = PreviousBindings;
		return false;
	}
	NotifyDataChanged();
	return true;
}

void UUBFPlayerDataSubsystem::AddGold(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	if (UUBFPlayerSaveGame* Data = EnsureData())
	{
		Data->Gold += Amount;
		SavePlayerData();
		NotifyDataChanged();
	}
}

FUBFMatchRewardResult UUBFPlayerDataSubsystem::GrantMatchRewards(float DamageDealt, float DamageReceived,
	int32 Knockouts, int32 Deaths, int32 MaxComboHits, bool bWon, bool bDraw)
{
	FUBFMatchRewardResult Result;
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data)
	{
		return Result;
	}

	// Participation always advances progression; the winner receives a modest bonus.
	Result.Experience = 100 + FMath::Clamp(FMath::FloorToInt(FMath::Max(0.0f, DamageDealt) / 15.0f), 0, 100)
		+ (bWon ? 75 : 0);
	Result.Gold = 150 + (bWon ? 100 : 0);
	Result.EventPoints = 20 + FMath::Clamp(FMath::FloorToInt(FMath::Max(0.0f, DamageDealt) / 50.0f), 0, 20)
		+ (bWon ? 10 : 0);

	const int32 PreviousLevel = Data->AccountLevel;
	const int32 PreviousExperience = Data->Experience;
	const int32 PreviousGold = Data->Gold;
	const int32 PreviousEventPoints = Data->EventPoints;
	const int32 PreviousMatchesPlayed = Data->MatchesPlayed;
	const int32 PreviousMatchesWon = Data->MatchesWon;
	const int32 PreviousMatchesLost = Data->MatchesLost;
	const int32 PreviousMatchesDrawn = Data->MatchesDrawn;
	const float PreviousTotalDamageDealt = Data->TotalDamageDealt;
	const float PreviousTotalDamageReceived = Data->TotalDamageReceived;
	const int32 PreviousTotalKnockouts = Data->TotalKnockouts;
	const int32 PreviousTotalDeaths = Data->TotalDeaths;
	const int32 PreviousBestComboHits = Data->BestComboHits;

	Data->Experience += Result.Experience;
	Data->Gold += Result.Gold;
	Data->EventPoints += Result.EventPoints;
	++Data->MatchesPlayed;
	if (bDraw)
	{
		++Data->MatchesDrawn;
	}
	else if (bWon)
	{
		++Data->MatchesWon;
	}
	else
	{
		++Data->MatchesLost;
	}
	Data->TotalDamageDealt += FMath::Max(0.0f, DamageDealt);
	Data->TotalDamageReceived += FMath::Max(0.0f, DamageReceived);
	Data->TotalKnockouts += FMath::Max(0, Knockouts);
	Data->TotalDeaths += FMath::Max(0, Deaths);
	Data->BestComboHits = FMath::Max(Data->BestComboHits, FMath::Max(0, MaxComboHits));
	while (Data->Experience >= FMath::Max(1000, Data->AccountLevel * 1000))
	{
		Data->Experience -= FMath::Max(1000, Data->AccountLevel * 1000);
		++Data->AccountLevel;
	}

	if (!SavePlayerData())
	{
		Data->AccountLevel = PreviousLevel;
		Data->Experience = PreviousExperience;
		Data->Gold = PreviousGold;
		Data->EventPoints = PreviousEventPoints;
		Data->MatchesPlayed = PreviousMatchesPlayed;
		Data->MatchesWon = PreviousMatchesWon;
		Data->MatchesLost = PreviousMatchesLost;
		Data->MatchesDrawn = PreviousMatchesDrawn;
		Data->TotalDamageDealt = PreviousTotalDamageDealt;
		Data->TotalDamageReceived = PreviousTotalDamageReceived;
		Data->TotalKnockouts = PreviousTotalKnockouts;
		Data->TotalDeaths = PreviousTotalDeaths;
		Data->BestComboHits = PreviousBestComboHits;
		return FUBFMatchRewardResult();
	}

	Result.bSaved = true;
	Result.NewAccountLevel = Data->AccountLevel;
	NotifyDataChanged();
	UE_LOG(UBFPlayerData::LogUBFPlayerData, Log,
		TEXT("Saved match record: %d played (%dW/%dL/%dD), %d KOs, %d deaths, best combo %d, damage %.0f/%.0f. Rewards: +%d XP, +%d Gold, +%d Event Points (level %d)."),
		Data->MatchesPlayed, Data->MatchesWon, Data->MatchesLost, Data->MatchesDrawn,
		Data->TotalKnockouts, Data->TotalDeaths, Data->BestComboHits,
		Data->TotalDamageDealt, Data->TotalDamageReceived,
		Result.Experience, Result.Gold, Result.EventPoints, Result.NewAccountLevel);
	return Result;
}

bool UUBFPlayerDataSubsystem::RemoveGold(int32 Amount)
{
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || Amount <= 0 || Data->Gold < Amount)
	{
		return false;
	}
	Data->Gold -= Amount;
	SavePlayerData();
	NotifyDataChanged();
	return true;
}

void UUBFPlayerDataSubsystem::AddEventPoints(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	if (UUBFPlayerSaveGame* Data = EnsureData())
	{
		Data->EventPoints += Amount;
		SavePlayerData();
		NotifyDataChanged();
	}
}

bool UUBFPlayerDataSubsystem::RemoveEventPoints(int32 Amount)
{
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data || Amount <= 0 || Data->EventPoints < Amount)
	{
		return false;
	}
	Data->EventPoints -= Amount;
	SavePlayerData();
	NotifyDataChanged();
	return true;
}

void UUBFPlayerDataSubsystem::AddGachaTickets(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	if (UUBFPlayerSaveGame* Data = EnsureData())
	{
		Data->GachaTickets += Amount;
		SavePlayerData();
		NotifyDataChanged();
	}
}

void UUBFPlayerDataSubsystem::AddItem(const FString& ItemName)
{
	const FString CleanItemName = ItemName.TrimStartAndEnd();
	if (CleanItemName.IsEmpty())
	{
		return;
	}
	if (UUBFPlayerSaveGame* Data = EnsureData())
	{
		Data->InventoryItems.Add(CleanItemName);
		SavePlayerData();
		NotifyDataChanged();
	}
}

FUBFShopPurchaseResult UUBFPlayerDataSubsystem::PurchasePrototypeShopItem(const FString& RawItemId)
{
	FUBFShopPurchaseResult Result;
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data)
	{
		Result.Message = TEXT("No se pudo cargar el perfil local.");
		return Result;
	}

	const FString ItemId = RawItemId.TrimStartAndEnd().ToLower();
	FString ItemName;
	int32 Price = 0;
	bool bUsesEventPoints = false;
	if (ItemId == TEXT("basic_armor"))
	{
		ItemName = TEXT("Armadura básica");
		Price = 1200;
	}
	else if (ItemId == TEXT("basic_ring"))
	{
		ItemName = TEXT("Anillo básico");
		Price = 850;
	}
	else if (ItemId == TEXT("basic_necklace"))
	{
		ItemName = TEXT("Collar básico");
		Price = 1500;
	}
	else if (ItemId == TEXT("event_ring"))
	{
		ItemName = TEXT("Anillo de evento");
		Price = 500;
		bUsesEventPoints = true;
	}
	else if (ItemId == TEXT("event_necklace"))
	{
		ItemName = TEXT("Collar de evento");
		Price = 800;
		bUsesEventPoints = true;
	}
	else if (ItemId == TEXT("event_profile_frame"))
	{
		ItemName = TEXT("Marco de perfil de evento");
		Price = 350;
		bUsesEventPoints = true;
	}
	else
	{
		Result.Message = TEXT("El producto no está disponible.");
		return Result;
	}

	if (Data->InventoryItems.Contains(ItemName))
	{
		Result.Message = TEXT("Este producto ya está en tu inventario.");
		Result.ItemName = ItemName;
		return Result;
	}

	int32& Balance = bUsesEventPoints ? Data->EventPoints : Data->Gold;
	if (Balance < Price)
	{
		Result.Message = bUsesEventPoints ? TEXT("No tienes suficientes Puntos de Evento.") : TEXT("No tienes suficiente oro.");
		Result.ItemName = ItemName;
		return Result;
	}

	const int32 PreviousBalance = Balance;
	const TArray<FString> PreviousInventory = Data->InventoryItems;
	Balance -= Price;
	Data->InventoryItems.Add(ItemName);
	if (!SavePlayerData())
	{
		Balance = PreviousBalance;
		Data->InventoryItems = PreviousInventory;
		Result.Message = TEXT("No se pudo guardar la compra; no se descontó la moneda.");
		Result.ItemName = ItemName;
		return Result;
	}

	Result.bSucceeded = true;
	Result.ItemName = ItemName;
	Result.Message = FString::Printf(TEXT("Compraste %s por %s %s."), *ItemName,
		*FText::AsNumber(Price).ToString(), bUsesEventPoints ? TEXT("Puntos de Evento") : TEXT("oro"));
	NotifyDataChanged();
	UE_LOG(UBFPlayerData::LogUBFPlayerData, Log, TEXT("Shop purchase saved: %s for %d %s."), *ItemName,
		Price, bUsesEventPoints ? TEXT("event points") : TEXT("gold"));
	return Result;
}

FUBFGiftCodeResult UUBFPlayerDataSubsystem::RedeemGiftCode(const FString& RawCode)
{
	FUBFGiftCodeResult Result;
	UUBFPlayerSaveGame* Data = EnsureData();
	if (!Data)
	{
		Result.Message = TEXT("No se pudo cargar el perfil local.");
		return Result;
	}

	const FString Code = UBFPlayerData::NormalizeGiftCode(RawCode);
	if (Code.IsEmpty())
	{
		Result.Message = TEXT("Escribe un código de regalo.");
		return Result;
	}
	if (Data->RedeemedCodes.Contains(Code))
	{
		Result.Message = TEXT("Este código ya fue canjeado en este perfil.");
		return Result;
	}

	// Keep redemption transactional: a failed local save must not leave the reward
	// applied in memory while reporting success to the player.
	const int32 PreviousGold = Data->Gold;
	const int32 PreviousEventPoints = Data->EventPoints;
	const int32 PreviousGachaTickets = Data->GachaTickets;
	const TArray<FString> PreviousItems = Data->InventoryItems;
	const TSet<FString> PreviousRedeemedCodes = Data->RedeemedCodes;

	TArray<FString> Rewards;
	if (Code == TEXT("OPENWELCOME2"))
	{
		Data->Gold += 10000;
		Data->EventPoints += 500;
		Rewards = { TEXT("+10,000 ORO"), TEXT("+500 PUNTOS DE EVENTO") };
	}
	else if (Code == TEXT("INEEDMONEY21K"))
	{
		Data->Gold += 21000;
		Data->EventPoints += 250;
		Rewards = { TEXT("+21,000 ORO"), TEXT("+250 PUNTOS DE EVENTO") };
	}
	else if (Code == TEXT("OPENBETALESTGO"))
	{
		Data->Gold += 15000;
		Data->EventPoints += 1000;
		Data->GachaTickets += 10;
		Rewards = { TEXT("+15,000 ORO"), TEXT("+1,000 PUNTOS DE EVENTO"), TEXT("+10 TICKETS DE GACHA") };
	}
	else if (Code == TEXT("MAKEONLYFAI23"))
	{
		const TArray<FString> Rings = { TEXT("Anillo de Obsidiana"), TEXT("Anillo de Cristal"), TEXT("Anillo del Vórtice") };
		const TArray<FString> Necklaces = { TEXT("Collar del Eclipse"), TEXT("Collar del Guardián"), TEXT("Collar de Aurora") };
		const FString Ring = Rings[FMath::RandRange(0, Rings.Num() - 1)];
		const FString Necklace = Necklaces[FMath::RandRange(0, Necklaces.Num() - 1)];
		Data->InventoryItems.Add(Ring);
		Data->InventoryItems.Add(Necklace);
		Data->EventPoints += 500;
		Rewards = { FString::Printf(TEXT("+%s"), *Ring), FString::Printf(TEXT("+%s"), *Necklace), TEXT("+500 PUNTOS DE EVENTO") };
	}
	else if (Code == TEXT("RPMODSGAMESBONUS"))
	{
		Data->Gold += 30000;
		Data->EventPoints += 2000;
		Data->GachaTickets += 100;
		Rewards = { TEXT("+30,000 ORO"), TEXT("+2,000 PUNTOS DE EVENTO"), TEXT("+100 TICKETS DE GACHA") };
	}
	else
	{
		Result.Message = TEXT("El código no existe o no está activo.");
		return Result;
	}

	Data->RedeemedCodes.Add(Code);
	if (!SavePlayerData())
	{
		Data->Gold = PreviousGold;
		Data->EventPoints = PreviousEventPoints;
		Data->GachaTickets = PreviousGachaTickets;
		Data->InventoryItems = PreviousItems;
		Data->RedeemedCodes = PreviousRedeemedCodes;
		Result.Message = TEXT("No se pudo guardar el perfil. No se aplicaron las recompensas; inténtalo de nuevo.");
		return Result;
	}
	NotifyDataChanged();
	Result.bSucceeded = true;
	Result.Message = TEXT("CÓDIGO CANJEADO");
	Result.RewardSummary = FString::Join(Rewards, TEXT("\n"));
	UE_LOG(UBFPlayerData::LogUBFPlayerData, Log, TEXT("Código de regalo canjeado: %s"), *Code);
	return Result;
}

void UUBFPlayerDataSubsystem::NotifyDataChanged()
{
	OnPlayerDataChanged.Broadcast();
}
