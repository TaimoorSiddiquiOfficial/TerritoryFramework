#include "Story/TerritoryStoryOutcomeAnalyzer.h"

#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryStealthProfile.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"

namespace
{
	FString DisplayName(const UTerritoryDefinition* Definition)
	{
		if (!Definition) return TEXT("Invalid Territory Definition");
		return Definition->DisplayName.IsEmpty()
			? Definition->GetName() : Definition->DisplayName.ToString();
	}

	template <typename TEnum>
	FString EnumDisplayName(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)).ToString();
		}
		return FString::FromInt(static_cast<int32>(Value));
	}

	FString TagOrNone(const FGameplayTag& Tag)
	{
		return Tag.IsValid() ? Tag.ToString() : TEXT("no faction");
	}

	FString JoinNonEmpty(const TArray<FString>& Values, const TCHAR* Separator)
	{
		TArray<FString> Filtered;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty()) Filtered.Add(Value);
		}
		return FString::Join(Filtered, Separator);
	}

	FString DescribeCondition(UNarrativeCondition* Condition)
	{
		if (!Condition) return TEXT("invalid condition");
		FString Text = Condition->GetGraphDisplayText();
		if (Text.TrimStartAndEnd().IsEmpty())
		{
			Text = Condition->GetClass()->GetDisplayNameText().ToString();
		}
		if (Condition->bNot && !Text.StartsWith(TEXT("NOT"), ESearchCase::IgnoreCase))
		{
			Text = FString::Printf(TEXT("NOT (%s)"), *Text);
		}
		return Text;
	}

	template <typename TConditionPointer>
	FString DescribeConditions(const TArray<TConditionPointer>& Conditions)
	{
		TArray<FString> Descriptions;
		for (const TConditionPointer& ConditionPointer : Conditions)
		{
			UNarrativeCondition* Condition = ConditionPointer;
			if (Condition) Descriptions.Add(DescribeCondition(Condition));
		}
		return FString::Join(Descriptions, TEXT(" AND "));
	}

	FString DescribeEvent(UNarrativeEvent* Event)
	{
		if (!Event) return TEXT("invalid event");
		FString Text = Event->GetGraphDisplayText();
		if (Text.TrimStartAndEnd().IsEmpty())
		{
			Text = Event->GetClass()->GetDisplayNameText().ToString();
		}
		const FString Conditions = DescribeConditions(Event->Conditions);
		if (!Conditions.IsEmpty())
		{
			Text += FString::Printf(TEXT(" (runs only if %s)"), *Conditions);
		}
		if (Event->GetClass()->ClassGeneratedBy)
		{
			Text += TEXT(" [custom Blueprint]");
		}
		return Text;
	}

	FString DescribeEvents(const TArray<TObjectPtr<UNarrativeEvent>>& Events)
	{
		TArray<FString> Descriptions;
		for (UNarrativeEvent* Event : Events)
		{
			if (Event) Descriptions.Add(DescribeEvent(Event));
		}
		return FString::Join(Descriptions, TEXT("; "));
	}

	template <typename TNarrativePointer>
	bool ContainsCustomBlueprintObject(const TArray<TNarrativePointer>& Objects)
	{
		for (const TNarrativePointer& ObjectPointer : Objects)
		{
			const UObject* Object = ObjectPointer;
			if (Object && Object->GetClass()->ClassGeneratedBy) return true;
		}
		return false;
	}

	FString DescribeCapabilities(const FGameplayTagContainer& Capabilities)
	{
		TArray<FString> Tags;
		for (const FGameplayTag& Tag : Capabilities)
		{
			Tags.Add(Tag.ToString());
		}
		Tags.Sort();
		return FString::Join(Tags, TEXT(", "));
	}

	void AddScenario(FTerritoryStoryOutcomeReport& Report, const FString& Category,
		const FString& Title, ETerritoryStoryOutcomeCertainty Certainty,
		const FString& When, const FString& OnlyIf, const FString& Then,
		const FString& IfNot, const FString& AlsoAffects, const FString& Source)
	{
		// A setting can participate in more than one high-level explanation. Do not
		// render the exact same outcome twice when those paths collapse to one row.
		// Source is deliberately excluded from the identity and merged below.
		FTerritoryStoryOutcomeScenario* Existing = Report.Scenarios.FindByPredicate(
			[&](const FTerritoryStoryOutcomeScenario& Scenario)
			{
				return Scenario.Category == Category
					&& Scenario.Title == Title
					&& Scenario.Certainty == Certainty
					&& Scenario.When == When
					&& Scenario.OnlyIf == OnlyIf
					&& Scenario.Then == Then
					&& Scenario.IfNot == IfNot
					&& Scenario.AlsoAffects == AlsoAffects;
			});
		if (Existing)
		{
			if (!Source.IsEmpty() && !Existing->Source.Contains(Source))
			{
				Existing->Source = JoinNonEmpty(
					{ Existing->Source, Source }, TEXT("; "));
			}
			return;
		}

		FTerritoryStoryOutcomeScenario& Scenario = Report.Scenarios.AddDefaulted_GetRef();
		Scenario.Category = Category;
		Scenario.Title = Title;
		Scenario.Certainty = Certainty;
		Scenario.When = When;
		Scenario.OnlyIf = OnlyIf;
		Scenario.Then = Then;
		Scenario.IfNot = IfNot;
		Scenario.AlsoAffects = AlsoAffects;
		Scenario.Source = Source;
	}

	FString DescribeStateEffects(const FTerritoryStateConfig& Config)
	{
		TArray<FString> Effects;
		if (!Config.GrantedCommandCapabilities.IsEmpty())
		{
			Effects.Add(FString::Printf(
				TEXT("The current owner receives %s while this row is active"),
				*DescribeCapabilities(Config.GrantedCommandCapabilities)));
		}
		if (Config.StealthProfileOverride)
		{
			Effects.Add(FString::Printf(TEXT("Stealth uses %s"),
				*Config.StealthProfileOverride->GetName()));
		}
		if (Config.Audio.bOverrideNarrativeMusic)
		{
			Effects.Add(Config.Audio.MusicTheme.IsValid()
				? FString::Printf(TEXT("Narrative Music selects %s%s"),
					*Config.Audio.MusicTheme.ToString(),
					Config.Audio.bImmediateThemeChange
						? TEXT(" immediately") : TEXT(" with its authored fade"))
				: TEXT("Narrative Music override is enabled but has no theme"));
		}
		if (Config.Audio.HasStateEffects())
		{
			Effects.Add(TEXT("configured local state-enter/state-exit sounds may play for a player inside"));
		}
		return JoinNonEmpty(Effects, TEXT("; "));
	}

	ETerritoryState ResolveInitialPlaceState(const UTerritoryDefinition* Definition)
	{
		switch (Definition->InitialState)
		{
		case ETerritoryInitialState::Claimed:
			return Definition->InitialOwningFaction.IsValid()
				? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		case ETerritoryInitialState::Unclaimed:
			return ETerritoryState::Unclaimed;
		case ETerritoryInitialState::Locked:
		case ETerritoryInitialState::Automatic:
		default:
			return Definition->InitialOwningFaction.IsValid()
				? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		}
	}

	ETerritoryAvailability ResolveInitialAvailability(
		const UTerritoryDefinition* Definition)
	{
		return Definition->InitialState == ETerritoryInitialState::Locked
			? ETerritoryAvailability::Locked : Definition->InitialAvailability;
	}

	void AddNewCampaignScenario(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryDefinition* Definition)
	{
		const FString Name = DisplayName(Definition);
		const ETerritoryAvailability Availability = ResolveInitialAvailability(Definition);
		if (Definition->IsA<UTerritoryPlaceDefinition>())
		{
			const ETerritoryState State = ResolveInitialPlaceState(Definition);
			FString Outcome = FString::Printf(TEXT("%s starts %s and %s"), *Name,
				*EnumDisplayName(Availability), *EnumDisplayName(State));
			if (State == ETerritoryState::Claimed)
			{
				Outcome += FString::Printf(TEXT(" by %s"),
					*TagOrNone(Definition->InitialOwningFaction));
			}
			Outcome += FString::Printf(TEXT(". It requests %d initial guards."),
				FMath::Max(0, Definition->InitialGuardCount));
			AddScenario(Report, TEXT("Campaign"), TEXT("New campaign seed"),
				ETerritoryStoryOutcomeCertainty::Configured,
				TEXT("A brand-new campaign is created; saved or replicated state is not being restored."),
				TEXT("The Place Definition is registered by the campaign City hierarchy."),
				Outcome,
				TEXT("A saved campaign keeps its saved owner, state and availability instead of reapplying this seed."),
				TEXT("Guard creation still requires valid Blueprint classes, NPC Definitions and loaded physical actors."),
				TEXT("03 New Campaign"));
		}
		else
		{
			AddScenario(Report, TEXT("Campaign"), TEXT("New campaign hierarchy seed"),
				ETerritoryStoryOutcomeCertainty::Configured,
				TEXT("A brand-new campaign is created."),
				TEXT("The complete authored child hierarchy is registered."),
				FString::Printf(TEXT("%s starts locally %s. Its political owner and state are derived from its children, not from Initial Owner or Initial State."),
					*Name, *EnumDisplayName(Availability)),
				TEXT("Missing, locked or streamed-out children cannot produce a false secure claim."),
				TEXT("State events may run later when child control changes the derived parent state."),
				TEXT("03 New Campaign; 10 Hierarchy"));
		}
	}

	void AddAvailabilityScenario(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryDefinition* Definition)
	{
		const FTerritoryStateConfig* Locked =
			Definition->StateConfigs.Find(ETerritoryState::Locked);
		const FString EntryConditions = Locked
			? DescribeConditions(Locked->EntryConditions) : FString();
		const FString ExitConditions = Locked
			? DescribeConditions(Locked->ExitConditions) : FString();
		const FString EntryEvents = Locked
			? DescribeEvents(Locked->EntryEvents) : FString();
		const FString ExitEvents = Locked
			? DescribeEvents(Locked->ExitEvents) : FString();
		const FString Effects = Locked
			? DescribeStateEffects(*Locked) : FString();
		const bool bCustomBlueprint = Locked
			&& (ContainsCustomBlueprintObject(Locked->EntryConditions)
				|| ContainsCustomBlueprintObject(Locked->ExitConditions)
				|| ContainsCustomBlueprintObject(Locked->EntryEvents)
				|| ContainsCustomBlueprintObject(Locked->ExitEvents));
		const bool bStartsLocked = ResolveInitialAvailability(Definition)
			== ETerritoryAvailability::Locked;
		AddScenario(Report, TEXT("Availability"),
			TEXT("Locked availability lifecycle"),
			bCustomBlueprint
				? ETerritoryStoryOutcomeCertainty::CustomBlueprint
				: (!EntryConditions.IsEmpty() || !ExitConditions.IsEmpty()
					? ETerritoryStoryOutcomeCertainty::RuntimeConditional
					: ETerritoryStoryOutcomeCertainty::Configured),
			bStartsLocked
				? TEXT("A new campaign starts this Territory Locked, or a trusted server action later tries to unlock it.")
				: TEXT("A trusted server action locks/unlocks this Territory, or an ancestor changes effective availability."),
			JoinNonEmpty({ EntryConditions.IsEmpty() ? FString()
					: FString::Printf(TEXT("Lock: %s"), *EntryConditions),
				ExitConditions.IsEmpty() ? FString()
					: FString::Printf(TEXT("Unlock: %s"), *ExitConditions) }, TEXT("; ")),
			JoinNonEmpty({ bStartsLocked
					? TEXT("The Territory stays hidden and silent until unlock succeeds.")
					: TEXT("Local availability starts open."),
				EntryEvents.IsEmpty() ? FString()
					: FString::Printf(TEXT("On lock: %s."), *EntryEvents),
				ExitEvents.IsEmpty() ? FString()
					: FString::Printf(TEXT("On unlock: %s."), *ExitEvents) }, TEXT(" ")),
			bStartsLocked
				? TEXT("A failed Locked exit condition keeps it hidden and silent for capture, guards, counterattacks, economy, production, perks and POI tracking.")
				: TEXT("A failed local condition blocks that requested lock/unlock. Effective availability also fails closed when an ancestor path is locked, missing or cyclic."),
			JoinNonEmpty({ TEXT("Automatic Hierarchy opens ancestors before a Place and does not reveal siblings. District/City targets attempt eligible descendants."), Effects }, TEXT("; ")),
			TEXT("03 New Campaign > Initial Availability; 05 State Rules > Locked"));
	}

	void AddStateRuleScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryDefinition* Definition)
	{
		const bool bAggregate = !Definition->IsA<UTerritoryPlaceDefinition>();
		for (const ETerritoryState State : { ETerritoryState::Unclaimed,
			ETerritoryState::Contested, ETerritoryState::Claimed })
		{
			const FTerritoryStateConfig* Config = Definition->StateConfigs.Find(State);
			if (!Config) continue;

			const FString EntryConditions = DescribeConditions(Config->EntryConditions);
			const FString ExitConditions = DescribeConditions(Config->ExitConditions);
			const FString EntryEvents = DescribeEvents(Config->EntryEvents);
			const FString ExitEvents = DescribeEvents(Config->ExitEvents);
			const FString Effects = DescribeStateEffects(*Config);

			const bool bHasContent = !EntryConditions.IsEmpty() || !ExitConditions.IsEmpty()
				|| !EntryEvents.IsEmpty() || !ExitEvents.IsEmpty() || !Effects.IsEmpty();
			if (!bHasContent) continue;

			const bool bParentPoliticalConditionsIgnored = bAggregate
				&& State != ETerritoryState::Locked
				&& (!Config->EntryConditions.IsEmpty() || !Config->ExitConditions.IsEmpty());
			const bool bContainsCustomBlueprint =
				ContainsCustomBlueprintObject(Config->EntryConditions)
				|| ContainsCustomBlueprintObject(Config->ExitConditions)
				|| ContainsCustomBlueprintObject(Config->EntryEvents)
				|| ContainsCustomBlueprintObject(Config->ExitEvents);
			const FString When = State == ETerritoryState::Locked
				? TEXT("Availability enters or leaves Locked.")
				: FString::Printf(TEXT("Political control enters or leaves %s."),
					*EnumDisplayName(State));
			FString Then = EntryEvents.IsEmpty()
				? TEXT("No entry event is authored.")
				: FString::Printf(TEXT("On entry: %s."), *EntryEvents);
			if (!ExitEvents.IsEmpty())
			{
				Then += FString::Printf(TEXT(" On exit: %s."), *ExitEvents);
			}

			AddScenario(Report, TEXT("State Rules"),
				FString::Printf(TEXT("%s lifecycle"), *EnumDisplayName(State)),
				bParentPoliticalConditionsIgnored
					? ETerritoryStoryOutcomeCertainty::Warning
					: (bContainsCustomBlueprint
						? ETerritoryStoryOutcomeCertainty::CustomBlueprint
					: (!EntryConditions.IsEmpty() || !ExitConditions.IsEmpty()
						? ETerritoryStoryOutcomeCertainty::RuntimeConditional
						: ETerritoryStoryOutcomeCertainty::Configured)),
				When,
				JoinNonEmpty({ EntryConditions.IsEmpty() ? FString()
					: FString::Printf(TEXT("Enter: %s"), *EntryConditions),
					ExitConditions.IsEmpty() ? FString()
					: FString::Printf(TEXT("Leave: %s"), *ExitConditions) }, TEXT("; ")),
				Then,
				bParentPoliticalConditionsIgnored
					? TEXT("City/District political state conditions cannot block the hierarchy reducer. Move a story gate to Locked Exit Conditions or to child Place rules. The parent events still run after the derived state changes.")
					: TEXT("A failed condition blocks that independent state transition or skips an event whose own inherited conditions fail."),
				Effects,
				FString::Printf(TEXT("05 State Rules > %s"), *EnumDisplayName(State)));
		}
	}

	void AddStealthScenario(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryPlaceDefinition* Place)
	{
		const UTerritoryStealthProfile* Profile = Place->DefaultStealthProfile;
		if (!Profile)
		{
			AddScenario(Report, TEXT("Stealth and Capture"), TEXT("Enter the Place"),
				ETerritoryStoryOutcomeCertainty::Configured,
				TEXT("A valid hostile player enters the physical story bounds."),
				Place->bStoryCaptureFromBounds
					? TEXT("Story Capture From Bounds is enabled and normal capture admission passes.")
					: TEXT("Story Capture From Bounds is disabled."),
				Place->bStoryCaptureFromBounds
					? TEXT("The player is registered as a contester immediately; there is no stealth deferral profile.")
					: TEXT("Entering bounds alone does not begin a story contest."),
				TEXT("Diplomacy, availability, defenders and capture eligibility can still reject or halt capture."),
				TEXT("Contested Entry Events run once when the state actually changes."),
				TEXT("05 State Rules > Default Stealth Profile; 06 Capture"));
			return;
		}

		const FString Exposure = FString::Printf(
			TEXT("Confirmed exposure uses %s. Point-blank sight %s at %.0f cm; firing while seen %s; unseen fire %s; damage %s."),
			*EnumDisplayName(Profile->EscalationScope),
			Profile->bPointBlankSightAlwaysExposes ? TEXT("forces exposure") : TEXT("uses normal evidence"),
			Profile->PointBlankSightExposureDistance,
			Profile->bFireWhileSeenExposes ? TEXT("exposes") : TEXT("does not automatically expose"),
			Profile->bFireWhileUnseenStartsInvestigation ? TEXT("starts investigation") : TEXT("is ignored by Territory investigation"),
			Profile->bDamageImmediatelyExposes ? TEXT("exposes immediately") : TEXT("uses accumulated evidence"));
		AddScenario(Report, TEXT("Stealth and Capture"), TEXT("Infiltration and exposure"),
			ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("A player enters this Place while the effective stealth profile is active."),
			Profile->bAllowStealthInfiltration
				? TEXT("The player has not yet reached an exposure threshold and has not failed a disguise identity check.")
				: TEXT("Stealth infiltration is disabled by this profile."),
			Profile->bAllowStealthInfiltration
				? TEXT("Presence begins Undetected. Suspicious evidence may start Narrative investigation without starting political conflict.")
				: TEXT("The normal immediate story-bounds contest flow is used."),
			TEXT("If confirmed, active temporary stealth abilities/effects may be cancelled and the configured escalation policy runs."),
			Exposure,
			TEXT("05 State Rules > Default Stealth Profile (or active State override)"));
	}

	void AddCaptureAndDefenderScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryPlaceDefinition* Place)
	{
		FString CaptureMode;
		ETerritoryStoryOutcomeCertainty CaptureCertainty =
			ETerritoryStoryOutcomeCertainty::RuntimeConditional;
		if (Place->bStoryCaptureFromBounds)
		{
			CaptureMode = TEXT("The complete Place bounds provide story contest participation. Automatic Capture Point progress is disabled, even when the Capture Point row is enabled.");
		}
		else if (Place->CapturePoint.bEnabled)
		{
			CaptureMode = Place->CapturePoint.bAutomaticCapture
				? FString::Printf(TEXT("The physical Capture Point builds normal domination/multiplayer progress inside its %.0f cm radius."), Place->CapturePoint.CaptureRadius)
				: TEXT("The Capture Point is presentation/manual interaction only; it does not automatically add capture progress.");
		}
		else
		{
			CaptureMode = TEXT("No story-bounds or Capture Point adapter is enabled. Capture requires an explicit Narrative/server mutation path.");
			CaptureCertainty = ETerritoryStoryOutcomeCertainty::Warning;
		}

		AddScenario(Report, TEXT("Stealth and Capture"), TEXT("Capture admission and progress"),
			CaptureCertainty,
			TEXT("A hostile faction attempts to contest and capture this Place."),
			TEXT("The Place and its ancestors are available, diplomacy allows capture, the attacker is alive, and all state conditions for an independent Place pass."),
			CaptureMode,
			TEXT("Defenders, blocked diplomacy, a lock, failed state conditions or no active capture pressure stop/decay progress."),
			FString::Printf(TEXT("At most %d strategic attackers participate concurrently before Narrative difficulty applies its optional lower attack-token cap."),
				FMath::Max(1, Place->MaxConcurrentAttackers)),
			TEXT("06 Capture; 03 New Campaign > Max Concurrent Attackers"));

		const FString DefenderEvents = DescribeEvents(Place->DefenderDiedEvents);
		const FString AllDefeatedEvents = DescribeEvents(Place->AllDefendersDefeatedEvents);
		const bool bCustomDefenderEvent =
			ContainsCustomBlueprintObject(Place->DefenderDiedEvents)
			|| ContainsCustomBlueprintObject(Place->AllDefendersDefeatedEvents);
		AddScenario(Report, TEXT("Combat and Handover"), TEXT("Defenders are defeated"),
			bCustomDefenderEvent
				? ETerritoryStoryOutcomeCertainty::CustomBlueprint
				: (!DefenderEvents.IsEmpty() || !AllDefeatedEvents.IsEmpty())
				? ETerritoryStoryOutcomeCertainty::RuntimeConditional
				: ETerritoryStoryOutcomeCertainty::Configured,
			TEXT("A registered defender dies; the final branch happens when no registered defender remains."),
			TEXT("The server receives a valid Narrative ASC death transition for a defender belonging to this Place."),
			JoinNonEmpty({ DefenderEvents.IsEmpty() ? TEXT("Each defender death has no authored Narrative event.")
				: FString::Printf(TEXT("Each death: %s."), *DefenderEvents),
				AllDefeatedEvents.IsEmpty() ? TEXT("All defenders defeated has no authored Narrative event.")
				: FString::Printf(TEXT("Final defender: %s."), *AllDefeatedEvents) }, TEXT(" ")),
			TEXT("Event-inherited conditions can skip an event. Defender defeat alone never changes ownership."),
			Place->StoryOwner.bEnabled
				? FString::Printf(TEXT("The configured Story Owner may activate at %.0f cm interaction distance%s."),
					Place->StoryOwner.InteractionDistance,
					Place->StoryOwner.bBeginDialogueOnActivation ? TEXT(" and begin dialogue") : TEXT(""))
				: TEXT("No Story Owner is configured; capture must finish through capture progress or another Narrative event."),
			TEXT("07 Guards > Narrative; 11 Place > Story Owner"));

		FString Garrison;
		switch (Place->PostCaptureGarrisonPolicy)
		{
		case ETerritoryPostCaptureGarrisonPolicy::ConfiguredForEveryOwner:
			Garrison = FString::Printf(TEXT("Every new owner requests %d guards."), Place->InitialGuardCount);
			break;
		case ETerritoryPostCaptureGarrisonPolicy::AlwaysUnstaffed:
			Garrison = TEXT("Every new owner starts with zero assigned guards.");
			break;
		case ETerritoryPostCaptureGarrisonPolicy::PlayerChooses:
		default:
			Garrison = FString::Printf(TEXT("A player-faction owner starts with zero assigned guards; AI/world captures request %d."), Place->InitialGuardCount);
			break;
		}
		AddScenario(Report, TEXT("Combat and Handover"), TEXT("Ownership handover completes"),
			ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("Capture progress reaches completion or an authorized Story Owner/Narrative handover requests a real faction change."),
			TEXT("Final availability, diplomacy, state conditions, defenders and capture context validation pass."),
			FString::Printf(TEXT("The Place becomes Claimed by the requesting faction. %s Guard selection follows the new owner through faction mappings, then the default NPC Definition."), *Garrison),
			TEXT("A same-owner reset does not replay capture rewards. A Faction A to Faction B handover runs Claimed Exit events for the old tenure, then Claimed Entry events for the new tenure."),
			TEXT("The parent District immediately recalculates control from every authored Place."),
			TEXT("03 New Campaign; 05 State Rules > Claimed; 07 Guards"));
	}

	FString DescribeResourceRates(const TArray<FTerritoryResourceRate>& Rates,
		const TCHAR* Direction)
	{
		TArray<FString> Rows;
		for (const FTerritoryResourceRate& Rate : Rates)
		{
			if (!Rate.ItemClass || Rate.QuantityPerCycle <= 0) continue;
			Rows.Add(FString::Printf(TEXT("%s %d %s%s"), Direction,
				Rate.QuantityPerCycle, *Rate.ItemClass->GetName(),
				Rate.QuantityPerUpgradeLevel > 0
					? *FString::Printf(TEXT(" (+%d per upgrade)"), Rate.QuantityPerUpgradeLevel)
					: TEXT("")));
		}
		return FString::Join(Rows, TEXT(", "));
	}

	void AddEconomyScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryPlaceDefinition* Place)
	{
		AddScenario(Report, TEXT("Economy"), TEXT("Claimed income and upkeep"),
			ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("The Territory economy processes a cycle for the current owner."),
			TEXT("The Place and ancestor path are available and the Place is securely Claimed."),
			FString::Printf(TEXT("The authored rate is %d income minus %d upkeep per assigned guard. Recruitment costs %d per guard. Each upgrade can add %d income."),
				Place->PeriodicIncome, Place->GuardUpkeepPerCycle,
				Place->GuardRecruitmentCost, Place->IncomeBonusPerLevel),
			TEXT("Locked, unclaimed, contested or lost Places do not pay their former owner."),
			TEXT("Narrative inventory/account policy remains the authority for the real currency transaction."),
			TEXT("04 Economy; 10 Place > Upgrades"));

		if (!Place->ProductionProfile) return;
		const UTerritoryProductionProfile* Profile = Place->ProductionProfile;
		for (const FTerritoryProductionRule& Rule : Profile->Rules)
		{
			const FString Inputs = DescribeResourceRates(Rule.Inputs, TEXT("consume"));
			const FString Outputs = DescribeResourceRates(Rule.Outputs, TEXT("produce"));
			AddScenario(Report, TEXT("Economy"),
				Rule.DisplayName.IsEmpty() ? Rule.RuleTag.ToString() : Rule.DisplayName.ToString(),
				ETerritoryStoryOutcomeCertainty::RuntimeConditional,
				TEXT("A new Narrative campaign production cycle is available for this rule."),
				FString::Printf(TEXT("Rule is enabled; upgrade level is at least %d%s%s; the owning faction has a valid Narrative inventory with enough storage."),
					Rule.MinimumUpgradeLevel,
					Rule.bRequiresClaimedState ? TEXT("; Place is Claimed") : TEXT(""),
					Rule.bPauseWhileContested ? TEXT("; Place is not Contested") : TEXT("")),
				JoinNonEmpty({ Inputs, Outputs }, TEXT("; ")),
				TEXT("Missing input consumes that cycle without output. Unavailable/full storage remains pending within the configured catch-up limit. Inactive state or upgrade gates consume the cycle as inactive."),
				TEXT("Losing or locking the Place stops future production for the former owner; all real items remain in Narrative inventory."),
				FString::Printf(TEXT("10 Place > Production > %s"),
					Rule.RuleTag.IsValid() ? *Rule.RuleTag.ToString() : TEXT("Unnamed Rule")));
		}
	}

	void AddCounterAttackScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryPlaceDefinition* Place)
	{
		const UTerritoryCounterAttackProfile* Profile = Place->CounterAttackProfile;
		if (!Profile) return;
		for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
		{
			const FString Schedule = Force.ScheduleMode == ETerritoryCounterScheduleMode::FiniteSeries
				? FString::Printf(TEXT("a finite series of at most %d battles"),
					Force.MaximumScheduledAssaults)
				: (Force.ScheduleMode == ETerritoryCounterScheduleMode::SingleAssault
					? TEXT("one battle") : TEXT("an unlimited schedule of separate finite battles"));
			const FString Staging = Force.StagingRequirement
				== ETerritoryAssaultStagingRequirement::OwnsSecureDistrict
				? TEXT("the attacker owns at least one loaded, unlocked District whose complete authored Place set is secure")
				: TEXT("no Territory holding is required by this force row");
			const FString TimeGate = Force.TimePolicy
				== ETerritoryCounterTimePolicy::NarrativeTimeWindow
				? FString::Printf(TEXT("Narrative time is inside %.0f-%.0f"),
					Force.TimeWindowStart, Force.TimeWindowEnd)
				: TEXT("any Narrative campaign time is allowed");
			TArray<FString> QuestDescriptions;
			for (const FTerritoryCounterAttackQuestRule& Rule : Profile->QuestRules)
			{
				QuestDescriptions.Add(FString::Printf(TEXT("%s: %s %s for %s"),
					Rule.QuestClass ? *Rule.QuestClass->GetName() : TEXT("missing quest"),
					*EnumDisplayName(Rule.Action), *EnumDisplayName(Rule.QuestState),
					*EnumDisplayName(Rule.PlayerScope)));
			}
			const FString QuestGate = Profile->QuestRules.IsEmpty()
				? TEXT("no automatic counterattack quest rule is authored")
				: FString::Printf(TEXT("all Narrative quest rules pass (%s)"),
					*FString::Join(QuestDescriptions, TEXT("; ")));
			const FString CapabilityGate =
				Profile->bRequireReinforcementCapabilityForStrategicCounterattacks
				? TEXT("the attacking faction still owns the Reinforcements capability")
				: TEXT("the profile does not require the Reinforcements capability");
			int32 VehicleApproaches = 0;
			for (const FTerritoryAssaultApproach& Approach :
				Place->CounterAttackApproaches)
			{
				if (Approach.bEnabled
					&& Approach.EntryType ==
						ETerritoryAssaultEntryType::NarrativeVehicle)
				{
					++VehicleApproaches;
				}
			}

			AddScenario(Report, TEXT("Counterattack"),
				FString::Printf(TEXT("%s strategic response"),
					Force.Faction.IsValid() ? *Force.Faction.ToString() : TEXT("Unconfigured faction")),
				ETerritoryStoryOutcomeCertainty::ChanceBased,
				TEXT("Another faction captures this Place and the post-capture grace period ends."),
				FString::Printf(TEXT("Diplomacy is War; %s; %s; %s; %s; a valid route, finite budget and probability decision pass."),
					*Staging, *CapabilityGate, *QuestGate, *TimeGate),
				FString::Printf(TEXT("The force may schedule %s. Each battle has %d total attackers in waves of at most %d. %d enabled vehicle approaches may use the faction signature vehicle or route fallback."),
					*Schedule, Force.PlannedForce, Force.WaveSize, VehicleApproaches),
				TEXT("Any failed hard gate cancels or blocks the response. A failed probability roll changes no ownership and spawns nobody."),
				Profile->bRequirePlayerProximityForActivation
					? FString::Printf(TEXT("Deployment waits for a relevant player within %.0f cm."), Profile->ActivationRadius)
					: TEXT("After warning, attackers may deploy and fight Place guards without waiting for the player."),
				TEXT("08 Counter Attack > Profile, Faction Forces and Approaches"));
		}

		AddScenario(Report, TEXT("Counterattack"), TEXT("Cleared Place recapture"),
			ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("A physical counterattack clears every defending guard and begins holding the Place."),
			TEXT("The assault still has living finite attackers and is allowed to capture Territory."),
			Profile->bUseUnattendedRecaptureHandover
				? FString::Printf(TEXT("With no living defending player in the Place or District, a %.0f campaign-time handover countdown begins."), Profile->UnattendedRecaptureDelayGameTime)
				: TEXT("The normal physical capture flow continues without an unattended handover countdown."),
			TEXT("A living defender returning to the Place or District cancels the unattended countdown and requires the fight to finish."),
			Profile->bConcedeWhenDefendingPlayerDies
				? TEXT("If the cleared Place has a defending player who dies before respawn, it is conceded immediately.")
				: TEXT("Defending-player death does not use the immediate concession rule."),
			TEXT("08 Counter Attack > Scheduling > Recapture"));
	}

	void AddManagementScenario(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryDefinition* Definition)
	{
		if (!Definition->ManagementPoint.bEnabled) return;
		AddScenario(Report, TEXT("Presentation"), TEXT("Open Territory management"),
			ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("A player interacts with the configured Narrative POI/management actor."),
			FString::Printf(TEXT("The actor is loaded, the Territory is effectively available, and the player is within %.0f cm."),
				Definition->ManagementPoint.InteractionDistance),
			TEXT("The configured Territory management widget opens on its Narrative/CommonUI layer."),
			TEXT("Locked or unloaded physical targets remain silent; the strategic directory can still show permitted read-only information."),
			Definition->ManagementPoint.ManagedDistrictOverride.IsValid()
				? FString::Printf(TEXT("Commands target %s."),
					*Definition->ManagementPoint.ManagedDistrictOverride.ToString())
				: TEXT("Commands target this District, or the parent District when the point belongs to a Place."),
			TEXT("09 Management"));
	}

	void AddDistrictScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryDistrictDefinition* District)
	{
		TArray<FString> ChildNames;
		for (const UTerritoryPlaceDefinition* Place : District->Places)
		{
			ChildNames.Add(Place ? DisplayName(Place) : TEXT("invalid Place row"));
		}
		AddScenario(Report, TEXT("Hierarchy"), TEXT("District control is reduced from Places"),
			District->Places.IsEmpty()
				? ETerritoryStoryOutcomeCertainty::Warning
				: ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("Any authored child Place changes owner, political state or availability, or streams in/out."),
			TEXT("The exact District Places array is the only child authority."),
			FString::Printf(TEXT("All %d Places loaded, unlocked, Claimed and owned by one faction makes the District Claimed by that faction. Children: %s."),
				District->Places.Num(), *FString::Join(ChildNames, TEXT(", "))),
			TEXT("Mixed, partial, locked, contested or missing child control cannot produce a secure claim. Political child control produces Contested; no political control produces Unclaimed."),
			District->bIsCapital
				? FString::Printf(TEXT("This is a capital District: capture grants the configured capital reward path and child income uses the %.2fx capital multiplier where applicable."), District->CapitalIncomeMultiplier)
				: TEXT("This is not marked as the capital District."),
			TEXT("10 District > Hierarchy and Economy"));
	}

	void AddCityScenarios(FTerritoryStoryOutcomeReport& Report,
		const UTerritoryCityDefinition* City)
	{
		TArray<FString> ChildNames;
		for (const UTerritoryDistrictDefinition* District : City->Districts)
		{
			ChildNames.Add(District ? DisplayName(District) : TEXT("invalid District row"));
		}
		AddScenario(Report, TEXT("Hierarchy"), TEXT("City control is reduced from Districts"),
			City->Districts.IsEmpty()
				? ETerritoryStoryOutcomeCertainty::Warning
				: ETerritoryStoryOutcomeCertainty::RuntimeConditional,
			TEXT("Any authored District changes its derived state, owner or availability, or streams in/out."),
			TEXT("The exact City Districts array is the only child authority."),
			FString::Printf(TEXT("All %d Districts loaded, unlocked, Claimed and owned by one faction makes the City Claimed by that faction. Districts: %s."),
				City->Districts.Num(), *FString::Join(ChildNames, TEXT(", "))),
			TEXT("Mixed, partial, locked, contested or missing District control cannot produce a secure City claim."),
			TEXT("City capture/loss delegates and State events run only after the derived result is committed; no parent capture rewrites a child."),
			TEXT("10 City > Hierarchy"));
	}
}

