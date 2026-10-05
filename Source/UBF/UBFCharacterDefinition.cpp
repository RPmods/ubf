#include "UBFCharacterDefinition.h"

namespace
{
	FString BuildAbilityDescription(const TCHAR* Slot, EUBFAbilityEffect Effect, float Damage,
		float Range, float Radius, float GaugeCost, float Cooldown, float MovementDistance,
		float EffectDuration, float CounterDamage)
	{
		FString Action;
		FString TacticalUse;
		switch (Effect)
		{
		case EUBFAbilityEffect::ForwardLine:
			Action = FString::Printf(TEXT("Lanza un golpe estrecho en línea recta hacia donde mira el combatiente, con hasta %.0f unidades de alcance. Su trayectoria cubre menos anchura que un barrido, así que conviene alinear al rival antes de confirmar la habilidad."), Range);
			TacticalUse = TEXT("Sirve para castigar una recuperación visible, iniciar un combo desde distancia media o rematar a un objetivo que intenta escapar por un pasillo despejado.");
			break;
		case EUBFAbilityEffect::ForwardArea:
			Action = FString::Printf(TEXT("Ejecuta un barrido frontal que alcanza a los enemigos dentro de un radio aproximado de %.0f unidades alrededor del impacto. Puede conectar con más de un rival si se agrupan delante; no cubre por igual la espalda."), Radius);
			TacticalUse = TEXT("Resérvala para disputar espacio, interrumpir una entrada o aprovechar a varios rivales juntos. Acércate lo suficiente para no gastar gauge fuera de la zona efectiva.");
			break;
		case EUBFAbilityEffect::SelfPulse:
			Action = FString::Printf(TEXT("Libera un pulso alrededor del propio combatiente, con radio de %.0f unidades. No requiere apuntar una línea concreta y puede alcanzar rivales cercanos a su alrededor, pero no sirve para golpear a distancia."), Radius);
			TacticalUse = TEXT("Guárdala para cuando un rival entre a corta distancia o te rodeen. El área se centra en ti: no gastes gauge esperando alcanzar a un blanco alejado.");
			break;
		case EUBFAbilityEffect::PullWave:
			Action = FString::Printf(TEXT("Proyecta una onda frontal de hasta %.0f unidades y daña a los rivales dentro de un radio de %.0f. Al conectar, los atrae hacia tu posición y los eleva brevemente; el tirón depende de que el blanco esté delante y dentro de la zona."), Range, Radius);
			TacticalUse = TEXT("Úsala para interrumpir una retirada, traer a un rival al alcance de tus básicos o preparar la continuación de un combo. Apunta al frente y calcula la llegada: los objetivos fuera de la zona no quedan atrapados.");
			break;
		case EUBFAbilityEffect::PushWave:
			Action = FString::Printf(TEXT("Descarga una onda frontal de hasta %.0f unidades que golpea dentro de un radio de %.0f y desplaza a los enemigos hacia fuera. La separación que crea puede cortar una presión continua."), Range, Radius);
			TacticalUse = TEXT("Sirve para recuperar espacio, proteger a un aliado o apartar a un rival de un objetivo. Si buscas continuar el combo de inmediato, mide el empuje porque también aleja al objetivo.");
			break;
		case EUBFAbilityEffect::DashStrike:
			Action = FString::Printf(TEXT("Avanza aproximadamente %.0f unidades hacia delante y golpea a los enemigos que encuentra en la trayectoria, hasta un alcance de %.0f y un radio de %.0f."), MovementDistance, Range, Radius);
			TacticalUse = TEXT("Es una herramienta de entrada y reposicionamiento: úsala para cerrar distancia o atravesar una apertura confirmada. Si el rival esquiva a tiempo, el desplazamiento puede dejarte expuesto al terminar.");
			break;
		case EUBFAbilityEffect::CounterStance:
			Action = FString::Printf(TEXT("Adopta una guardia de contraataque durante %.1f s. Si un enemigo te impacta desde delante en esa ventana, reduces ese daño al 25%% y respondes con un contraataque de %.0f de daño; si nadie te golpea, la ventana se pierde."), EffectDuration, CounterDamage);
			TacticalUse = TEXT("Actívala al leer el inicio de un ataque, no después de recibirlo. No cubre ataques que llegan por detrás y el oponente puede esperar a que la postura termine.");
			break;
		default:
			Action = TEXT("Resuelve un impacto de combate contra los rivales dentro de su zona válida.");
			TacticalUse = TEXT("Busca una apertura confirmada para obtener valor y evita lanzarla cuando el objetivo ya salió de alcance.");
			break;
		}

		FString SlotUse;
		if (FCString::Stricmp(Slot, TEXT("Primary")) == 0)
		{
			SlotUse = TEXT("Q · HABILIDAD PRIMARIA. Es la opción de menor coste: encadénala después de un básico confirmado o úsala para abrir una defensa sin vaciar tu recurso.");
		}
		else if (FCString::Stricmp(Slot, TEXT("Secondary")) == 0)
		{
			SlotUse = TEXT("E · HABILIDAD SECUNDARIA. Tiene más impacto y consume una parte mayor del gauge; guárdala para controlar la distancia o extender una oportunidad clara.");
		}
		else
		{
			SlotUse = TEXT("F · DEFINITIVA. Requiere el gauge completo; espera a confirmar el alcance o a que el rival quede comprometido antes de gastarlo.");
		}
		return FString::Printf(TEXT("%s\n%s\n%s\nDaño base %.0f · alcance %.0f · gauge %.0f%% · recarga %.1f s."),
			*SlotUse, *Action, *TacticalUse, Damage, Range, GaugeCost, Cooldown);
	}

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
		Ability->DetailedDescription = FText::FromString(BuildAbilityDescription(Slot, Effect,
			Damage, Range, Radius, Cost, Cooldown, Ability->MovementDistance,
			Ability->EffectDuration, Ability->CounterDamage));
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

FName UUBFCharacterCatalogSubsystem::ChooseCharacterId(const TSet<FName>& ExcludedIds,
	int32 PreferredStartIndex) const
{
	if (CharacterIds.IsEmpty())
	{
		return NAME_None;
	}

	const int32 StartIndex = FMath::Abs(PreferredStartIndex) % CharacterIds.Num();
	for (int32 Offset = 0; Offset < CharacterIds.Num(); ++Offset)
	{
		const FName Candidate = CharacterIds[(StartIndex + Offset) % CharacterIds.Num()];
		if (!ExcludedIds.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return NAME_None;
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
		TEXT("Presión caótica y cambios de ritmo."), TEXT("Impulso roto"), TEXT("Cada impacto que continúa una cadena de básicos concede 4 puntos extra de gauge. La bonificación solo cuenta después del primer golpe confirmado: mantén el combo conectado para desbloquear antes E y la definitiva, en vez de atacar al aire."),
		TEXT("Corte Carmesí"), TEXT("Ruptura"), TEXT("Ruido Blanco"),
		EUBFPassiveMechanic::ComboGaugeBonus, 4.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 535.0f, 100.0f, 210.0f);
	AddFighter(TEXT("Akira"), TEXT("Akira"), TEXT("Usa líneas precisas y pasos largos para dictar distancia."),
		TEXT("Precisión de alcance medio y entradas directas."), TEXT("Cristal de retorno"), TEXT("Cuando Q, E o la definitiva aciertan, Akira obtiene 4 puntos adicionales de gauge además de la carga normal del impacto. La pasiva premia medir la distancia y confirmar cada lanzamiento; fallar una habilidad no activa el beneficio."),
		TEXT("Lanza Boreal"), TEXT("Paso Glacial"), TEXT("Vórtice Cero"),
		EUBFPassiveMechanic::AbilityHitGaugeBonus, 4.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardLine, 520.0f, 100.0f, 390.0f);
	AddFighter(TEXT("Eilene"), TEXT("Eilene"), TEXT("Alterna ataques y reposicionamiento disciplinado."),
		TEXT("Movilidad lateral y ataques de precisión."), TEXT("Paso medido"), TEXT("Después de un dash, el siguiente básico que conecte inflige 25% más de daño y consume la bonificación al acertar. Usa el desplazamiento para salir de una línea de ataque y volver con un golpe reforzado; no gastes ese estado con un ataque que no pueda alcanzar al rival."),
		TEXT("Desvío"), TEXT("Línea de Intercepción"), TEXT("Cerco Final"),
		EUBFPassiveMechanic::NextAttackAfterDash, 0.25f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardArea, EUBFAbilityEffect::SelfPulse, 575.0f, 95.0f, 280.0f);

	// Five public-name adaptations. Visual/personality details remain provisional where public evidence is insufficient.
	AddFighter(TEXT("Nozomidol"), TEXT("Nozomidol"), TEXT("Marca el ritmo con ondas y pulsos sonoros."),
		TEXT("Ritmo, alcance medio y empuje."), TEXT("Compás"), TEXT("Cada habilidad que impacta devuelve 3 puntos adicionales de gauge. Mantén la precisión de tus ondas para disponer de Q con más frecuencia y acercarte antes a la definitiva; el bonus depende del impacto real, no de iniciar la animación."),
		TEXT("Nota de Marea"), TEXT("Resonancia"), TEXT("Encore Abisal"),
		EUBFPassiveMechanic::AbilityHitGaugeBonus, 3.0f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PullWave, EUBFAbilityEffect::SelfPulse, 525.0f, 100.0f, 360.0f);
	AddFighter(TEXT("EmikoAi"), TEXT("EmikoAi"), TEXT("Esquiva, lee el ataque y responde con precisión."),
		TEXT("Contraataques direccionales y ventanas de timing."), TEXT("Parallax"), TEXT("El contraataque de E inflige 45% más de daño cuando una ofensiva frontal activa la postura. Úsala al leer el inicio de un ataque y responde dentro de la ventana; no requiere una esquiva especial, pero sí orientación y buen timing."),
		TEXT("Eco Reverso"), TEXT("Lectura"), TEXT("Foco Espejo"),
		EUBFPassiveMechanic::CounterAmplifier, 1.45f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::ForwardLine, 525.0f, 100.0f, 250.0f);
	AddFighter(TEXT("Maybkchan"), TEXT("Maybkchan"), TEXT("Prepara rutas y cambia el punto de entrada."),
		TEXT("Anclas, reposicionamiento y control espacial."), TEXT("Ruta guardada"), TEXT("Los dashes de Maybkchan recorren 30% más distancia. Esto amplía su entrada y también el margen para escapar de una zona de peligro; controla el punto final para no atravesar al rival y quedar expuesta detrás de él."),
		TEXT("Paso de Ruta"), TEXT("Anillo de Entrada"), TEXT("Cruce Final"),
		EUBFPassiveMechanic::DashDistanceBonus, 0.30f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 535.0f, 100.0f, 330.0f);
	AddFighter(TEXT("HelenCreth"), TEXT("Helen Creth"), TEXT("Marca rivales y activa sellos en el momento oportuno."),
		TEXT("Marcas visibles y detonaciones preparadas."), TEXT("Cláusula"), TEXT("Un impacto de habilidad marca al rival durante 6 segundos; mientras dure la marca, los ataques posteriores de Helen le infligen 18% más de daño. Aplica la marca primero y aprovecha esa ventana para enlazar básicos o una segunda habilidad; si fallas, no se marca a nadie."),
		TEXT("Trazo Vinculante"), TEXT("Cláusula Activa"), TEXT("Firma Final"),
		EUBFPassiveMechanic::MarkAbilityTargets, 0.18f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::PushWave, EUBFAbilityEffect::SelfPulse, 500.0f, 100.0f, 400.0f);
	AddFighter(TEXT("Gizz"), TEXT("Gizz"), TEXT("Cambia de ángulo y domina rutas de media distancia."),
		TEXT("Trayectorias curvas y movilidad orbital."), TEXT("Órbita"), TEXT("Cuando una habilidad conecta, el siguiente básico de Gizz recibe 30% más de daño y consume la carga al impactar. Alterna habilidad y básico para convertir el control de trayectoria en presión; evita lanzar básicos antes de confirmar la habilidad."),
		TEXT("Vector Curvo"), TEXT("Cambio de Órbita"), TEXT("Caída Invertida"),
		EUBFPassiveMechanic::NextBasicAfterSkill, 0.30f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardArea, 540.0f, 100.0f, 420.0f);

	// Five original UBF fighters with different movement/effect signatures.
	AddFighter(TEXT("IriaVoss"), TEXT("Iria Voss"), TEXT("Castiga ataques previsibles con respuestas arriesgadas."),
		TEXT("Timing, guardia frontal y estocadas."), TEXT("Filo expuesto"), TEXT("El contraataque frontal de E inflige 85% más de daño cuando un golpe activa la postura. Colócala mirando al atacante justo antes del impacto para reducir el daño recibido y responder con una estocada reforzada; la espalda y los ataques que llegan tarde quedan fuera de la ventana."),
		TEXT("Estocada de Riesgo"), TEXT("Guardia de Retorno"), TEXT("Filo Final"),
		EUBFPassiveMechanic::CounterAmplifier, 1.85f,
		EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::ForwardLine, 515.0f, 100.0f, 230.0f);
	AddFighter(TEXT("KaelSerein"), TEXT("Kael Serein"), TEXT("Enlaza ataques aéreos con desplazamientos cortos."),
		TEXT("Cambios de nivel y movilidad aérea."), TEXT("Corriente alterna"), TEXT("El especial cargado de Kael inflige 25% más de daño si impacta mientras está en el aire. Salta o combina una elevación antes del cargado para activar la ventaja; en el suelo conserva el daño normal y no recibe el extra."),
		TEXT("Ascenso"), TEXT("Corte de Viento"), TEXT("Lluvia Inversa"),
		EUBFPassiveMechanic::AirborneChargedBonus, 0.25f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::ForwardLine, EUBFAbilityEffect::SelfPulse, 590.0f, 95.0f, 300.0f);
	AddFighter(TEXT("NaraQuill"), TEXT("Nara Quill"), TEXT("Prepara zonas de presión con dispositivos visibles."),
		TEXT("Perímetros, preparación y castigo de rutas."), TEXT("Coordenada"), TEXT("Todas las habilidades de Nara obtienen 35% más de radio efectivo. La expansión facilita alcanzar rivales que se agrupan y controlar una entrada, pero conserva el requisito de orientación y alcance: no convierte un golpe frontal en daño omnidireccional."),
		TEXT("Perímetro"), TEXT("Ancla"), TEXT("Zona Cero"),
		EUBFPassiveMechanic::ExpandedAbilityArea, 0.35f,
		EUBFAbilityEffect::ForwardArea, EUBFAbilityEffect::SelfPulse, EUBFAbilityEffect::ForwardLine, 490.0f, 105.0f, 460.0f);
	AddFighter(TEXT("RivenSol"), TEXT("Riven Sol"), TEXT("Acumula energía durante combos y la descarga en ráfagas."),
		TEXT("Riesgo, sobrecarga y ventanas breves de potencia."), TEXT("Banco solar"), TEXT("Al bajar a 35% de salud o menos, Riven inflige 20% más de daño con sus ataques y habilidades. La bonificación recompensa sobrevivir en una situación crítica, pero no reduce el daño recibido; busca una apertura decisiva en vez de cambiar golpes sin ventaja."),
		TEXT("Carga Solar"), TEXT("Reingreso"), TEXT("Colapso Lumínico"),
		EUBFPassiveMechanic::LowHealthDamageBonus, 0.20f,
		EUBFAbilityEffect::SelfPulse, EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::PullWave, 530.0f, 105.0f, 220.0f);
	AddFighter(TEXT("TaviOrun"), TEXT("Tavi Orun"), TEXT("Usa ecos visuales para cambiar la dirección del combate."),
		TEXT("Fintas, cambios de lado y trayectorias anunciadas."), TEXT("Imagen residual"), TEXT("La ventana de contraataque de Tavi dura 0.35 segundos más que la normal. Activa E al leer una ofensiva frontal para ampliar el margen de respuesta; la postura sigue siendo direccional y puede ser esquivada o esperada por el adversario."),
		TEXT("Eco Cortante"), TEXT("Finta"), TEXT("Imagen Final"),
		EUBFPassiveMechanic::ExtendedCounterWindow, 0.35f,
		EUBFAbilityEffect::DashStrike, EUBFAbilityEffect::CounterStance, EUBFAbilityEffect::SelfPulse, 520.0f, 100.0f, 340.0f);
}
