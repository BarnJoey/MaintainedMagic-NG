#include "Run.hpp"
#include <algorithm>
#include <chrono>
#include <numeric>

namespace MAINT
{
	// -- Small helpers --------------------------------------------------------

	static inline bool IsBoundWeaponSpell(const RE::SpellItem* spell)
	{
		return spell && !spell->effects.empty() &&
		       spell->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kBoundWeapon);
	}

	static inline bool IsSummonSpell(const RE::SpellItem* spell)
	{
		return spell && !spell->effects.empty() &&
		       spell->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kSummonCreature);
	}

	static inline bool IsSelfDelivery(const RE::SpellItem* spell)
	{
		return spell && spell->data.delivery == RE::MagicSystem::Delivery::kSelf;
	}

	static inline const RE::BGSEquipSlot* LeftHandSlot()
	{
		static const auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(SLOT_LEFT_HAND);
		return slot;
	}
	static inline const RE::BGSEquipSlot* RightHandSlot()
	{
		static const auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(SLOT_RIGHT_HAND);
		return slot;
	}

	// Silences FXPersist-flag visuals on a maintained spell (returns those we changed so they can be restored later)
	static std::vector<RE::Effect*> SilenceSpellFX(RE::SpellItem* const& theSpell)
	{
		std::vector<RE::Effect*> ret;
		ret.reserve(theSpell ? theSpell->effects.size() : 0);

		for (auto const& eff : theSpell->effects) {
			const bool persists = eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			if (!persists)
				continue;

			switch (eff->baseEffect->GetArchetype()) {
			case RE::EffectSetting::Archetype::kLight:
			case RE::EffectSetting::Archetype::kBoundWeapon:
			case RE::EffectSetting::Archetype::kDisguise:
			case RE::EffectSetting::Archetype::kSummonCreature:
			case RE::EffectSetting::Archetype::kNightEye:
			case RE::EffectSetting::Archetype::kInvisibility:
			case RE::EffectSetting::Archetype::kGuide:
			case RE::EffectSetting::Archetype::kWerewolf:
			case RE::EffectSetting::Archetype::kWerewolfFeed:
				// informational only; avoid heavy formatting when not logging
				spdlog::debug("{} fx will not be silenced", eff->baseEffect->GetName());
				break;
			default:
				eff->baseEffect->data.flags.reset(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
				ret.emplace_back(eff);
			}
		}
		return ret;
	}

	static bool IsMaintainable(RE::SpellItem* const& theSpell, RE::Actor* const& theCaster)
	{
		if (!theSpell || !theCaster)
			return false;

		if (theSpell->As<RE::ScrollItem>()) {
			spdlog::info("Spell is Scroll");
			return false;
		}
		if (theSpell->As<RE::EnchantmentItem>()) {
			spdlog::info("Spell is Enchantment");
			return false;
		}
		if (theSpell->effects.empty()) {
			spdlog::info("Spell has no effects");
			return false;
		}
		if (theSpell->data.castingType != RE::MagicSystem::CastingType::kFireAndForget) {
			spdlog::info("Spell is not FF");
			return false;
		}
		{
			const auto dur = theSpell->effects.front()->GetDuration();
			if (dur <= 5.0) {
				spdlog::info("Spell has duration of 5 seconds or less");
				return false;
			}
		}
		{
			const auto costWithCaster = theSpell->CalculateMagickaCost(theCaster);
			const auto costWithoutCaster = theSpell->CalculateMagickaCost(nullptr);
			if (costWithCaster <= 5.0 && costWithoutCaster <= 5.0) {
				spdlog::info("Spell has cost of 5 or less");
				return false;
			}
		}

		// Keywords
		const auto& forms = MAINT::FORMS::GetSingleton();
		if (theSpell->HasKeyword(forms.KywdMaintainedSpell)) {
			spdlog::info("Spell has Maintained keyword");
			return false;
		}
		if (theSpell->HasKeyword(forms.KywdExcludeFromSystem)) {
			spdlog::info("Spell has exclusion keyword");
			return false;
		}
		if (theSpell->HasKeywordString("_m3HealerDummySpell")) {
			spdlog::info("Spell has Allylink keyword");
			return false;
		}

		// Delivery and archetype checks
		const auto archetype = theSpell->effects[0]->baseEffect->GetArchetype();
		if (!IsSelfDelivery(theSpell)) {
			if (archetype == RE::EffectSetting::Archetype::kSummonCreature)
				return true;
			spdlog::info("Spell does not target self, and is not summon");
			return false;
		}

		// Allow bound weapons (optional)
		if (archetype == RE::EffectSetting::Archetype::kBoundWeapon) {
			if (!MAINT::CONFIG::AllowBoundWeapons) {
				spdlog::info("Spell is bound weapon");
				return false;
			}

			// Disallow maintaining if already dual-equipped as a spell that summons the same associated form in both hands
			const auto leftObj = theCaster->GetEquippedObject(true);
			const auto rightObj = theCaster->GetEquippedObject(false);
			const auto leftSpell = leftObj ? leftObj->As<RE::SpellItem>() : nullptr;
			const auto rightSpell = rightObj ? rightObj->As<RE::SpellItem>() : nullptr;

			const auto assoc = theSpell->effects[0]->baseEffect->data.associatedForm;
			const bool leftSame = leftSpell && !leftSpell->effects.empty() && leftSpell->effects[0]->baseEffect->data.associatedForm == assoc;
			const bool rightSame = rightSpell && !rightSpell->effects.empty() && rightSpell->effects[0]->baseEffect->data.associatedForm == assoc;

			if (leftSame && rightSame) {
				spdlog::info("Player already has this bound weapon");
				RE::DebugNotification(std::format("Only one instance of {} can be maintained.", theSpell->GetName()).c_str());
				return false;
			}
		}

		return true;
	}

	static RE::SpellItem* CreateMaintainSpell(RE::SpellItem* const& theSpell)
	{
		static auto const& spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		const auto* file = theSpell->GetFile(0);
		const auto& fileString = file ? file->GetFilename() : "VIRTUAL";
		
		spdlog::info("Maintainify({})", theSpell->GetName());
		spdlog::info(" - FID {:08X}", file ? theSpell->GetLocalFormID() : theSpell->GetFormID());
		spdlog::info(" - File {}", fileString);

		//spdlog::info("Maintainify({}, 0x{:08X}~{})", theSpell->GetName(), theSpell->GetLocalFormID(), fileString);


		auto* infiniteSpell = spellFactory->Create();
		infiniteSpell->SetFormID(MAINT::FORMS::GetSingleton().NextFormID(), false);

		infiniteSpell->fullName = std::format("Maintained {}", theSpell->GetFullName());

		// Copy relevant data
		infiniteSpell->data = theSpell->data;
		infiniteSpell->avEffectSetting = theSpell->avEffectSetting;
		infiniteSpell->boundData = theSpell->boundData;
		infiniteSpell->descriptionText = theSpell->descriptionText;
		infiniteSpell->equipSlot = theSpell->equipSlot;

		// Convert to constant self ability
		infiniteSpell->data.spellType = RE::MagicSystem::SpellType::kAbility;
		infiniteSpell->SetDelivery(RE::MagicSystem::Delivery::kSelf);
		infiniteSpell->SetCastingType(RE::MagicSystem::CastingType::kConstantEffect);

		// Copy keywords
		for (uint32_t i = 0; i < theSpell->numKeywords; ++i)
			infiniteSpell->AddKeyword(theSpell->GetKeywordAt(i).value());

		// Tag Cloak for toggles
		for (auto const& eff : theSpell->effects) {
			if (eff->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kCloak)) {
				infiniteSpell->AddKeyword(MAINT::FORMS::GetSingleton().KywdMagicCloak);
				break;
			}
		}

		infiniteSpell->AddKeyword(MAINT::FORMS::GetSingleton().KywdMaintainedSpell);
		infiniteSpell->effects = theSpell->effects;

		return infiniteSpell;
	}

	static RE::SpellItem* CreateDebuffSpell(RE::SpellItem* const& theSpell, float const& magnitude)
	{
		static auto const& debuffSpellTemplate = MAINT::FORMS::GetSingleton().SpelMagickaDebuffTemplate;
		static auto const& spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();

		const auto* file = theSpell->GetFile(0);
		const auto& fileString = file ? file->GetFilename() : "VIRTUAL";
		spdlog::info("Debuffify({}, 0x{:08X}~{})", theSpell->GetName(), file ? theSpell->GetLocalFormID() : theSpell->GetFormID(), fileString);

		auto* debuffSpell = spellFactory->Create();
		debuffSpell->SetFormID(MAINT::FORMS::GetSingleton().NextFormID(), false);

		debuffSpell->fullName = std::format("Maintained {}", theSpell->GetFullName());

		debuffSpell->data = RE::SpellItem::Data{ debuffSpellTemplate->data };
		debuffSpell->avEffectSetting = debuffSpellTemplate->avEffectSetting;
		debuffSpell->boundData = debuffSpellTemplate->boundData;

		debuffSpell->equipSlot = MAINT::FORMS::GetSingleton().EquipSlotVoice;

		debuffSpell->data.spellType = RE::MagicSystem::SpellType::kAbility;
		debuffSpell->SetDelivery(RE::MagicSystem::Delivery::kSelf);
		debuffSpell->SetCastingType(RE::MagicSystem::CastingType::kConstantEffect);

		debuffSpell->AddKeyword(MAINT::FORMS::GetSingleton().KywdMaintainedSpell);

		debuffSpell->effects.emplace_back(debuffSpellTemplate->effects.front());
		debuffSpell->effects.back()->effectItem.magnitude = magnitude;

		return debuffSpell;
	}

	static void BuildActiveSpellsCache()
	{
		spdlog::info("BuildActiveSpellsCache()");
		static const auto& player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			spdlog::error("\tPlayer is NULL");
			return;
		}

		// Repopulate the toggle FLST
		for (const auto& playerSpell : player->GetActorRuntimeData().addedSpells) {
			if (!MAINT::CACHE::SpellToMaintainedSpell.containsKey(playerSpell))
				continue;
			MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->AddForm(playerSpell);
		}

		// Restore debuff magnitudes from live state
		const auto& activeEffects = player->AsMagicTarget()->GetActiveEffectList();
		for (const auto& [baseSpell, maintData] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = maintData;
			for (const auto& aeff : *activeEffects) {
				if (aeff->spell == debuffSpell && aeff->GetCasterActor().get() == player && aeff->effect == debuffSpell->effects.front()) {
					spdlog::info("Restoring {} magnitude to {}", debuffSpell->GetName(), std::abs(aeff->GetMagnitude()));
					debuffSpell->effects.front()->effectItem.magnitude = std::abs(aeff->GetMagnitude());
					break;
				}
			}
		}
	}

	static void Purge()
	{
		spdlog::info("Purge()");
		for (const auto& [_, v] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = v;
			maintSpell->SetDelete(true);
			debuffSpell->SetDelete(true);
		}
		MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->ClearData();
		MAINT::CACHE::SpellToMaintainedSpell.clear();
	}

	static float CalculateUpkeepCost(RE::SpellItem* const& baseSpell, RE::Actor* const& theCaster)
	{
		static auto const& NEUTRAL_DURATION = static_cast<float>(MAINT::CONFIG::CostBaseDuration);
		static auto const& EXPONENT = MAINT::CONFIG::CostReductionExponent;

		spdlog::info("CalculateUpkeepCost()");
		const float baseCost = baseSpell->CalculateMagickaCost(theCaster);

		if (NEUTRAL_DURATION == 0.0F)
			return baseCost;

		const auto& firstEff = baseSpell->effects.front();
		const uint32_t effectiveDuration = std::max<uint32_t>(1u, firstEff->GetDuration());
		float mult = effectiveDuration < NEUTRAL_DURATION ? std::pow(NEUTRAL_DURATION / effectiveDuration, 2.0f) : std::sqrt(NEUTRAL_DURATION / effectiveDuration);

		const float baseDur = static_cast<float>(firstEff->GetDuration());
		float finalDur = baseDur;

		for (const auto& aeff : *theCaster->AsMagicTarget()->GetActiveEffectList()) {
			if (aeff->spell == baseSpell && aeff->GetCasterActor().get() == theCaster && aeff->effect == firstEff) {
				finalDur = aeff->duration;
				const float durmult = std::sqrt(baseDur / finalDur);
				mult *= durmult;

				spdlog::info("NeutralDur {} vs BaseDur {} vs RealDur {} => Cost Mult: {}x", NEUTRAL_DURATION, baseDur, aeff->duration, mult);
				break;
			}
		}

		if (finalDur > baseDur) {
			mult *= std::pow(NEUTRAL_DURATION / finalDur, EXPONENT);
			spdlog::info("Mult after CostReductionExponent: {}x", mult);
		}

		return std::round(baseCost * mult);
	}

	static void MaintainSpell(RE::SpellItem* const& baseSpell, RE::Actor* const& theCaster)
	{
		spdlog::info("MaintainSpell({}, 0x{:08X})", baseSpell->GetName(), baseSpell->GetFormID());

		if (!IsMaintainable(baseSpell, theCaster)) {
			RE::DebugNotification(std::format("Cannot maintain {}.", baseSpell->GetName()).c_str());
			return;
		}

		const float baseCost = baseSpell->CalculateMagickaCost(theCaster);
		const float magCost = CalculateUpkeepCost(baseSpell, theCaster);

		if (magCost > theCaster->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) + baseCost) {
			RE::DebugNotification(std::format("Need {} Magicka to maintain {}.", static_cast<uint32_t>(magCost), baseSpell->GetName()).c_str());
			return;
		}

		if (MAINT::CACHE::SpellToMaintainedSpell.containsKey(baseSpell)) {
			spdlog::info("\tActor already has constant version of {}.", baseSpell->GetName());
			return;
		}

		auto* maintSpell = CreateMaintainSpell(baseSpell);
		auto* debuffSpell = CreateDebuffSpell(baseSpell, magCost);

		spdlog::info("\tRemoving Base Effect of {}", baseSpell->GetName());
		auto handle = theCaster->GetHandle();
		theCaster->AsMagicTarget()->DispelEffect(baseSpell, handle);
		theCaster->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, baseCost);

		if (MAINT::CONFIG::DoSilenceFX) {
			spdlog::info("Silencing SpellFX");
			for (auto const& eff : MAINT::SilenceSpellFX(maintSpell))
				MAINT::UpdatePCHook::PushFXRestore(eff);
		}

		spdlog::info("\tAdding Constant Effect with Maintain Cost of {}", magCost);
		if (IsBoundWeaponSpell(baseSpell)) {
			auto* equipMgr = RE::ActorEquipManager::GetSingleton();
			// Check each hand and replace the spell with maintained one if present
			if (theCaster->GetEquippedObject(true) == baseSpell->As<RE::TESForm>()) {
				equipMgr->EquipSpell(theCaster, maintSpell, LeftHandSlot());
			} else if (theCaster->GetEquippedObject(false) == baseSpell->As<RE::TESForm>()) {
				equipMgr->EquipSpell(theCaster, maintSpell, RightHandSlot());
			}
		} else {
			theCaster->AddSpell(maintSpell);
		}

		theCaster->AddSpell(debuffSpell);
		MAINT::CACHE::SpellToMaintainedSpell.insert(baseSpell, { maintSpell, debuffSpell });

		MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->AddForm(baseSpell);
		RE::DebugNotification(std::format("Maintaining {} for {} Magicka.", baseSpell->GetName(), static_cast<uint32_t>(magCost)).c_str());
	}

	static void LoadSavegameMapping(const std::string& identifier)
	{
		spdlog::info("LoadSavegameMapping({})", identifier);

		constexpr auto getPluginNameWithLocalID = [](const std::string& part) -> std::pair<std::string, RE::FormID> {
			const std::size_t tildePos = part.find("~");
			if (tildePos == std::string::npos)
				return { std::string(), 0x0 };

			std::string pluginName = part.substr(0, tildePos);
			auto localFormID = lexical_cast_hex_to_formid(part.substr(tildePos + 1));
			return { pluginName, localFormID };
		};
		constexpr auto getSpellIDWithDebuffID = [](const std::string& part) -> std::pair<RE::FormID, RE::FormID> {
			const std::size_t tildePos = part.find("~");
			if (tildePos == std::string::npos)
				return { 0x0, 0x0 };
			try {
				auto spellFormID = lexical_cast_hex_to_formid(part.substr(0, tildePos));
				auto debuffFormID = lexical_cast_hex_to_formid(part.substr(tildePos + 1));
				return { spellFormID, debuffFormID };
			} catch (...) {
				return { 0x0, 0x0 };
			}
		};

		const auto& dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			spdlog::error("\tFailed to fetch TESDataHandler!");
			return;
		}

		const auto* spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		if (!spellFactory) {
			spdlog::error("\tFailed to fetch IFormFactory: SPEL!");
			return;
		}

		const auto subSection = std::format("MAP:{}", identifier);
		const auto* ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::MAP_FILE);
		for (const auto& [k, v] : ini->GetAllKeyValuePairs(subSection)) {
			const auto& [plugin, formid] = getPluginNameWithLocalID(k);
			const auto& [maintSpellFormID, debuffSpellFormID] = getSpellIDWithDebuffID(v);

			const auto& baseSpell = plugin != "VIRTUAL" ? dataHandler->LookupForm<RE::SpellItem>(formid, plugin) : RE::TESForm::LookupByID<RE::SpellItem>(formid);
			if (!baseSpell)
			{
				spdlog::error("Base Spell {}~{:08X} not found", plugin, formid);
				continue;
			}

			const auto& infSpell = CreateMaintainSpell(const_cast<RE::SpellItem*>(baseSpell));
			if (!infSpell) {
				spdlog::error("\tFailed to create Maintained Spell: {}", baseSpell->GetName());
				return;
			}
			if (maintSpellFormID != 0x0)
				infSpell->SetFormID(maintSpellFormID, false);

			const auto& debuffSpell = CreateDebuffSpell(const_cast<RE::SpellItem*>(baseSpell), 0.0f);
			if (!debuffSpell) {
				spdlog::error("\tFailed to create Maintained Spell: {}", baseSpell->GetName());
				return;
			}
			if (debuffSpellFormID != 0x0)
				debuffSpell->SetFormID(debuffSpellFormID, false);

			MAINT::CACHE::SpellToMaintainedSpell.insert(const_cast<RE::SpellItem*>(baseSpell), { const_cast<RE::SpellItem*>(infSpell), const_cast<RE::SpellItem*>(debuffSpell) });
		}
	}

	static void StoreSavegameMapping(const std::string& identifier)
	{
		spdlog::info("StoreSavegameMapping({})", identifier);
		static auto* ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::MAP_FILE);

		const auto subSection = std::format("MAP:{}", identifier);
		ini->DeleteSection(subSection);

		for (const auto& [baseSpell, maintData] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = maintData;

			const auto* file = baseSpell->GetFile(0);
			const auto fileName = file ? file->GetFilename() : "VIRTUAL"sv;
			const auto keyString = std::format("{}~0x{:08X}", fileName, file ? baseSpell->GetLocalFormID() : baseSpell->GetFormID());
			const auto rhs = std::format("0x{:08X}~0x{:08X}", maintSpell->GetFormID(), debuffSpell->GetFormID());

			ini->SetValue(subSection, keyString, rhs, std::format("# {}", baseSpell->GetName()));
		}
		ini->Save();
	}

	void AwardPlayerExperience(RE::PlayerCharacter* const& player)
	{
		if (MAINT::CONFIG::MaintainedExpMultiplier <= 0.0f)
			return;
		for (const auto& [baseSpell, _] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const float baseCost = baseSpell->CalculateMagickaCost(nullptr);
			player->AddSkillExperience(baseSpell->GetAssociatedSkill(), baseCost * MAINT::CONFIG::MaintainedExpMultiplier);
		}
	}

	void CheckUpkeepValidity(RE::Actor* const& theActor)
	{
		if (MAINT::CACHE::SpellToMaintainedSpell.empty())
			return;

		const float av = theActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
		if (av >= 0)
			return;

		static const auto mindCrush = MAINT::FORMS::GetSingleton().SpelMindCrush;

		if (theActor->GetRace() == WerewolfBeastRace() || theActor->GetRace() == VampireBeastRace() || theActor->AsMagicTarget()->HasMagicEffect(mindCrush->effects[0]->baseEffect))
			return;

		logger::debug("Triggered Mind Crush");

		const auto& map = MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap();
		const float totalMagDrain = std::accumulate(map.begin(), map.end(), 0.0f,
			[](float current, const std::pair<RE::SpellItem* const, MAINT::CACHE::MaintainedSpell>& entry) {
				const auto& [maintSpell, debuffSpell] = entry.second;
				return current + debuffSpell->effects.front()->GetMagnitude();
			});

		for (const auto& [baseSpell, maintainedSpellPair] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			if (IsBoundWeaponSpell(baseSpell)) {
				const auto& [maintSpell, debuffSpell] = maintainedSpellPair;
				MAINT::CACHE::DeferredDispelList.emplace(maintSpell, baseSpell);
			}
		}

		theActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)
			->CastSpellImmediate(mindCrush, false, theActor, 1.0, true, totalMagDrain, nullptr);
	}

	void ForceMaintainedSpellUpdate(RE::Actor* const& theActor)
	{
		if (MAINT::CACHE::SpellToMaintainedSpell.empty())
			return;

		// Basic timing stats (lightweight)
		constexpr uint32_t _AVG_WINDOW{ 100 };
		static double _runTime{ 0.0 };
		static uint32_t _runCount{ 0 };
		const auto start = std::chrono::high_resolution_clock::now();

		// Deferred dispels (bound weapon hand state changes)
		if (!MAINT::CACHE::DeferredDispelList.empty()) {
			auto* equipMgr = RE::ActorEquipManager::GetSingleton();
			for (auto it = MAINT::CACHE::DeferredDispelList.begin(); it != MAINT::CACHE::DeferredDispelList.end();) {
				const auto& [maintSpell, baseSpell] = *it;
				bool doErase = false;

				// 0 = left, 1 = right
				if (theActor->GetActorRuntimeData().selectedSpells[0] == maintSpell) {
					logger::debug("Deferred restore (left hand): {}", maintSpell->GetName());
					equipMgr->EquipSpell(theActor, baseSpell, LeftHandSlot());
					doErase = true;
				}
				if (theActor->GetActorRuntimeData().selectedSpells[1] == maintSpell) {
					logger::debug("Deferred restore (right hand): {}", maintSpell->GetName());
					equipMgr->EquipSpell(theActor, baseSpell, RightHandSlot());
					doErase = true;
				}

				if (doErase) {
					it = MAINT::CACHE::DeferredDispelList.erase(it);
				} else {
					++it;
				}
			}
		}

		// Build Spell->ActiveEffect set (reuse container to avoid churn)
		static std::map<RE::SpellItem*, std::unordered_multiset<RE::ActiveEffect*>> SpellToActiveEffects;
		SpellToActiveEffects.clear();

		static auto const& mmDebufEffect = MAINT::FORMS::GetSingleton().SpelMagickaDebuffTemplate->effects.front();

		const auto& effList = theActor->AsMagicTarget()->GetActiveEffectList();
		for (const auto& e : *effList) {
			if (auto* asSpl = e->spell ? e->spell->As<RE::SpellItem>() : nullptr; asSpl && e->effect->baseEffect != mmDebufEffect->baseEffect) {
				const bool hasKywd = asSpl->HasKeyword(MAINT::FORMS::GetSingleton().KywdMaintainedSpell);
				const bool isBaseSpell = MAINT::CACHE::SpellToMaintainedSpell.containsKey(asSpl);
				if (isBaseSpell) {
					const auto& pair = MAINT::CACHE::SpellToMaintainedSpell.getValue(asSpl);
					auto* mSpl = pair.first;  // maintained (infinite) spell
					SpellToActiveEffects[mSpl].insert(e);
				} else if (hasKywd) {
					e->elapsedSeconds = 0.0f;
					SpellToActiveEffects[asSpl].insert(e);
				}
			}
		}

		std::vector<std::pair<RE::SpellItem*, MAINT::CACHE::MaintainedSpell>> toRemove;
		toRemove.reserve(MAINT::CACHE::SpellToMaintainedSpell.size());

		// Validate each maintained spell
		for (const auto& [baseSpell, maintainedSpellPair] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = maintainedSpellPair;

			// Bound-weapon specific validation
			if (IsBoundWeaponSpell(baseSpell)) {
				const auto* right = theActor->GetEquippedObject(false);
				const auto* left = theActor->GetEquippedObject(true);

				bool found = false;
				if (right && right->IsWeapon()) {
					const auto* weap = right->As<RE::TESObjectWEAP>();
					for (const auto& eff : baseSpell->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto* rightSpell = theActor->GetActorRuntimeData().selectedSpells[1];
					if (rightSpell == maintSpell && theActor->HasSpell(debuffSpell))
						continue;
				}

				if (left && left->IsWeapon()) {
					// BUGFIX: use 'left' here (was 'right' previously)
					const auto* weap = left->As<RE::TESObjectWEAP>();
					for (const auto& eff : baseSpell->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto* leftSpell = theActor->GetActorRuntimeData().selectedSpells[0];
					if (leftSpell == maintSpell && theActor->HasSpell(debuffSpell))
						continue;
				}
			}

			const auto itList = SpellToActiveEffects.find(maintSpell);
			if (itList == SpellToActiveEffects.end()) {
				spdlog::debug("{} not found on Actor", maintSpell->GetName());
				toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
				continue;
			}

			auto* mSpl = itList->first;
			auto& effSet = itList->second;

			// Count mismatch checks
			if (mSpl->effects.size() < effSet.size()) {
				spdlog::trace("{} EFF count mismatch: Spell has LESS", mSpl->GetName());
				toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
				continue;
			} else if (mSpl->effects.size() > effSet.size()) {
				spdlog::trace("{} EFF count mismatch: Spell has MORE", mSpl->GetName());
				// If more effects on spell definition than present on actor, ensure exclusives and sources match
				const auto getUniques = [](const RE::BSTArray<RE::Effect*>& arr) {
					std::set<RE::TESForm*> uniq;
					std::vector<RE::Effect*> out;
					out.reserve(arr.size());
					for (const auto& item : arr) {
						auto* assoc = item->baseEffect->data.associatedForm;
						if (assoc && uniq.insert(assoc).second)
							out.push_back(item);
					}
					return out;
				};

				const auto uniqueList = getUniques(mSpl->effects);

				const auto hasDifferentSource = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->spell->As<RE::SpellItem>() != mSpl;
				});

				if (hasDifferentSource != effSet.end()) {
					spdlog::debug("\t{} source mismatch! Found at least one: {} (0x{:08X})",
						mSpl->GetName(), (*hasDifferentSource)->spell->GetName(), (*hasDifferentSource)->spell->GetFormID());
					toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
					continue;
				}
				if (!uniqueList.empty() && uniqueList.size() > effSet.size()) {
					spdlog::debug("\tExclusives are missing");
					toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
					continue;
				}
			} else {
				// Duration sanity (expect effectively infinite)
				constexpr uint32_t HUGE_DUR = 60 * 60 * 24 * 356;
				const auto hasWrongDuration = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->duration > 0.0 && static_cast<uint32_t>(e->duration - e->elapsedSeconds) < HUGE_DUR;
				});
				if (hasWrongDuration != effSet.end()) {
					spdlog::debug("EFF duration does not match");
					toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
					continue;
				}
			}

			// Ensure at least one is active
			const auto hasActives = std::find_if(effSet.begin(), effSet.end(), [](RE::ActiveEffect* e) {
				return !e->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled);
			});
			if (hasActives == effSet.end()) {
				spdlog::debug("{} active count is zero", maintSpell->GetName());
				toRemove.emplace_back(baseSpell, std::make_pair(maintSpell, debuffSpell));
			}
		}

		// Cleanup removals
		if (!toRemove.empty()) {
			for (const auto& [baseSpell, maintSpellPair] : toRemove) {
				const auto& [maintSpell, debuffSpell] = maintSpellPair;
				spdlog::info("Dispelling missing/invalid {} (0x{:08X})", maintSpell->GetName(), maintSpell->GetFormID());

				if (IsBoundWeaponSpell(maintSpell) && !MAINT::CACHE::DeferredDispelList.contains({ maintSpell, baseSpell })) {
					logger::debug("Deferring cleanup of {}", maintSpell->GetName());
					MAINT::CACHE::DeferredDispelList.emplace(maintSpell, baseSpell);
				}
				if (theActor->HasSpell(debuffSpell)) {
					theActor->RemoveSpell(maintSpell);
					theActor->RemoveSpell(debuffSpell);
					RE::DebugNotification(std::format("{} is no longer being maintained.", baseSpell->GetName()).c_str());
				}
				MAINT::CACHE::SpellToMaintainedSpell.eraseKey(baseSpell);
			}

			MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->ClearData();
			for (const auto& [spl, _] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap())
				MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->AddForm(spl);
		}

		// Timing stats
		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> duration = end - start;
		_runTime += duration.count();
		_runCount += 1;
		if (_runCount == _AVG_WINDOW) {
			const auto avgMs = (_runTime / _runCount) * 1000.0;
			spdlog::info("ForceMaintainedSpellUpdate() avg time: {}ms", avgMs);
			_runTime = 0.0;
			_runCount = 0;
		}
	}
}  // namespace MAINT

