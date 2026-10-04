#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "UBFCombatCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UUBFAbilityDefinition;
class UUBFCharacterAIProfile;
class UUBFCharacterDefinition;
class USpringArmComponent;

/** Server-authoritative prototype fighter used by the local arena slice. */
UCLASS()
class UBF_API AUBFCombatCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUBFCombatCharacter();

	virtual void BeginPlay() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	float GetHealthRatio() const;

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	float GetSkillGaugeRatio() const;

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	float GetCurrentSkillGauge() const { return CurrentSkillGauge; }

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	FString GetCombatFeedbackMessage() const;
	bool HasMatchResult() const { return bHasMatchResult; }
	const FString& GetMatchResultLabel() const { return MatchResultLabel; }
	float GetMatchDamageDealt() const { return MatchDamageDealt; }
	float GetMatchDamageReceived() const { return MatchDamageReceived; }
	int32 GetMatchKnockouts() const { return MatchKnockouts; }
	int32 GetMatchDeaths() const { return MatchDeaths; }
	int32 GetMaximumComboHits() const { return MaximumComboHits; }
	float GetMatchDuration() const { return MatchDurationSeconds; }
	int32 GetMatchExperienceReward() const { return MatchExperienceReward; }
	int32 GetMatchGoldReward() const { return MatchGoldReward; }
	int32 GetMatchEventReward() const { return MatchEventReward; }
	FName GetCharacterId() const { return CharacterId; }
	bool IsGoldenBearer() const { return bIsGoldenBearer; }
	bool IsMasterGolem() const { return bIsMasterGolem; }
	FText GetCharacterDisplayName() const;
	const UUBFCharacterAIProfile* GetCharacterAIProfile() const;

	UFUNCTION(BlueprintPure, Category="UBF|Combat")
	int32 GetTeamId() const { return TeamId; }

	float GetBasicAttackReach() const { return BasicAttackRange + BasicAttackRadius + 45.0f; }
	float GetAbilityMaximumRange(uint8 AbilitySlot) const;
	float GetAbilityGaugeCost(uint8 AbilitySlot) const;
	float GetAbilityCooldownRemaining(uint8 AbilitySlot) const;
	float GetAbilityCooldownRatio(uint8 AbilitySlot) const;
	FText GetAbilityDisplayName(uint8 AbilitySlot) const;
	bool IsChargingSpecial() const { return bIsChargingSpecial; }
	float GetTimeSinceBasicAttack() const;

	void SetTeamId(int32 NewTeamId);
	void PerformBotBasicAttack(AUBFCombatCharacter* Target);
	void PerformBotAbility(uint8 AbilitySlot);
	bool PerformBotDash(const FVector& Direction);
	void SetCharacterId(FName NewCharacterId);
	void SetGoldenBearer(bool bNewValue);
	void SetAsMasterGolem();
	void ShowCombatMessage(const FString& Message, float Duration) { ClientShowCombatMessage(Message, Duration); }
	void ShowMatchResultMessage(const FString& Message, float DurationSeconds,
		int32 ExperienceReward, int32 GoldReward, int32 EventReward);

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestBasicAttack();

	UFUNCTION(Server, Reliable)
	void ServerRequestAbility(uint8 AbilitySlot);

	UFUNCTION(Server, Reliable)
	void ServerRequestDash();

	UFUNCTION(Server, Reliable)
	void ServerRequestGrab();

	UFUNCTION(Server, Reliable)
	void ServerStartChargedSpecial();

	UFUNCTION(Server, Reliable)
	void ServerReleaseChargedSpecial();

	UFUNCTION(Server, Reliable)
	void ServerCancelChargedSpecial();

	UFUNCTION(Client, Reliable)
	void ClientShowCombatMessage(const FString& Message, float Duration);

	UFUNCTION(Client, Reliable)
	void ClientFinishMatch(const FString& Message, float DamageDealt, float DamageReceived,
		int32 Knockouts, int32 Deaths, int32 MaxComboHits, float DurationSeconds,
		int32 ExperienceReward, int32 GoldReward, int32 EventReward);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayActionAnimation(bool bWasHit, uint8 AttackStage);

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_CharacterId();

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void EnhancedMoveForward(const struct FInputActionValue& Value);
	void EnhancedMoveRight(const struct FInputActionValue& Value);
	void EnhancedTurn(const struct FInputActionValue& Value);
	void EnhancedLookUp(const struct FInputActionValue& Value);
	void EnhancedJumpStarted(const struct FInputActionValue& Value);
	void EnhancedJumpCompleted(const struct FInputActionValue& Value);
	void EnhancedBasicAttackStarted(const struct FInputActionValue& Value);
	void EnhancedBasicAttackCompleted(const struct FInputActionValue& Value);
	void EnhancedAbilityQ(const struct FInputActionValue& Value);
	void EnhancedAbilityE(const struct FInputActionValue& Value);
	void EnhancedUltimate(const struct FInputActionValue& Value);
	void EnhancedDash(const struct FInputActionValue& Value);
	void EnhancedChargedStarted(const struct FInputActionValue& Value);
	void EnhancedChargedCompleted(const struct FInputActionValue& Value);
	void EnhancedReturnToMenu(const struct FInputActionValue& Value);
	UInputAction* CreateRuntimeInputAction(const FName& ActionId, uint8 ValueType);
	void AddRuntimeInputMapping(class UInputAction* Action, const FKey& Key, bool bNegate = false);
	void RemoveRuntimeInputMapping();
	void HandleBasicAttackPressed();
	void HandleBasicAttackReleased();
	void DispatchPendingBasicAttack();
	void BeginLocalGrabGesture();
	void RequestAbilityQ();
	void RequestAbilityE();
	void RequestUltimate();
	void RequestDash();
	void HandleChargedSpecialPressed();
	void HandleChargedSpecialReleased();
	void PerformChargedSpecial(float ChargeSeconds);
	void ReturnToMenu();
	void AddSkillGauge(float Amount);
	void ApplyCharacterDefinition();
	const UUBFAbilityDefinition* GetAbilityDefinition(uint8 AbilitySlot) const;
	void UpdateLocomotionAnimation();
	bool CanFight() const;
	void RegisterConfirmedComboHit(float Now);
	void ResolveGrab();
	void PlayActionAnimation(bool bWasHit, uint8 AttackStage);
	void PerformBasicAttack();
	bool PerformDash(const FVector& Direction);
	bool ActivateAbility(uint8 AbilitySlot, bool bNotifyOwningPlayer);
	float GetOutgoingPassiveDamageMultiplier() const;

	UPROPERTY(VisibleAnywhere, Category="UBF|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category="UBF|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	TObjectPtr<UUBFCharacterDefinition> CharacterDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeInputMappingContext;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> RuntimeInputActions;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterId, VisibleAnywhere, Category="UBF|Character")
	FName CharacterId = TEXT("Wizz");

	UPROPERTY(ReplicatedUsing=OnRep_CurrentHealth, VisibleAnywhere, Category="UBF|Combat")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat")
	float MaximumHealth = 100.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Combat")
	int32 TeamId = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Combat")
	float CurrentSkillGauge = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	bool bIsGoldenBearer = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	bool bIsMasterGolem = false;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="1.0"))
	float SkillGaugeMaximum = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="0.0"))
	float GaugeBasicHitReward = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="0.0"))
	float GaugeComboHitReward = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="0.0"))
	float GaugeChargedHitReward = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="0.0"))
	float GaugeReceiveHitReward = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Skill Gauge", meta=(ClampMin="0.0"))
	float GaugeReceiveHeavyReward = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0", ClampMax="100.0"))
	float AbilityQCost = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0", ClampMax="100.0"))
	float AbilityECost = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0", ClampMax="100.0"))
	float UltimateCost = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityQDamage = 22.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityEDamage = 34.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float UltimateDamage = 48.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Movement", meta=(ClampMin="0.0"))
	float DashSpeed = 950.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Movement", meta=(ClampMin="0.0"))
	float DashCooldown = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Grab", meta=(ClampMin="0.0"))
	float GrabRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Grab", meta=(ClampMin="0.0"))
	float GrabDamage = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Grab", meta=(ClampMin="0.0"))
	float GrabStartupDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Grab", meta=(ClampMin="0.1"))
	float GrabRecoveryDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Charged Special", meta=(ClampMin="0.1"))
	float ChargedSpecialMaximumChargeTime = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Charged Special", meta=(ClampMin="0.0"))
	float ChargedSpecialMaximumRange = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Charged Special", meta=(ClampMin="0.0"))
	float ChargedSpecialBaseDamage = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Charged Special", meta=(ClampMin="0.0"))
	float ChargedSpecialFullChargeBonusDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Charged Special", meta=(ClampMin="0.0"))
	float ChargedSpecialHitRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityQMaximumRange = 480.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityEMaximumRange = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float UltimateMaximumRange = 980.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityQImpactOffset = 320.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityEImpactOffset = 490.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float UltimateImpactOffset = 690.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityQImpactRadius = 190.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityEImpactRadius = 285.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float UltimateImpactRadius = 390.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Abilities", meta=(ClampMin="0.0"))
	float AbilityRecoveryDuration = 0.85f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat")
	float BasicAttackDamage = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat")
	float BasicAttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat")
	float BasicAttackRadius = 85.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat")
	float BasicAttackCooldown = 0.48f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Combo", meta=(ClampMin="0.1"))
	float ComboResetWindow = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Combat|Combo", meta=(ClampMin="0.0"))
	float ComboBonusDamagePerHit = 3.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> AttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrossAttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> HookAttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ChargedAttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> HitAnimation;

	float LastBasicAttackTime = -100.0f;
	float LastConfirmedComboTime = -100.0f;
	float PendingDamageGaugeReward = 0.0f;
	float NextAbilityTimes[3] = { 0.0f, 0.0f, 0.0f };
	float AbilityCooldownDurations[3] = { 0.0f, 0.0f, 0.0f };
	float NextDashTime = 0.0f;
	float NextGrabTime = 0.0f;
	float ChargeStartTime = 0.0f;
	float NextChargedAttackTime = 0.0f;
	float CounterStanceEndTime = 0.0f;
	float CounterStanceDamage = 0.0f;
	float NextBasicDamageMultiplier = 1.0f;
	float MarkedDamageBonus = 0.0f;
	float MarkExpiryTime = 0.0f;
	int32 MarkedByTeamId = INDEX_NONE;
	bool bCounterStanceArmed = false;
	bool bApplyingAbilityDamage = false;
	FString CombatFeedbackMessage;
	float CombatFeedbackExpiry = 0.0f;
	FString MatchResultLabel;
	float MatchDamageDealt = 0.0f;
	float MatchDamageReceived = 0.0f;
	int32 MatchKnockouts = 0;
	int32 MatchDeaths = 0;
	int32 MaximumComboHits = 0;
	int32 CurrentComboHits = 0;
	float MatchDurationSeconds = 0.0f;
	int32 MatchExperienceReward = 0;
	int32 MatchGoldReward = 0;
	int32 MatchEventReward = 0;
	FTimerHandle PendingBasicAttackInputTimer;
	FTimerHandle GrabResolveTimer;
	uint8 NextComboStage = 0;
	bool bIsChargingSpecial = false;
	bool bHasMatchResult = false;
	bool bIsGrabStarting = false;
	bool bLocalLmbHeld = false;
	bool bLocalRmbHeld = false;
	bool bPendingBasicAttackInput = false;
	bool bLocalGrabGestureActive = false;
	float ActionAnimationRemaining = 0.0f;
	bool bIsMoving = false;
	bool bHasLocomotionAnimationStarted = false;
};
