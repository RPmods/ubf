#include "UBFBotAIController.h"

#include "UBFCombatCharacter.h"
#include "UBFCombatGameMode.h"
#include "UBFCharacterDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AUBFBotAIController::AUBFBotAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	// CharacterMovement consumes input every frame, so the lightweight bot
	// controller also needs to run every frame to keep movement responsive.
}

void AUBFBotAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	if (const AUBFCombatGameMode* MatchMode = GetWorld()->GetAuthGameMode<AUBFCombatGameMode>())
	{
		if (MatchMode->IsMatchFinished())
		{
			return;
		}
	}

	AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(GetPawn());
	if (!Fighter || Fighter->GetCurrentHealth() <= 0.0f || !Fighter->CanFight())
	{
		return;
	}
	const UUBFCharacterAIProfile* Profile = Fighter->GetCharacterAIProfile();

	const float Now = GetWorld()->GetTimeSeconds();
	AUBFCombatCharacter* Target = CurrentTarget.Get();
	if (Now >= NextTargetSearchTime || !IsValid(Target) || Target->GetCurrentHealth() <= 0.0f
		|| Target->GetTeamId() == Fighter->GetTeamId())
	{
		AUBFCombatCharacter* PreviousTarget = Target;
		Target = FindNearestEnemy(Fighter);
		CurrentTarget = Target;
		NextTargetSearchTime = Now + 0.35f;
		if (Target != PreviousTarget)
		{
			float ReactionDelay = Profile ? FMath::Max(0.0f, Profile->ReactionDelay) : 0.25f;
			if (Difficulty == EUBFBotDifficulty::Easy)
			{
				ReactionDelay *= 1.55f;
			}
			else if (Difficulty == EUBFBotDifficulty::Hard)
			{
				ReactionDelay *= 0.72f;
			}
			ReactionReadyTime = Now + ReactionDelay;
			LastProgressTime = Now;
			LastObservedDistance = TNumericLimits<float>::Max();
			DetourEndTime = 0.0f;
			NextDetourSide = (GetUniqueID() & 1) == 0 ? 1.0f : -1.0f;
			if (Target)
			{
				UE_LOG(LogTemp, Verbose, TEXT("UBF bot %s selected enemy %s (team %d -> %d)."),
					*GetNameSafe(Fighter), *GetNameSafe(Target), Fighter->GetTeamId(), Target->GetTeamId());
			}
		}
	}
	if (Now < ReactionReadyTime)
	{
		return;
	}

	if (!Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Fighter->GetActorLocation();
	ToTarget.Z = 0.0f;
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const bool bDetouring = Now < DetourEndTime;
	if (!bDetouring && Distance < LastObservedDistance - 2.0f)
	{
		LastProgressTime = Now;
	}
	LastObservedDistance = Distance;
	if (Now >= NextDiagnosticsTime)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UBF bot %s status: distance=%.0f, speed=%.0f, position=%s, target=%s."),
			*GetNameSafe(Fighter), Distance, Fighter->GetVelocity().Size2D(),
			*Fighter->GetActorLocation().ToCompactString(), *Target->GetActorLocation().ToCompactString());
		NextDiagnosticsTime = Now + 2.0f;
	}

	const FRotator Facing(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	SetControlRotation(Facing);
	const float DodgeRange = FMath::Max(Fighter->GetBasicAttackReach() * 1.45f, 360.0f);
	const bool bTargetThreatening = Target->IsChargingSpecial()
		|| Target->GetTimeSinceBasicAttack() <= 0.16f;
	if (Profile && Now >= NextDodgeEvaluationTime && bTargetThreatening && Distance <= DodgeRange)
	{
		float DodgeChance = FMath::Clamp(Profile->DodgeReaction, 0.0f, 1.0f);
		if (Fighter->GetHealthRatio() <= Profile->RetreatThreshold)
		{
			DodgeChance = FMath::Min(1.0f, DodgeChance + FMath::Clamp(Profile->RecoveryBehavior, 0.0f, 1.0f) * 0.25f);
		}
		if (Difficulty == EUBFBotDifficulty::Easy) DodgeChance *= 0.65f;
		else if (Difficulty == EUBFBotDifficulty::Hard) DodgeChance = FMath::Min(1.0f, DodgeChance + 0.18f);

		NextDodgeEvaluationTime = Now + FMath::Lerp(0.45f, 0.20f, DodgeChance);
		if (FMath::FRand() <= DodgeChance)
		{
			const FVector Away = -ToTarget / Distance;
			const FVector Lateral = FVector::CrossProduct(ToTarget / Distance, FVector::UpVector).GetSafeNormal()
				* NextDetourSide;
			NextDetourSide *= -1.0f;
			if (Fighter->PerformBotDash((Away * 0.65f + Lateral * 0.75f).GetSafeNormal()))
			{
				NextAttackTime = FMath::Max(NextAttackTime, Now + 0.25f);
				return;
			}
		}
	}

	if (Now >= NextAttackTime)
	{
		float AttackFrequency = Profile ? FMath::Clamp(Profile->AttackFrequency, 0.0f, 1.0f) : 0.5f;
		float SkillPriority = Profile ? FMath::Clamp(Profile->SkillPriority, 0.0f, 1.0f) : 0.5f;
		float UltimatePriority = Profile ? FMath::Clamp(Profile->UltimatePriority, 0.0f, 1.0f) : 0.5f;
		if (Difficulty == EUBFBotDifficulty::Easy)
		{
			AttackFrequency *= 0.72f;
			SkillPriority *= 0.62f;
			UltimatePriority *= 0.62f;
		}
		else if (Difficulty == EUBFBotDifficulty::Hard)
		{
			AttackFrequency = FMath::Min(1.0f, AttackFrequency + 0.18f);
			SkillPriority = FMath::Min(1.0f, SkillPriority + 0.16f);
			UltimatePriority = FMath::Min(1.0f, UltimatePriority + 0.12f);
		}
		const float RetryDelay = FMath::Lerp(1.15f, 0.48f, AttackFrequency);
		if (Fighter->GetCurrentSkillGauge() >= Fighter->GetAbilityGaugeCost(2)
			&& FMath::FRand() <= UltimatePriority
			&& Distance <= Fighter->GetAbilityMaximumRange(2))
		{
			Fighter->PerformBotAbility(2);
			NextAttackTime = Now + RetryDelay;
			return;
		}
		if (Fighter->GetCurrentSkillGauge() >= Fighter->GetAbilityGaugeCost(1)
			&& FMath::FRand() <= SkillPriority
			&& Distance <= Fighter->GetAbilityMaximumRange(1))
		{
			Fighter->PerformBotAbility(1);
			NextAttackTime = Now + RetryDelay;
			return;
		}
		if (Fighter->GetCurrentSkillGauge() >= Fighter->GetAbilityGaugeCost(0)
			&& FMath::FRand() <= SkillPriority
			&& Distance <= Fighter->GetAbilityMaximumRange(0))
		{
			Fighter->PerformBotAbility(0);
			NextAttackTime = Now + RetryDelay;
			return;
		}
	}

	if (Distance <= Fighter->GetBasicAttackReach())
	{
		if (Now >= NextAttackTime)
		{
			const float HealthBeforeAttack = Target->GetCurrentHealth();
			Fighter->PerformBotBasicAttack(Target);
			if (Target->GetCurrentHealth() < HealthBeforeAttack)
			{
				UE_LOG(LogTemp, Verbose, TEXT("UBF bot %s hit %s; target health %.0f -> %.0f."),
					*GetNameSafe(Fighter), *GetNameSafe(Target), HealthBeforeAttack, Target->GetCurrentHealth());
			}
		float AttackFrequency = Profile ? FMath::Clamp(Profile->AttackFrequency, 0.0f, 1.0f) : 0.5f;
		if (Difficulty == EUBFBotDifficulty::Easy) AttackFrequency *= 0.72f;
		else if (Difficulty == EUBFBotDifficulty::Hard) AttackFrequency = FMath::Min(1.0f, AttackFrequency + 0.18f);
			NextAttackTime = Now + FMath::Lerp(0.95f, 0.50f, AttackFrequency);
		}
		return;
	}

	const float RecoveryThreshold = Profile
		? FMath::Clamp(Profile->RetreatThreshold + Profile->RecoveryBehavior * 0.08f, 0.0f, 0.95f)
		: 0.0f;
	if (Profile && Fighter->GetHealthRatio() <= RecoveryThreshold
		&& Profile->Aggression < 0.7f && Distance < Profile->PreferredDistance)
	{
		Fighter->AddMovementInput(-ToTarget / Distance, 0.75f, true);
		return;
	}

	if (bDetouring)
	{
		Fighter->AddMovementInput(DetourDirection, 1.0f, true);
		return;
	}

	// The blockout arena contains central geometry. If direct pursuit stops
	// reducing distance, briefly strafe around the obstruction before resuming.
	if (Now - LastProgressTime > 0.55f)
	{
		DetourDirection = FVector::CrossProduct(ToTarget / Distance, FVector::UpVector).GetSafeNormal() * NextDetourSide;
		NextDetourSide *= -1.0f;
		DetourEndTime = Now + 0.65f;
		LastProgressTime = DetourEndTime + 0.15f;
		UE_LOG(LogTemp, Verbose, TEXT("UBF bot %s is routing around an obstruction."), *GetNameSafe(Fighter));
		Fighter->AddMovementInput(DetourDirection, 1.0f, true);
		return;
	}

	Fighter->AddMovementInput(ToTarget / Distance, 1.0f, true);
}