// -- Event hook ---------------------------------------------------------------

class SpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent>
{
public:
	RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
	{
		if (!a_event || !a_event->object)
			return RE::BSEventNotifyControl::kContinue;

		auto* theCaster = a_event->object->As<RE::Actor>();
		if (theCaster != RE::PlayerCharacter::GetSingleton())
			return RE::BSEventNotifyControl::kContinue;

		if (static_cast<short>(MAINT::FORMS::GetSingleton().GlobMaintainModeEnabled->value) == 0)
			return RE::BSEventNotifyControl::kContinue;

		if (auto* theSpell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell)) {
			MAINT::MaintainSpell(theSpell, theCaster);
			MAINT::UpdatePCHook::ResetEffCheckTimer();
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	static SpellCastEventHandler& GetSingleton()
	{
		static SpellCastEventHandler singleton;
		return singleton;
	}
	static void Install()
	{
		auto& eventProcessor = SpellCastEventHandler::GetSingleton();
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSpellCastEvent>(&eventProcessor);
	}
};

// -- Config / lifecycle -------------------------------------------------------

static void ReadConfiguration()
{
	spdlog::info("Maintained Map @ {}", MAINT::CONFIG::MAP_FILE);
	spdlog::info("Maintained Config @ {}", MAINT::CONFIG::CONFIG_FILE);

	static auto* ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::CONFIG_FILE);