int32 FTerritoryStoryOutcomeReport::Count(
	ETerritoryStoryOutcomeCertainty Certainty) const
{
	int32 Result = 0;
	for (const FTerritoryStoryOutcomeScenario& Scenario : Scenarios)
	{
		if (Scenario.Certainty == Certainty) ++Result;
	}
	return Result;
}

FString FTerritoryStoryOutcomeReport::BuildPlainText() const
{
	FString Text = FString::Printf(TEXT("STORY OUTCOME (READ ONLY)\n%s | %s\n%s\n\n"),
		*DefinitionName, *DefinitionType, *Headline);
	for (const FTerritoryStoryOutcomeScenario& Scenario : Scenarios)
	{
		Text += FString::Printf(TEXT("[%s] %s — %s\n"), *Scenario.Category,
			*Scenario.Title,
			*FTerritoryStoryOutcomeAnalyzer::CertaintyLabel(Scenario.Certainty));
		if (!Scenario.When.IsEmpty()) Text += TEXT("When: ") + Scenario.When + TEXT("\n");
		if (!Scenario.OnlyIf.IsEmpty()) Text += TEXT("Only if: ") + Scenario.OnlyIf + TEXT("\n");
		if (!Scenario.Then.IsEmpty()) Text += TEXT("Then: ") + Scenario.Then + TEXT("\n");
		if (!Scenario.IfNot.IsEmpty()) Text += TEXT("If not: ") + Scenario.IfNot + TEXT("\n");
		if (!Scenario.AlsoAffects.IsEmpty()) Text += TEXT("Also affects: ") + Scenario.AlsoAffects + TEXT("\n");
		if (!Scenario.Source.IsEmpty()) Text += TEXT("Based on: ") + Scenario.Source + TEXT("\n");
		Text += TEXT("\n");
	}
	if (!ValidationErrors.IsEmpty() || !ValidationWarnings.IsEmpty())
	{
		Text += TEXT("SETUP HEALTH\n");
		for (const FString& Error : ValidationErrors) Text += TEXT("Error: ") + Error + TEXT("\n");
		for (const FString& Warning : ValidationWarnings) Text += TEXT("Warning: ") + Warning + TEXT("\n");
	}
	return Text;
}

