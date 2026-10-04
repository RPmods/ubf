#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UBFCharacterDefinition.generated.h"

class UInputAction;

UENUM(BlueprintType)
enum class EUBFAbilityTargeting : uint8
{
	ForwardArea,
	SingleTarget,
	Self,
	GroundPoint
};

UENUM(BlueprintType)
enum class EUBFAbilityMovement : uint8
{
	None,
	DashForward,
	RepositionToTarget,
	ReturnToAnchor
};

UENUM(BlueprintType)
enum class EUBFAbilityEffect : uint8
{
	ForwardArea,
	ForwardLine,
	SelfPulse,
	PullWave,
	PushWave,
	DashStrike,
	CounterStance
};

UENUM(BlueprintType)
enum class EUBFPassiveMechanic : uint8
{
	None,
	ComboGaugeBonus,
	AbilityHitGaugeBonus,
	NextAttackAfterDash,
	CounterAmplifier,
	DashDistanceBonus,
	MarkAbilityTargets,
	NextBasicAfterSkill,
	AirborneChargedBonus,
	ExpandedAbilityArea,
	LowHealthDamageBonus,
	ExtendedCounterWindow
};

USTRUCT(BlueprintType)
struct FUBFCharacterCombatStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin="1.0"))
	float MaximumHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin="0.0"))
	float MaxWalkSpeed = 520.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin="0.0"))
	float JumpZVelocity = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float BasicAttackDamage = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float BasicAttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float BasicAttackRadius = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float BasicAttackCooldown = 0.48f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Attack", meta=(ClampMin="0.1"))
	float ChargedMaximumChargeTime = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Attack", meta=(ClampMin="0.0"))
	float ChargedMaximumRange = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Attack", meta=(ClampMin="0.0"))
	float ChargedBaseDamage = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Attack", meta=(ClampMin="0.0"))
	float ChargedFullChargeBonusDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Attack", meta=(ClampMin="0.0"))
	float ChargedHitRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
	float DashSpeed = 950.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
	float DashCooldown = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0.0"))
	float GrabRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0.0"))
	float GrabDamage = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0.0"))
	float GrabStartupDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0.1"))
	float GrabRecoveryDuration = 1.0f;
};

USTRUCT(BlueprintType)
struct FUBFSkillGaugeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="1.0"))
	float Maximum = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0.0"))
	float BasicHitReward = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0.0"))
	float ComboHitReward = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0.0"))
	float ChargedHitReward = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0.0"))
	float ReceiveHitReward = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge", meta=(ClampMin="0.0"))
	float ReceiveHeavyHitReward = 3.0f;
};

UCLASS(BlueprintType)
class UBF_API UUBFAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability")
	FName AbilityID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0.0"))
	float Cooldown = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0.0"))
	float GaugeCost = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0.0"))
	float CastTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0.0"))
	float RecoveryTime = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float Damage = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float Range = 480.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float Radius = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat" )
	float ImpactOffset = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	float Knockback = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	float KnockdownDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	EUBFAbilityTargeting Targeting = EUBFAbilityTargeting::ForwardArea;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	EUBFAbilityMovement Movement = EUBFAbilityMovement::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	EUBFAbilityEffect Effect = EUBFAbilityEffect::ForwardArea;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float EffectDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float MovementDistance = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float CounterDamage = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> CameraProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> VFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> SFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> Animation;
};

UCLASS(BlueprintType)
class UBF_API UUBFCharacterAIProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Aggression = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0"))
	float PreferredDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RetreatThreshold = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AttackFrequency = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SkillPriority = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float UltimatePriority = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DodgeReaction = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ComboPreference = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float TargetPriority = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AllyProtection = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Golem Mode", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GoldenBearerPriority = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Golem Mode", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MasterGolemPriority = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Golem Mode", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EscapeBehavior = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RecoveryBehavior = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0"))
	float ReactionDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior")
	FName Difficulty = TEXT("NORMAL");
};

UCLASS(BlueprintType)
class UBF_API UUBFCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character")
	FName CharacterID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character", meta=(MultiLine="true"))
	FText ShortDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character", meta=(MultiLine="true"))
	FText CombatStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character")
	FText PassiveName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character", meta=(MultiLine="true"))
	FText PassiveDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Passive")
	EUBFPassiveMechanic PassiveMechanic = EUBFPassiveMechanic::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Passive", meta=(ClampMin="0.0"))
	float PassiveMagnitude = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	FUBFCharacterCombatStats BaseStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge")
	FUBFSkillGaugeSettings GaugeSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UUBFAbilityDefinition> PrimarySkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UUBFAbilityDefinition> SecondarySkill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UUBFAbilityDefinition> Ultimate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	TObjectPtr<UUBFCharacterAIProfile> AIProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> AnimationSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> VFXSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> SFXSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UDataAsset> CameraProfile;
};

/** Runtime catalog for the playable prototype roster. Production assets can replace these defaults later. */
UCLASS()
class UBF_API UUBFCharacterCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	const TArray<FName>& GetCharacterIds() const { return CharacterIds; }
	UUBFCharacterDefinition* FindCharacter(FName CharacterId) const;

private:
	void BuildDefaultCatalog();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUBFCharacterDefinition>> Definitions;

	TArray<FName> CharacterIds;
};
