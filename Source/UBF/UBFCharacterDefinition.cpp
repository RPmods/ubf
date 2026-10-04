#include "UBFCharacterDefinition.h"

namespace
{
	UUBFAbilityDefinition* MakeAbility(UUBFCharacterDefinition* Character, const TCHAR* Slot,
		const TCHAR* Name, float Cost, float Cooldown, float Damage, float Range, float Radius,
	float Offset, EUBFAbilityEffect Effect, float Knockback = 260.0f)
	{
		const FString CharacterName = Character->CharacterID.ToString();
		const FString AbilityName = FString::Printf(TEXT("%s_%s"), *CharacterName, Slot);
		UUBFAbilityDefinition* Ability = NewObject<UUBFAbilityDefinition>(Character,
			FName(*AbilityName));
		Ability->AbilityID = FName(*AbilityName);
		Ability->DisplayName = FText::FromString(Name);
		Ability->GaugeCost = Cost;
		Ability->Cooldown = Cooldown;
		Ability->RecoveryTime = Cooldown;
		Ability->Damage = Damage;
		Ability->Range = Range;
		Ability->Radius = Radius;
		Ability->ImpactOffset = Offset;
		Ability->Knockback = Knockback;
		Ability->CounterDamage = Damage * 0.75f;
		Ability->Effect = Effect;
		Ability->Targeting = Effect == EUBFAbilityEffect::SelfPulse
			? EUBFAbilityTargeting::Self : EUBFAbilityTargeting::ForwardArea;
		return Ability;
	}
}

void UUBFCharacterCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildDefaultCatalog();
}

UUBFCharacterDefinition* UUBFCharacterCatalogSubsystem::FindCharacter(FName CharacterId) const
{
	if (const TObjectPtr<UUBFCharacterDefinition>* Found = Definitions.Find(CharacterId))
	{
		return Found->Get();
	}
	return nullptr;
}

