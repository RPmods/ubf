#include "UBFPlayerData.h"

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
	SavePlayerData();
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