	const std::map<std::string, spdlog::level::level_enum> logMap = {
		{ "off", spdlog::level::off },
		{ "info", spdlog::level::info },
		{ "debug", spdlog::level::debug },
	};

	if (!ini->HasKey("CONFIG", "LogLevel"))
		ini->SetValue("CONFIG", "LogLevel", "info", "# Options: off, info, debug");

	const auto confLogLevel = ini->GetValue("CONFIG", "LogLevel");
	spdlog::info("Set Log Level to {}", confLogLevel);
	if (const auto it = logMap.find(confLogLevel); it != logMap.end())
		spdlog::set_level(it->second);
	else
		spdlog::set_level(spdlog::level::debug);

	if (!ini->HasKey("CONFIG", "SilencePersistentSpellFX")) {
		ini->SetBoolValue("CONFIG", "SilencePersistentSpellFX", false,
			"# If true, will disable persistent spell visuals on maintained spells. This includes flesh spell FX, the aura of Cloak spells, pretty much everything else.");
	}
	MAINT::CONFIG::DoSilenceFX = ini->GetBoolValue("CONFIG", "SilencePersistentSpellFX");
	spdlog::info("FX Will {} silenced", MAINT::CONFIG::DoSilenceFX ? "be" : "not be");

	if (!ini->HasKey("CONFIG", "CostNeutralDuration")) {
		ini->SetLongValue("CONFIG", "CostNeutralDuration", 60,
			"# At this BASE spell duration, maintenance cost will be equal to its casting cost. Shorter spells will be more expensive, longer will be cheaper.\n"
			"# Reduce to make maintenance cheaper across the board.\n"
			"# Set to 0 to disable all cost scaling and only use the spell's casting cost.");
	}
	MAINT::CONFIG::CostBaseDuration = ini->GetLongValue("CONFIG", "CostNeutralDuration");
	spdlog::info("CostNeutralDuration is {}", MAINT::CONFIG::CostBaseDuration);