FTerritoryStoryOutcomeReport FTerritoryStoryOutcomeAnalyzer::Analyze(
	const UTerritoryDefinition* Definition, bool bIncludeFullValidation)
{
	FTerritoryStoryOutcomeReport Report;
	if (!Definition)
	{
		Report.DefinitionName = TEXT("Invalid Territory Definition");
		Report.DefinitionType = TEXT("Unknown");
		Report.Headline = TEXT("No asset is available to analyze.");
		return Report;
	}

	Report.DefinitionName = DisplayName(Definition);
	Report.DefinitionType = Definition->IsA<UTerritoryCityDefinition>() ? TEXT("City")
		: (Definition->IsA<UTerritoryDistrictDefinition>() ? TEXT("District") : TEXT("Place"));

	AddNewCampaignScenario(Report, Definition);
	AddAvailabilityScenario(Report, Definition);
	AddStateRuleScenarios(Report, Definition);
	AddManagementScenario(Report, Definition);

	if (const UTerritoryPlaceDefinition* Place =
		Cast<UTerritoryPlaceDefinition>(Definition))
	{
		AddStealthScenario(Report, Place);
		AddCaptureAndDefenderScenarios(Report, Place);
		AddEconomyScenarios(Report, Place);
		AddCounterAttackScenarios(Report, Place);
	}
	else if (const UTerritoryDistrictDefinition* District =
		Cast<UTerritoryDistrictDefinition>(Definition))
	{
		AddDistrictScenarios(Report, District);
	}
	else if (const UTerritoryCityDefinition* City =
		Cast<UTerritoryCityDefinition>(Definition))
	{
		AddCityScenarios(Report, City);
	}

	if (bIncludeFullValidation)
	{
		UTerritoryDataValidator::ValidateDefinition(
			const_cast<UTerritoryDefinition*>(Definition),
			Report.ValidationErrors, Report.ValidationWarnings);
	}

	Report.Headline = FString::Printf(TEXT("%d scenarios • %d configured • %d runtime-dependent • %d chance-based • %d custom Blueprint • %d setup issues"),
		Report.Scenarios.Num(),
		Report.Count(ETerritoryStoryOutcomeCertainty::Configured),
		Report.Count(ETerritoryStoryOutcomeCertainty::RuntimeConditional),
		Report.Count(ETerritoryStoryOutcomeCertainty::ChanceBased),
		Report.Count(ETerritoryStoryOutcomeCertainty::CustomBlueprint),
		Report.ValidationErrors.Num() + Report.ValidationWarnings.Num()
			+ Report.Count(ETerritoryStoryOutcomeCertainty::Warning));
	return Report;
}

FString FTerritoryStoryOutcomeAnalyzer::CertaintyLabel(
	ETerritoryStoryOutcomeCertainty Certainty)
{
	switch (Certainty)
	{
	case ETerritoryStoryOutcomeCertainty::Configured: return TEXT("Configured Result");
	case ETerritoryStoryOutcomeCertainty::RuntimeConditional: return TEXT("Runtime Condition");
	case ETerritoryStoryOutcomeCertainty::ChanceBased: return TEXT("Chance-Based");
	case ETerritoryStoryOutcomeCertainty::CustomBlueprint: return TEXT("Custom Blueprint");
	case ETerritoryStoryOutcomeCertainty::Warning: return TEXT("Setup Warning");
	default: return TEXT("Unknown");
	}
}
