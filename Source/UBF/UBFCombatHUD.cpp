#include "UBFCombatHUD.h"

#include "UBFCombatGameState.h"
#include "UBFPlayerData.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UBFCombatCharacter.h"

void AUBFCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;
	const FLinearColor Panel(0.012f, 0.020f, 0.032f, 0.88f);
	const FLinearColor White(0.92f, 0.95f, 0.98f, 1.0f);
	const FLinearColor Muted(0.60f, 0.68f, 0.76f, 1.0f);
	const FLinearColor Cyan(0.05f, 0.72f, 0.92f, 1.0f);
	const FLinearColor Crimson(0.88f, 0.035f, 0.12f, 1.0f);

	DrawRect(Panel, 34.0f, 32.0f, 360.0f, 132.0f);
	DrawText(TEXT("UBF  //  TRAINING ARENA"), White, 54.0f, 48.0f, GEngine->GetMediumFont(), 0.75f);
	DrawText(TEXT("SERVER-AUTHORITATIVE COMBAT"), Muted, 54.0f, 78.0f, GEngine->GetSmallFont(), 0.9f);

	AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(PlayerOwner ? PlayerOwner->GetPawn() : nullptr);
	const AUBFCombatGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	if (MatchState && MatchState->IsGolemMode())
	{
		const float ObjectiveWidth = 340.0f;
		const float ObjectiveX = (ScreenWidth - ObjectiveWidth) * 0.5f;
		const float ObjectiveY = ScreenWidth >= 1100.0f ? 32.0f : 170.0f;
		DrawRect(Panel, ObjectiveX, ObjectiveY, ObjectiveWidth, 58.0f);
		DrawRect(FLinearColor(1.0f, 0.62f, 0.08f, 1.0f), ObjectiveX, ObjectiveY, ObjectiveWidth, 3.0f);
		DrawText(FString::Printf(TEXT("GOLEM MODE    %d  :  %d"),
			MatchState->GetTeamScore(0), MatchState->GetTeamScore(1)), White,
			ObjectiveX + 82.0f, ObjectiveY + 14.0f, GEngine->GetMediumFont(), 0.9f);
		if (Fighter && Fighter->IsGoldenBearer())
		{
			DrawText(TEXT("GOLDEN BEARER  ·  SOBREVIVE PARA MARCAR"),
				FLinearColor(1.0f, 0.72f, 0.16f, 1.0f), ObjectiveX + 28.0f, ObjectiveY + 40.0f,
				GEngine->GetSmallFont(), 0.72f);
		}
		if (Fighter)
		{
			for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
			{
				const AUBFCombatCharacter* EnemyMaster = *It;
				if (!EnemyMaster || !EnemyMaster->IsMasterGolem() || EnemyMaster->GetTeamId() == Fighter->GetTeamId())
				{
					continue;
				}
				const float BarX = (ScreenWidth - 420.0f) * 0.5f;
				DrawRect(Panel, BarX, ObjectiveY + 66.0f, 420.0f, 38.0f);
				DrawText(FString::Printf(TEXT("ENEMY MASTER GOLEM  ·  %03.0f HP"), EnemyMaster->GetCurrentHealth()),
					White, BarX + 14.0f, ObjectiveY + 72.0f, GEngine->GetSmallFont(), 0.78f);
				DrawRect(FLinearColor(0.18f, 0.10f, 0.11f, 1.0f), BarX + 14.0f, ObjectiveY + 91.0f, 392.0f, 5.0f);
				DrawRect(Crimson, BarX + 14.0f, ObjectiveY + 91.0f, 392.0f * EnemyMaster->GetHealthRatio(), 5.0f);
				break;
			}
		}
	}
	if (Fighter)
	{
		const float BarWidth = 246.0f;
		const FLinearColor HealthColor = Fighter->GetHealthRatio() > 0.30f ? Cyan : Crimson;
		DrawRect(FLinearColor(0.11f, 0.13f, 0.17f, 1.0f), 54.0f, 104.0f, BarWidth, 11.0f);
		DrawRect(HealthColor, 54.0f, 104.0f, BarWidth * Fighter->GetHealthRatio(), 11.0f);
		DrawText(FString::Printf(TEXT("HP %03.0f"), Fighter->GetCurrentHealth()), White,
			312.0f, 101.0f, GEngine->GetSmallFont(), 0.75f);

		DrawRect(FLinearColor(0.11f, 0.13f, 0.17f, 1.0f), 54.0f, 128.0f, BarWidth, 11.0f);
		DrawRect(Cyan, 54.0f, 128.0f, BarWidth * Fighter->GetSkillGaugeRatio(), 11.0f);
		DrawText(FString::Printf(TEXT("GAUGE %03.0f%%"), Fighter->GetCurrentSkillGauge()), Muted,
			312.0f, 125.0f, GEngine->GetSmallFont(), 0.75f);

		if (ScreenWidth >= 760.0f)
		{
			const float PanelX = ScreenWidth - 338.0f;
			DrawRect(Panel, PanelX, 32.0f, 304.0f, 124.0f);
			DrawRect(Cyan, PanelX, 32.0f, 3.0f, 124.0f);
			DrawText(FString::Printf(TEXT("FIGHTER  //  %s"), *Fighter->GetCharacterDisplayName().ToString()),
				White, PanelX + 18.0f, 44.0f, GEngine->GetMediumFont(), 0.78f);
			for (uint8 Slot = 0; Slot < 3; ++Slot)
			{
				const float RowY = 74.0f + static_cast<float>(Slot) * 24.0f;
				const float CooldownRatio = Fighter->GetAbilityCooldownRatio(Slot);
				const float Remaining = Fighter->GetAbilityCooldownRemaining(Slot);
				const FName ActionId = Slot == 0 ? FName(TEXT("PrimarySkill"))
					: (Slot == 1 ? FName(TEXT("SecondarySkill")) : FName(TEXT("Ultimate")));
				const TCHAR* DefaultLabel = Slot == 0 ? TEXT("Q") : (Slot == 1 ? TEXT("E") : TEXT("F"));
			const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
				? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
			const FString InputLabel = PlayerData
				? PlayerData->GetInputBinding(ActionId).GetDisplayName().ToString() : DefaultLabel;
				const FString AbilityName = Fighter->GetAbilityDisplayName(Slot).ToString();
				DrawText(FString::Printf(TEXT("%s  %s"), *InputLabel, *AbilityName), Muted,
					PanelX + 18.0f, RowY, GEngine->GetSmallFont(), 0.76f);
				DrawText(Remaining > 0.05f ? FString::Printf(TEXT("%.1fs"), Remaining) : TEXT("READY"),
					Remaining > 0.05f ? Crimson : Cyan, PanelX + 246.0f, RowY, GEngine->GetSmallFont(), 0.72f);
				DrawRect(FLinearColor(0.10f, 0.15f, 0.20f, 1.0f), PanelX + 18.0f, RowY + 15.0f, 268.0f, 3.0f);
				DrawRect(Cyan, PanelX + 18.0f, RowY + 15.0f, 268.0f * (1.0f - CooldownRatio), 3.0f);
			}
		}

		const FString Feedback = Fighter->GetCombatFeedbackMessage();
		if (!Feedback.IsEmpty() && !Fighter->HasMatchResult())
		{
			DrawText(Feedback, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f), ScreenWidth * 0.5f - 120.0f,
				ScreenHeight * 0.78f, GEngine->GetMediumFont(), 1.0f);
		}
	}

	const bool bShowingMatchResult = Fighter && Fighter->HasMatchResult();
	if (bShowingMatchResult)
	{
		const float ResultWidth = FMath::Min(500.0f, ScreenWidth - 48.0f);
		const float ResultHeight = 420.0f;
		const float ResultX = (ScreenWidth - ResultWidth) * 0.5f;
		const float ResultY = (ScreenHeight - ResultHeight) * 0.5f;
		const FString& Result = Fighter->GetMatchResultLabel();
		const FLinearColor ResultColor = Result == TEXT("VICTORIA") ? Cyan
			: (Result == TEXT("DERROTA") ? Crimson : FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));

		DrawRect(FLinearColor(0.008f, 0.014f, 0.025f, 0.96f), ResultX, ResultY, ResultWidth, ResultHeight);
		DrawRect(ResultColor, ResultX, ResultY, ResultWidth, 4.0f);
		DrawText(Result, ResultColor, ResultX + 38.0f, ResultY + 26.0f, GEngine->GetLargeFont(), 1.35f);
		DrawText(TEXT("MATCH RESULT"), Muted, ResultX + 40.0f, ResultY + 78.0f, GEngine->GetSmallFont(), 0.9f);

		const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(Fighter->GetMatchDuration()));
		const FString MatchTime = FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60);
		const FString StatLabels[] = { TEXT("KNOCKOUTS"), TEXT("DEATHS"), TEXT("DAMAGE DEALT"),
			TEXT("DAMAGE RECEIVED"), TEXT("MAX COMBO"), TEXT("MATCH TIME"),
			TEXT("XP REWARD"), TEXT("GOLD REWARD"), TEXT("EVENT POINTS") };
		const FString StatValues[] = {
			FString::FromInt(Fighter->GetMatchKnockouts()),
			FString::FromInt(Fighter->GetMatchDeaths()),
			FString::Printf(TEXT("%.0f"), Fighter->GetMatchDamageDealt()),
			FString::Printf(TEXT("%.0f"), Fighter->GetMatchDamageReceived()),
			FString::Printf(TEXT("%d HITS"), Fighter->GetMaximumComboHits()),
			MatchTime,
			FString::Printf(TEXT("+%d"), Fighter->GetMatchExperienceReward()),
			FString::Printf(TEXT("+%d"), Fighter->GetMatchGoldReward()),
			FString::Printf(TEXT("+%d"), Fighter->GetMatchEventReward())
		};
		DrawRect(FLinearColor(0.42f, 0.32f, 0.12f, 0.9f), ResultX + 40.0f, ResultY + 301.0f, ResultWidth - 80.0f, 1.0f);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(StatLabels); ++Index)
		{
			const float RowY = ResultY + 110.0f + Index * 30.0f;
			const FLinearColor RowColor = Index >= 6 ? FLinearColor(1.0f, 0.78f, 0.28f, 1.0f) : Muted;
			DrawText(StatLabels[Index], RowColor, ResultX + 40.0f, RowY, GEngine->GetSmallFont(), 0.9f);
			DrawText(StatValues[Index], White, ResultX + ResultWidth - 150.0f, RowY, GEngine->GetSmallFont(), 0.9f);
		}
	}

	const float InstructionsY = ScreenHeight - 70.0f;
	DrawRect(Panel, 34.0f, InstructionsY - 10.0f, ScreenWidth - 68.0f, 44.0f);
	FString Instructions = TEXT("ESC  RETURN TO MAIN MENU");
	if (!bShowingMatchResult)
	{
		const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
		const FString PrimaryKey = PlayerData ? PlayerData->GetInputBinding(TEXT("PrimarySkill")).GetDisplayName().ToString() : TEXT("Q");
		const FString SecondaryKey = PlayerData ? PlayerData->GetInputBinding(TEXT("SecondarySkill")).GetDisplayName().ToString() : TEXT("E");
		const FString UltimateKey = PlayerData ? PlayerData->GetInputBinding(TEXT("Ultimate")).GetDisplayName().ToString() : TEXT("F");
		Instructions = FString::Printf(TEXT("WASD MOVE   SPACE JUMP   CTRL DASH   LMB ATTACK   LMB+RMB GRAB   RMB CHARGE   PRIMARY %s   SECONDARY %s   ULTIMATE %s   ESC MENU"),
			*PrimaryKey, *SecondaryKey, *UltimateKey);
	}
	DrawText(Instructions, White, 54.0f, InstructionsY, GEngine->GetSmallFont(), 0.85f);
}