void UUBFCharacterCatalogSubsystem::BuildDefaultCatalog()
{
	Definitions.Reset();
	CharacterIds.Reset();

	auto AddFighter = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Description,
		const TCHAR* Style, const TCHAR* PassiveName, const TCHAR* PassiveDescription,
		const TCHAR* PrimaryName, const TCHAR* SecondaryName, const TCHAR* UltimateName,
		EUBFPassiveMechanic PassiveMechanic, float PassiveMagnitude,
		EUBFAbilityEffect QEffect, EUBFAbilityEffect EEffect, EUBFAbilityEffect FEffect,
		float Speed = 520.0f, float Health = 100.0f, float PreferredDistance = 320.0f)
	{
		const FName CharacterId(Id);
		UUBFCharacterDefinition* Character = NewObject<UUBFCharacterDefinition>(this, CharacterId);
		Character->CharacterID = CharacterId;
		Character->DisplayName = FText::FromString(Name);
		Character->ShortDescription = FText::FromString(Description);
		Character->CombatStyle = FText::FromString(Style);
		Character->PassiveName = FText::FromString(PassiveName);
		Character->PassiveDescription = FText::FromString(PassiveDescription);
		Character->PassiveMechanic = PassiveMechanic;
		Character->PassiveMagnitude = FMath::Max(0.0f, PassiveMagnitude);
		Character->BaseStats.MaximumHealth = Health;
		Character->BaseStats.MaxWalkSpeed = Speed;
		Character->BaseStats.DashSpeed = Speed * 1.82f;
		Character->BaseStats.BasicAttackDamage = 12.0f;
		Character->BaseStats.BasicAttackRange = 180.0f;
		Character->BaseStats.BasicAttackRadius = 85.0f;
		Character->GaugeSettings.Maximum = 100.0f;
		Character->PrimarySkill = MakeAbility(Character, TEXT("Primary"), PrimaryName, 25.0f, 0.72f,
			22.0f, 480.0f, 160.0f, 320.0f, QEffect);
		Character->SecondarySkill = MakeAbility(Character, TEXT("Secondary"), SecondaryName, 50.0f, 1.05f,
			34.0f, 700.0f, 235.0f, 465.0f, EEffect, 380.0f);
		Character->Ultimate = MakeAbility(Character, TEXT("Ultimate"), UltimateName, 100.0f, 2.2f,
			48.0f, 980.0f, 340.0f, 670.0f, FEffect, 560.0f);
		Character->AIProfile = NewObject<UUBFCharacterAIProfile>(Character,
			FName(*FString::Printf(TEXT("%s_AI"), Id)));
		Character->AIProfile->PreferredDistance = PreferredDistance;
		Character->AIProfile->Aggression = PreferredDistance < 220.0f ? 0.82f : 0.56f;
		Character->AIProfile->GoldenBearerPriority = 0.86f;
		Character->AIProfile->MasterGolemPriority = 0.72f;
		Character->AIProfile->AllyProtection = 0.58f;
		Character->AIProfile->ReactionDelay = 0.22f;
		Definitions.Add(CharacterId, Character);
		CharacterIds.Add(CharacterId);
	};

	// Existing fighters: preserve their identities while giving each a distinct skill geometry.
	AddFighter(TEXT("Wizz"), TEXT("Wizz"), TEXT("Presiona y enlaza golpes para abrir defensas."),
		TEXT("Presión caótica y cambios de ritmo."), TEXT("Impulso roto"), TEXT("Los combos confirmados cargan gauge adicional."),
		TEXT("Corte Carmesí"), TEXT("Ruptura"), TEXT("Ruido Blanco"),
		EUBFPassiveMechanic::ComboGaugeBonus, 4.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 535.0f, 100.0f, 210.0f);
	AddFighter(TEXT("Akira"), TEXT("Akira"), TEXT("Usa líneas precisas y pasos largos para dictar distancia."),
		TEXT("Precisión de alcance medio y entradas directas."), TEXT("Cristal de retorno"), TEXT("Las habilidades acertadas restauran un poco más de gauge."),
		TEXT("Lanza Boreal"), TEXT("Paso Glacial"), TEXT("Vórtice Cero"),
		EUBFPassiveMechanic::AbilityHitGaugeBonus, 4.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardLine, 520.0f, 100.0f, 390.0f);
	AddFighter(TEXT("Eilene"), TEXT("Eilene"), TEXT("Alterna ataques y reposicionamiento disciplinado."),
		TEXT("Movilidad lateral y ataques de precisión."), TEXT("Paso medido"), TEXT("Cada dash prepara un ataque básico más potente."),
		TEXT("Desvío"), TEXT("Línea de Intercepción"), TEXT("Cerco Final"),
		EUBFPassiveMechanic::NextAttackAfterDash, 0.25f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardArea, EUBFAbilityEffect::SelfPulse, 575.0f, 95.0f, 280.0f);

	// Five public-name adaptations. Visual/personality details remain provisional where public evidence is insufficient.
	AddFighter(TEXT("Nozomidol"), TEXT("Nozomidol"), TEXT("Marca el ritmo con ondas y pulsos sonoros."),
		TEXT("Ritmo, alcance medio y empuje."), TEXT("Compás"), TEXT("Cada impacto de habilidad devuelve gauge adicional."),
		TEXT("Nota de Marea"), TEXT("Resonancia"), TEXT("Encore Abisal"),
		EUBFPassiveMechanic::AbilityHitGaugeBonus, 3.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PullWave, EUBFAbilityEffect::SelfPulse, 525.0f, 100.0f, 360.0f);
	AddFighter(TEXT("EmikoAi"), TEXT("EmikoAi"), TEXT("Esquiva, lee el ataque y responde con precisión."),
		TEXT("Contraataques direccionales y ventanas de timing."), TEXT("Parallax"), TEXT("Una esquiva precisa prepara la siguiente habilidad primaria."),
		TEXT("Eco Reverso"), TEXT("Lectura"), TEXT("Foco Espejo"),
		EUBFPassiveMechanic::CounterAmplifier, 1.45f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::ForwardLine, 525.0f, 100.0f, 250.0f);
	AddFighter(TEXT("Maybkchan"), TEXT("Maybkchan"), TEXT("Prepara rutas y cambia el punto de entrada."),
		TEXT("Anclas, reposicionamiento y control espacial."), TEXT("Ruta guardada"), TEXT("Sus dashes recorren más distancia para abrir nuevas rutas."),
		TEXT("Paso de Ruta"), TEXT("Anillo de Entrada"), TEXT("Cruce Final"),
		EUBFPassiveMechanic::DashDistanceBonus, 0.30f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 535.0f, 100.0f, 330.0f);
	AddFighter(TEXT("HelenCreth"), TEXT("Helen Creth"), TEXT("Marca rivales y activa sellos en el momento oportuno."),
		TEXT("Marcas visibles y detonaciones preparadas."), TEXT("Cláusula"), TEXT("Las habilidades marcan rivales; los golpes posteriores infligen daño extra."),
		TEXT("Trazo Vinculante"), TEXT("Cláusula Activa"), TEXT("Firma Final"),
		EUBFPassiveMechanic::MarkAbilityTargets, 0.18f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 500.0f, 100.0f, 400.0f);
	AddFighter(TEXT("Gizz"), TEXT("Gizz"), TEXT("Cambia de ángulo y domina rutas de media distancia."),
		TEXT("Trayectorias curvas y movilidad orbital."), TEXT("Órbita"), TEXT("Los impactos de habilidad cargan un fragmento para el siguiente básico."),
		TEXT("Vector Curvo"), TEXT("Cambio de Órbita"), TEXT("Caída Invertida"),
		EUBFPassiveMechanic::NextBasicAfterSkill, 0.30f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardArea, 540.0f, 100.0f, 420.0f);

	// Five original UBF fighters with different movement/effect signatures.
	AddFighter(TEXT("IriaVoss"), TEXT("Iria Voss"), TEXT("Castiga ataques previsibles con respuestas arriesgadas."),
		TEXT("Timing, guardia frontal y estocadas."), TEXT("Filo expuesto"), TEXT("Una esquiva precisa potencia el siguiente cargado."),
		TEXT("Estocada de Riesgo"), TEXT("Guardia de Retorno"), TEXT("Filo Final"),
		EUBFPassiveMechanic::CounterAmplifier, 1.85f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::ForwardLine, 515.0f, 100.0f, 230.0f);
	AddFighter(TEXT("KaelSerein"), TEXT("Kael Serein"), TEXT("Enlaza ataques aéreos con desplazamientos cortos."),
		TEXT("Cambios de nivel y movilidad aérea."), TEXT("Corriente alterna"), TEXT("El ataque cargado golpea más fuerte mientras Kael está en el aire."),
		TEXT("Ascenso"), TEXT("Corte de Viento"), TEXT("Lluvia Inversa"),
		EUBFPassiveMechanic::AirborneChargedBonus, 0.25f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::SelfPulse, 590.0f, 95.0f, 300.0f);
	AddFighter(TEXT("NaraQuill"), TEXT("Nara Quill"), TEXT("Prepara zonas de presión con dispositivos visibles."),
		TEXT("Perímetros, preparación y castigo de rutas."), TEXT("Coordenada"), TEXT("Sus habilidades ganan radio cerca de un ancla activa."),
		TEXT("Perímetro"), TEXT("Ancla"), TEXT("Zona Cero"),
		EUBFPassiveMechanic::ExpandedAbilityArea, 0.35f,
		EUBFAbilityEffect::ForwardArea, EUBFAbilityEffect::SelfPulse, EUBFAbilityEffect::ForwardLine, 490.0f, 105.0f, 460.0f);
	AddFighter(TEXT("RivenSol"), TEXT("Riven Sol"), TEXT("Acumula energía durante combos y la descarga en ráfagas."),
		TEXT("Riesgo, sobrecarga y ventanas breves de potencia."), TEXT("Banco solar"), TEXT("Con poca vida, Riven aumenta el daño que inflige."),
		TEXT("Carga Solar"), TEXT("Reingreso"), TEXT("Colapso Lumínico"),
		EUBFPassiveMechanic::LowHealthDamageBonus, 0.20f,
		EUBFAbilityEffect::SelfPulse, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::PullWave, 530.0f, 105.0f, 220.0f);
	AddFighter(TEXT("TaviOrun"), TEXT("Tavi Orun"), TEXT("Usa ecos visuales para cambiar la dirección del combate."),
		TEXT("Fintas, cambios de lado y trayectorias anunciadas."), TEXT("Imagen residual"), TEXT("La ventana de su contraataque dura más tiempo."),
		TEXT("Eco Cortante"), TEXT("Finta"), TEXT("Imagen Final"),
		EUBFPassiveMechanic::ExtendedCounterWindow, 0.35f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::SelfPulse, 520.0f, 100.0f, 340.0f);
}