AUBFCombatCharacter* AUBFBotAIController::FindNearestEnemy(AUBFCombatCharacter* Fighter) const
{
	if (!Fighter || !GetWorld())
	{
		return nullptr;
	}

	constexpr float MaximumTargetDistance = 10000.0f;
	float BestDistanceSquared = FMath::Square(MaximumTargetDistance);
	AUBFCombatCharacter* BestTarget = nullptr;
	const UUBFCharacterAIProfile* Profile = Fighter->GetCharacterAIProfile();
	AUBFCombatCharacter* FriendlyBearer = nullptr;
	if (Profile && Profile->AllyProtection > 0.0f)
	{
		for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
		{
			AUBFCombatCharacter* Candidate = *It;
			if (Candidate && Candidate->IsGoldenBearer() && Candidate->GetTeamId() == Fighter->GetTeamId())
			{
				FriendlyBearer = Candidate;
				break;
			}
		}
	}
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Candidate = *It;
		if (!Candidate || Candidate == Fighter || Candidate->GetCurrentHealth() <= 0.0f
			|| Candidate->GetTeamId() == Fighter->GetTeamId())
		{
			continue;
		}
		if (Candidate->IsMasterGolem() && !Fighter->IsGoldenBearer())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Fighter->GetActorLocation(), Candidate->GetActorLocation());
		float TargetPriority = Profile ? FMath::Clamp(Profile->TargetPriority, 0.0f, 1.0f) : 0.0f;
		if (Difficulty == EUBFBotDifficulty::Easy) TargetPriority *= 0.75f;
		else if (Difficulty == EUBFBotDifficulty::Hard) TargetPriority = FMath::Min(1.0f, TargetPriority + 0.18f);
		const float WeaknessBonus = (1.0f - Candidate->GetHealthRatio()) * TargetPriority * 0.35f;
		float ScoreMultiplier = 1.0f - WeaknessBonus;
		if (Profile && Candidate->IsGoldenBearer())
		{
			ScoreMultiplier *= 1.0f - FMath::Clamp(Profile->GoldenBearerPriority, 0.0f, 1.0f) * 0.60f;
		}
		if (Profile && Candidate->IsMasterGolem())
		{
			ScoreMultiplier *= 1.0f - FMath::Clamp(Profile->MasterGolemPriority, 0.0f, 1.0f) * 0.45f;
		}
		if (Profile && FriendlyBearer)
		{
			const float EnemyToBearer = FVector::Dist2D(Candidate->GetActorLocation(), FriendlyBearer->GetActorLocation());
			const float ThreatToBearer = 1.0f - FMath::Clamp(EnemyToBearer / 1600.0f, 0.0f, 1.0f);
			ScoreMultiplier *= 1.0f - FMath::Clamp(Profile->AllyProtection, 0.0f, 1.0f) * ThreatToBearer * 0.25f;
		}
		const float ScoredDistanceSquared = DistanceSquared * FMath::Max(0.10f, ScoreMultiplier);
		if (ScoredDistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = ScoredDistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}