	if (!ini->HasKey("CONFIG", "AllowBoundWeapons")) {
		ini->SetBoolValue("CONFIG", "AllowBoundWeapons", true,
			"# Whether to allow maintained bound weapons.\n"
			"# Sometimes a little glitchy. Only works with proper bound weapon spells, not script-based ones for bound shields or armor.\n"
			"# Must be re-summoned every time after unsheathing weapons (this is instant and free).\n"
			"# Maintenance will break if the bound weapon or bound weapon spell is removed from your hand.");
	}
	MAINT::CONFIG::AllowBoundWeapons = ini->GetBoolValue("CONFIG", "AllowBoundWeapons");
	spdlog::info("AllowBoundWeapons is {}", MAINT::CONFIG::AllowBoundWeapons);

	if (!ini->HasKey("CONFIG", "CostReductionExponent")) {
		ini->SetDoubleValue("CONFIG", "CostReductionExponent", 0.0,
			"# Determines the impact of spell durations on their maintenance cost.\n"
			"# Basically, final cost will be multiplied by (CostNeutralDuration / Effective Spell Duration)^CostReductionExponent\n"
			"# Leave at 0.0 to just use basic scaling (see CostNeutralDuration)");
	}
	MAINT::CONFIG::CostReductionExponent = static_cast<float>(ini->GetDoubleValue("CONFIG", "CostReductionExponent"));
	spdlog::info("CostReductionExponent is {}", MAINT::CONFIG::CostReductionExponent);

	if (!ini->HasKey("CONFIG", "MaintainedExpMultiplier")) {
		ini->SetDoubleValue("CONFIG", "MaintainedExpMultiplier", 1.0,
			"# Every 300 seconds, receive experience for each maintained spell equal to one spell cast, multiplied by this value.\n"
			"# 0.5 is half, 2.0 is double, 0 to disable passive experience entirely.");
	}
	MAINT::CONFIG::MaintainedExpMultiplier = static_cast<float>(ini->GetDoubleValue("CONFIG", "MaintainedExpMultiplier"));
	spdlog::info("MaintainedExpMultiplier is {}", MAINT::CONFIG::MaintainedExpMultiplier);

	ini->Save();
}

void OnInit(SKSE::MessagingInterface::Message* const a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		ReadConfiguration();
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		if (a_msg->dataLen > 0) {
			auto* charData = static_cast<char*>(a_msg->data);
			std::string saveFile(charData, a_msg->dataLen);
			spdlog::info("Load : {}", saveFile);
			MAINT::Purge();
			MAINT::FORMS::GetSingleton().LoadOffset(MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::MAP_FILE), saveFile);
			MAINT::LoadSavegameMapping(saveFile);
		}
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
		MAINT::BuildActiveSpellsCache();
		break;
	case SKSE::MessagingInterface::kSaveGame:
		if (a_msg->dataLen > 0) {
			auto* charData = static_cast<char*>(a_msg->data);
			std::string saveFile(charData, a_msg->dataLen);
			std::string saveFileWithExt = std::format("{}.ess", saveFile);
			spdlog::info("Save : {}", saveFileWithExt);
			MAINT::StoreSavegameMapping(saveFileWithExt);
		}
		break;
	default:
		break;
	}
}

bool Load()
{
	SpellCastEventHandler::Install();
	MAINT::UpdatePCHook::Install();
	return true;
}
