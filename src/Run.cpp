#include "Run.hpp"
#define INGAME_DEBUG

namespace MAINT
{
	static std::vector<RE::Effect*> SilenceSpellFX(RE::SpellItem* const& theSpell)
	{
		std::vector<RE::Effect*> ret;
		for (auto const& eff : theSpell->effects) {
			bool const& persists = eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			if (persists) {
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
					logger::info("{} fx will not be silenced", eff->baseEffect->GetName());
					break;
				default:
					eff->baseEffect->data.flags.reset(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
					ret.emplace_back(eff);
				}
			}
		}
		return ret;
	}

	static bool IsMaintainable(RE::SpellItem* const& theSpell, RE::Actor* const& theCaster)
	{
		if (theSpell->As<RE::ScrollItem>()) {
			logger::info("Spell is Scroll");
			return false;
		}
		if (theSpell->As<RE::EnchantmentItem>()) {
			logger::info("Spell is Enchantment");
			return false;
		}
		if (theSpell->effects.empty()) {
			logger::info("Spell has no effects");
			return false;
		}
		if (theSpell->data.castingType != RE::MagicSystem::CastingType::kFireAndForget) {
			logger::info("Spell is not FF");
			return false;
		}
		if (theSpell->effects.front()->GetDuration() <= 5.0) {
			logger::info("Spell has duration of 5 seconds or less");
			return false;
		}
		if (theSpell->CalculateMagickaCost(theCaster) <= 5.0 && theSpell->CalculateMagickaCost(nullptr) <= 5.0) {
			logger::info("Spell has cost of 5 or less");
			return false;
		}

		if (theSpell->HasKeyword(MAINT::FORMS::GetSingleton().KywdMaintainedSpell)) {
			logger::info("Spell has Maintained keyword");
			return false;
		}
		if (theSpell->HasKeyword(MAINT::FORMS::GetSingleton().KywdExcludeFromSystem)) {
			logger::info("Spell has exclusion keyword");
			return false;
		}
		if (theSpell->HasKeywordString("_m3HealerDummySpell")) {
			logger::info("Spell has Allylink keyword");
			return false;
		}
		const auto& archetype = theSpell->effects[0]->baseEffect->GetArchetype();
		if (theSpell->data.delivery != RE::MagicSystem::Delivery::kSelf) {
			if (archetype == RE::EffectSetting::Archetype::kSummonCreature)
				return true;
			logger::info("Spell does not target self, and is not summon");
			return false;
		}
		if (archetype == RE::EffectSetting::Archetype::kBoundWeapon) {
			if (MAINT::CONFIG::AllowBoundWeapons == false) {
				logger::info("Spell is bound weapon");
				return false;
			} else {
				const auto& leftHand = theCaster->GetEquippedObject(true);
				const auto& leftHandSpell = leftHand != nullptr ? leftHand->As<RE::SpellItem>() : nullptr;
				const auto& rightHand = theCaster->GetEquippedObject(false);
				const auto& rightHandSpell = rightHand != nullptr ? rightHand->As<RE::SpellItem>() : nullptr;
				if (
					(leftHandSpell != nullptr && leftHandSpell->effects[0]->baseEffect->data.associatedForm == theSpell->effects[0]->baseEffect->data.associatedForm) &&
					(rightHandSpell != nullptr && rightHandSpell->effects[0]->baseEffect->data.associatedForm == theSpell->effects[0]->baseEffect->data.associatedForm)) {
					logger::info("Player already has this bound weapon");
					RE::DebugNotification(std::format("Only one instance of {} can be maintained.", theSpell->GetName()).c_str());
					return false;
				}
			}
		}
		return true;
	}

	static RE::SpellItem* CreateMaintainSpell(RE::SpellItem* const& theSpell)
	{
		static auto const& spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		const auto& fileString = theSpell->GetFile(0) ? theSpell->GetFile(0)->GetFilename() : "VIRTUAL";
		logger::info("Maintainify({}, 0x{:08X}~{})", theSpell->GetName(), theSpell->GetLocalFormID(), fileString);

		auto infiniteSpell = spellFactory->Create();
		infiniteSpell->SetFormID(MAINT::FORMS::GetSingleton().NextFormID(), false);

		infiniteSpell->fullName = std::format("Maintained {}", theSpell->GetFullName());

		infiniteSpell->data = theSpell->data;
		//RE::SpellItem::Data{ theSpell->data };
		infiniteSpell->avEffectSetting = theSpell->avEffectSetting;
		infiniteSpell->boundData = theSpell->boundData;
		infiniteSpell->descriptionText = theSpell->descriptionText;

		infiniteSpell->equipSlot = theSpell->equipSlot;
		//MAINT::FORMS::GetSingleton().EquipSlotVoice;

		infiniteSpell->data.spellType = RE::MagicSystem::SpellType::kAbility;
		infiniteSpell->SetDelivery(RE::MagicSystem::Delivery::kSelf);
		infiniteSpell->SetCastingType(RE::MagicSystem::CastingType::kConstantEffect);

		for (uint32_t i = 0; i < theSpell->numKeywords; ++i)
			infiniteSpell->AddKeyword(theSpell->GetKeywordAt(i).value());

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
		const auto& fileString = theSpell->GetFile(0) ? theSpell->GetFile(0)->GetFilename() : "VIRTUAL";
		logger::info("Debuffify({}, 0x{:08X}~{})", theSpell->GetName(), theSpell->GetLocalFormID(), fileString);

		auto debuffSpell = spellFactory->Create();
		debuffSpell->SetFormID(MAINT::FORMS::GetSingleton().NextFormID(), false);

		debuffSpell->fullName = std::format("Maintained {}", theSpell->GetFullName());

		debuffSpell->data = RE::SpellItem::Data{ debuffSpellTemplate->data };

		debuffSpell->avEffectSetting = debuffSpellTemplate->avEffectSetting;
		debuffSpell->boundData = debuffSpellTemplate->boundData;
		//debuffSpell->descriptionText = theSpell->descriptionText;

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
		logger::info("BuildActiveSpellsCache()");
		static const auto& player = RE::PlayerCharacter::GetSingleton();
		if (player == nullptr) {
			logger::error("\tPlayer is NULL");
			return;
		}
		for (const auto& playerSpell : player->GetActorRuntimeData().addedSpells) {
			if (!MAINT::CACHE::SpellToMaintainedSpell.containsKey(playerSpell))
				continue;
			MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->AddForm(playerSpell);
		}

		auto const& activeEffects = player->AsMagicTarget()->GetActiveEffectList();
		for (auto const& [baseSpell, maintData] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			auto const& [maintSpell, debuffSpell] = maintData;
			for (auto const& aeff : *activeEffects) {
				if (aeff->spell == debuffSpell && aeff->GetCasterActor().get() == player && aeff->effect == debuffSpell->effects.front()) {
					logger::info("Restoring {} magnitude to {}", debuffSpell->GetName(), abs(aeff->GetMagnitude()));
					debuffSpell->effects.front()->effectItem.magnitude = abs(aeff->GetMagnitude());
					break;
				}
			}
		}
	}

	static void Purge()
	{
		logger::info("Purge()");
		for (const auto& [k, v] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = v;
			maintSpell->SetDelete(true);
			debuffSpell->SetDelete(true);
		}
		MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->ClearData();
		MAINT::CACHE::SpellToMaintainedSpell.clear();
	}

	static void LoadSavegameMapping(const std::string& identifier)
	{
		logger::info("LoadSavegameMapping({})", identifier);
		constexpr auto getPluginNameWithLocalID = [](const std::string& part) -> std::pair<std::string, RE::FormID> {
			std::size_t tildePos = part.find("~");

			if (tildePos == std::string::npos)
				return { std::string(), 0x0 };

			std::string pluginName = part.substr(0, tildePos);
			auto localFormID = lexical_cast_hex_to_formid(part.substr(tildePos + 1));
			return { pluginName, localFormID };
		};
		constexpr auto getSpellIDWithDebuffID = [](const std::string& part) -> std::pair<RE::FormID, RE::FormID> {
			std::size_t tildePos = part.find("~");

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
			logger::error("\tFailed to fetch TESDataHandler!");
			return;
		}

		const auto& spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		if (!spellFactory) {
			logger::error("\tFailed to fetch IFormFactory: SPEL!");
			return;
		}

		const auto subSection = std::format("MAP:{}", identifier);
		const auto ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::MAP_FILE);
		for (const auto& [k, v] : ini->GetAllKeyValuePairs(subSection)) {
			const auto& [plugin, formid] = getPluginNameWithLocalID(k);
			const auto& [maintSpellFormID, debuffSpellFormID] = getSpellIDWithDebuffID(v);

			const auto& baseSpell = dataHandler->LookupForm<RE::SpellItem>(formid, plugin);
			if (!baseSpell)
				continue;

			const auto& infSpell = CreateMaintainSpell(baseSpell);
			if (!infSpell) {
				logger::error("\tFailed to create Maintained Spell: {}", baseSpell->GetName());
				return;
			}
			if (maintSpellFormID != 0x0)
				infSpell->SetFormID(maintSpellFormID, false);

			const auto& debuffSpell = CreateDebuffSpell(baseSpell, 0.0f);
			if (!debuffSpell) {
				logger::error("\tFailed to create Maintained Spell: {}", baseSpell->GetName());
				return;
			}
			if (debuffSpellFormID != 0x0)
				debuffSpell->SetFormID(debuffSpellFormID, false);
			MAINT::CACHE::SpellToMaintainedSpell.insert(baseSpell, { infSpell, debuffSpell });
		}
	}

	static auto CalculateUpkeepCost(RE::SpellItem* const& baseSpell, RE::Actor* const& theCaster)
	{
		static auto const& NEUTRAL_DURATION = static_cast<float>(MAINT::CONFIG::CostBaseDuration);
		static auto const& EXPONENT = MAINT::CONFIG::CostReductionExponent;

		logger::info("CalculateUpkeepCost()");
		const auto& baseCost = baseSpell->CalculateMagickaCost(theCaster);

		if (NEUTRAL_DURATION == 0.0F) {
			return baseCost;
		}

		const auto& baseDuration = max(static_cast<uint32_t>(1u), baseSpell->effects.front()->GetDuration());
		auto mult = baseDuration < NEUTRAL_DURATION ? powf(NEUTRAL_DURATION / baseDuration, 2.0) : powf(sqrt(NEUTRAL_DURATION / baseDuration), EXPONENT);

		float baseDur = static_cast<float>(baseSpell->effects.front()->GetDuration());
		float finalDur = baseDur;
		for (const auto& aeff : *theCaster->AsMagicTarget()->GetActiveEffectList()) {
			if (aeff->spell == baseSpell && aeff->GetCasterActor().get() == theCaster && aeff->effect == baseSpell->effects.front()) {
				finalDur = aeff->duration;
				const auto& durmult = sqrt(baseDur / finalDur);
				mult *= durmult;

				logger::info("NeutralDur {} vs BaseDur {} vs RealDur {} => Cost Mult: {}x", NEUTRAL_DURATION, baseDur, aeff->duration, mult);
				break;
			}
		}

		if (finalDur > baseDur) {
			mult *= powf(NEUTRAL_DURATION / finalDur, EXPONENT);
			logger::info("Mult after CostReductionExponent: {}x", mult);
		}

		return roundf(baseCost * mult);
	}

	static void MaintainSpell(RE::SpellItem* const& baseSpell, RE::Actor* const& theCaster)
	{
		logger::info("MaintainSpell({}, 0x{:08X})", baseSpell->GetName(), baseSpell->GetFormID());

		if (!IsMaintainable(baseSpell, theCaster)) {
			RE::DebugNotification(std::format("Cannot maintain {}.", baseSpell->GetName()).c_str());
			return;
		}

		const auto& baseCost = baseSpell->CalculateMagickaCost(theCaster);
		auto magCost = CalculateUpkeepCost(baseSpell, theCaster);

		if (magCost > theCaster->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) + baseCost) {
			RE::DebugNotification(std::format("Need {} Magicka to maintain {}.", static_cast<uint32_t>(magCost), baseSpell->GetName()).c_str());
			return;
		}

		if (MAINT::CACHE::SpellToMaintainedSpell.containsKey(baseSpell)) {
			logger::info("\tActor already has constant version of {}.", baseSpell->GetName());
			return;
		}

		const auto& maintSpell = CreateMaintainSpell(baseSpell);
		const auto& debuffSpell = CreateDebuffSpell(baseSpell, magCost);

		logger::info("\tRemoving Base Effect of {}", baseSpell->GetName());
		auto handle = theCaster->GetHandle();
		theCaster->AsMagicTarget()->DispelEffect(baseSpell, handle);
		theCaster->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, baseCost);

		if (MAINT::CONFIG::DoSilenceFX) {
			logger::info("Silencing SpellFX");
			for (auto const& eff : MAINT::SilenceSpellFX(maintSpell))
				MAINT::UpdatePCHook::PushFXRestore(eff);
		}

		logger::info("\tAdding Constant Effect with Maintain Cost of {}", magCost);
		if (baseSpell->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kBoundWeapon)) {
			auto equipMgr = RE::ActorEquipManager::GetSingleton();
			if (theCaster->GetEquippedObject(true) == baseSpell->As<RE::TESForm>()) {
				//RE::DebugNotification(std::format("Unequipping {} from left hand.", baseSpell->GetName()).c_str());
				equipMgr->EquipSpell(theCaster, maintSpell, RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F43));
			} else if (theCaster->GetEquippedObject(false) == baseSpell->As<RE::TESForm>()) {
				//RE::DebugNotification(std::format("Unequipping {} from right hand.", baseSpell->GetName()).c_str());
				equipMgr->EquipSpell(theCaster, maintSpell, RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F42));
			}
		} else {
			theCaster->AddSpell(maintSpell);
		}
		theCaster->AddSpell(debuffSpell);
		MAINT::CACHE::SpellToMaintainedSpell.insert(baseSpell, { maintSpell, debuffSpell });

		MAINT::FORMS::GetSingleton().FlstMaintainedSpellToggle->AddForm(baseSpell);
		RE::DebugNotification(std::format("Maintaining {} for {} Magicka.", baseSpell->GetName(), static_cast<uint32_t>(magCost)).c_str());
	}

	static void StoreSavegameMapping(const std::string& identifier)
	{
		logger::info("StoreSavegameMapping({})", identifier);
		static auto ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::MAP_FILE);
		const auto subSection = std::format("MAP:{}", identifier);
		ini->DeleteSection(subSection);
		for (const auto& [baseSpell, maintData] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = maintData;
			auto keyString = std::format("{}~0x{:08X}", baseSpell->GetFile(0)->GetFilename(), baseSpell->GetLocalFormID());
			auto rightHandSide = std::format("0x{:08X}~0x{:08X}", maintSpell->GetFormID(), debuffSpell->GetFormID());
			ini->SetValue(subSection,
				keyString,
				rightHandSide,
				std::format("# {}", baseSpell->GetName()));
		}
		ini->Save();
	}

	void AwardPlayerExperience(RE::PlayerCharacter* const& player)
	{
		for (const auto& [baseSpell, _] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& baseCost = baseSpell->CalculateMagickaCost(nullptr);
			player->AddSkillExperience(baseSpell->GetAssociatedSkill(), baseCost);
		}
	}

	void CheckUpkeepValidity(RE::Actor* const& theActor)
	{
		if (MAINT::CACHE::SpellToMaintainedSpell.empty()) {
			return;
		}

		const auto& av = theActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
		if (av >= 0) {
			return;
		}

		const auto& map = MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap();
		const auto totalMagDrain = std::accumulate(map.begin(), map.end(), 0.0f,
			[&](float current, const std::pair<RE::SpellItem*, MAINT::CACHE::MaintainedSpell>& maintSpell) {
				const auto& [baseSpell, maintSpellData] = maintSpell;
				const auto& [infSpell, debuffSpell] = maintSpellData;
				return current + debuffSpell->effects.front()->GetMagnitude();
			});

		const auto& mindCrush = MAINT::FORMS::GetSingleton().SpelMindCrush;
		theActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)->CastSpellImmediate(mindCrush, false, theActor, 1.0, true, totalMagDrain, nullptr);
	}

	void ForceMaintainedSpellUpdate(RE::Actor* const& theActor)
	{
		if (MAINT::CACHE::SpellToMaintainedSpell.empty())
			return;
		constexpr uint32_t _AVG_WINDOW{ 100 };
		static double _runTime{ 0.0 };
		static uint32_t _runCount{ 0 };
		auto start = std::chrono::high_resolution_clock::now();

		if (!MAINT::CACHE::DeferredDispelList.empty()) {
			const auto& equipMgr = RE::ActorEquipManager::GetSingleton();
			for (auto it = MAINT::CACHE::DeferredDispelList.begin(); it != MAINT::CACHE::DeferredDispelList.end();) {
				const auto& [ maintSpell, baseSpell ] = *it;
				bool doErase = false;
				if (theActor->GetActorRuntimeData().selectedSpells[0] == maintSpell) {
					equipMgr->EquipSpell(theActor, baseSpell, RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F43));
					doErase = true;
				}
				if (theActor->GetActorRuntimeData().selectedSpells[1] == maintSpell) {
					equipMgr->EquipSpell(theActor, baseSpell, RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F42));
					doErase = true;
				} 
				if(doErase) {
					MAINT::CACHE::DeferredDispelList.erase(it);
				} else {
					++it;
				}
			}
		}

		std::map<RE::SpellItem*, std::unordered_multiset<RE::ActiveEffect*>> SpellToActiveEffects;
		static auto const& mmDebufEffect = MAINT::FORMS::GetSingleton().SpelMagickaDebuffTemplate->effects.front();

		const auto& effList = theActor->AsMagicTarget()->GetActiveEffectList();
		for (const auto& e : *effList) {
			if (auto const& asSpl = e->spell->As<RE::SpellItem>(); asSpl != nullptr && e->effect->baseEffect != mmDebufEffect->baseEffect) {
				auto const& hasKywd = asSpl->HasKeyword(MAINT::FORMS::GetSingleton().KywdMaintainedSpell);
				auto const& isBaseSpell = MAINT::CACHE::SpellToMaintainedSpell.containsKey(asSpl);
				if (isBaseSpell) {
					auto const& [mSpl, _] = MAINT::CACHE::SpellToMaintainedSpell.getValue(asSpl);
					SpellToActiveEffects[mSpl].insert(e);
				} else if (hasKywd) {
					e->elapsedSeconds = 0.0f;
					SpellToActiveEffects[asSpl].insert(e);
				}
			}
		}

		std::vector<std::pair<RE::SpellItem*, MAINT::CACHE::MaintainedSpell>> toRemove;
		std::vector<RE::SpellItem*> toDefer;
		for (const auto& [baseSpell, maintainedSpellPair] : MAINT::CACHE::SpellToMaintainedSpell.GetForwardMap()) {
			const auto& [maintSpell, debuffSpell] = maintainedSpellPair;

			if (baseSpell->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kBoundWeapon)) {
				const auto& right = theActor->GetEquippedObject(false);
				const auto& left = theActor->GetEquippedObject(true);

				bool found = false;
				if (right && right->IsWeapon()) {
					const auto& weap = right->As<RE::TESObjectWEAP>();
					for (const auto& eff : baseSpell->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto rightSpell = theActor->GetActorRuntimeData().selectedSpells[1];
					if (rightSpell == maintSpell && theActor->HasSpell(debuffSpell))
						continue;
				}
				if (left && left->IsWeapon()) {
					const auto& weap = right->As<RE::TESObjectWEAP>();
					for (const auto& eff : baseSpell->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto leftSpell = theActor->GetActorRuntimeData().selectedSpells[0];
					if (leftSpell == maintSpell && theActor->HasSpell(debuffSpell))
						continue;
				}
			}

			auto const& mSplInList = SpellToActiveEffects.find(maintSpell);

			if (mSplInList == SpellToActiveEffects.end()) {
				logger::debug("{} not found on Actor", maintSpell->GetName());
				//RE::DebugNotification(std::format("{} not found on Actor", maintSpell->GetName()).c_str());
				toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
				continue;
			}

			auto const& mSpl = mSplInList->first;
			auto const& effSet = mSplInList->second;
			if (mSpl->effects.size() < effSet.size()) {
				logger::debug("{} EFF count mismatch: Spell has LESS", mSpl->GetName());
				//RE::DebugNotification(std::format("{} EFF count mismatch: Spell has LESS", mSpl->GetName()).c_str());
				logger::debug("\t{} has:", mSpl->GetName());
				//RE::DebugNotification(std::format("\t{} has:", mSpl->GetName()).c_str());
				short n = 1;
				for (auto const& te : mSpl->effects) {
					logger::debug("\t{}\t{}", n++, te->baseEffect->GetName());
					//RE::DebugNotification(std::format("\t{}\t{}", n++, te->baseEffect->GetName()).c_str());
				}
				logger::debug("\tEffectSet has:");
				//RE::DebugNotification("\tEffectSet has:");
				n = 1;
				for (auto const& te : effSet) {
					logger::debug("\t{}\t{}, Src: {}", n++, te->effect->baseEffect->GetName(), te->spell ? te->spell->GetName() : "NULL/UNK");
					//RE::DebugNotification(std::format("\t{}\t{}, Src: {}", n++, te->effect->baseEffect->GetName(), te->spell ? te->spell->GetName() : "NULL/UNK").c_str());
				}
				toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
				continue;
			} else if (mSpl->effects.size() > effSet.size()) {
				auto constexpr getUniques = [&](RE::BSTArray<RE::Effect*> arr) {
					std::set<RE::TESForm*> uniqueItemsSet;
					std::vector<RE::Effect*> uniqueItems;

					for (const auto& item : arr) {
						if (item->baseEffect->data.associatedForm != nullptr && uniqueItemsSet.insert(item->baseEffect->data.associatedForm).second) {  // .second is true if insertion took place
							uniqueItems.push_back(item);
						}
					}
					return uniqueItems;
				};
				logger::debug("{} EFF count mismatch: Spell has MORE", mSpl->GetName());
				//RE::DebugNotification(std::format("{} EFF count mismatch: Spell has MORE", mSpl->GetName()).c_str());
				logger::debug("\t{} has:", mSpl->GetName());
				//RE::DebugNotification(std::format("\t{} has:", mSpl->GetName()).c_str());

				short n = 1;
				for (auto const& te : mSpl->effects) {
					logger::debug("\t{}\t{} (0x{:08X})", n++, te->baseEffect->GetName(), te->baseEffect->GetFormID());
					//RE::DebugNotification(std::format("\t{}\t{} (0x{:08X})", n++, te->baseEffect->GetName(), te->baseEffect->GetFormID()).c_str());
				}

				auto const& uniqueList = getUniques(mSpl->effects);
				if (!uniqueList.empty()) {
					logger::debug("\tThe following have exclusivity:");
					n = 1;
					for (auto const& te : uniqueList) {
						logger::debug("\t{}\t{} (0x{:08X}) # Assoc: {}", n++, te->baseEffect->GetName(), te->baseEffect->GetFormID(), te->baseEffect->data.associatedForm->GetName());
						//RE::DebugNotification(std::format("\t{}\t{} (0x{:08X}) # Assoc: {}", n++, te->baseEffect->GetName(), te->baseEffect->GetFormID(), te->baseEffect->data.associatedForm->GetName()).c_str());
					}
				}
				logger::debug("\tEffectSet has:");
				n = 1;
				for (auto const& te : effSet) {
					logger::debug("\t{}\t{} (0x{}), Src: {}", n++, te->effect->baseEffect->GetName(), te->effect->baseEffect->GetFormID(), te->spell ? te->spell->GetName() : "NULL/UNK");
					//RE::DebugNotification(std::format("\t{}\t{} (0x{}), Src: {}", n++, te->effect->baseEffect->GetName(), te->effect->baseEffect->GetFormID(), te->spell ? te->spell->GetName() : "NULL/UNK").c_str());
				}

				auto const& hasDifferentSource = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->spell->As<RE::SpellItem>() != mSpl;
				});
				if (hasDifferentSource != effSet.end()) {
					logger::debug("\t{} source mismatch! Found at least one: {} (0x{:08X})", mSpl->GetName(), (*hasDifferentSource)->spell->GetName(), (*hasDifferentSource)->spell->GetFormID());
					//RE::DebugNotification(std::format("\t{} source mismatch! Found at least one: {} (0x{:08X})", mSpl->GetName(), (*hasDifferentSource)->spell->GetName(), (*hasDifferentSource)->spell->GetFormID()).c_str());
					toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
					continue;
				} else {
					logger::debug("\tAll active sources match");
					//RE::DebugNotification("\t\tAll active sources match");
					if (uniqueList.size() > effSet.size()) {
						logger::debug("\tBut exclusives are missing");
						//RE::DebugNotification("\tBut exclusives are missing");
						toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
						continue;
					}
				}
			} else {
				constexpr uint32_t HUGE_DUR = 60 * 60 * 24 * 356;
				auto const& hasWrongDuration = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->duration > 0.0 && static_cast<uint32_t>(e->duration - e->elapsedSeconds) < HUGE_DUR;
				});
				if (hasWrongDuration != effSet.end()) {
					logger::debug("EFF duration does not match");
					//RE::DebugNotification("EFF duration does not match");
					toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
					continue;
				}
			}

			auto const& allMyEffects = SpellToActiveEffects.at(maintSpell);
			auto const& hasActives = std::find_if(allMyEffects.begin(), allMyEffects.end(), [](RE::ActiveEffect* e) {
				return !e->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled);
			});
			if (hasActives == allMyEffects.end()) {
				logger::debug("{} active count is zero", maintSpell->GetName());
				//RE::DebugNotification(std::format("{} active count is zero", maintSpell->GetName()).c_str());
				toRemove.emplace_back(std::make_pair(baseSpell, std::make_pair(maintSpell, debuffSpell)));
			}
		}

		if (!toRemove.empty()) {
			for (const auto& [baseSpell, maintSpellPair] : toRemove) {
				const auto& [maintSpell, debuffSpell] = maintSpellPair;
				logger::info("Dispelling missing/invalid {} (0x{:08X})", maintSpell->GetName(), maintSpell->GetFormID());
				if (maintSpell->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kBoundWeapon) && !MAINT::CACHE::DeferredDispelList.contains({ maintSpell, baseSpell })) {
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

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> duration = end - start;
		_runTime += duration.count();
		_runCount++;
		if (_runCount == _AVG_WINDOW) {
			auto _avgMs = (_runTime / _runCount) * 1000;
			logger::info("ForceMaintainedSpellUpdate() avg time: {}ms", _avgMs);
			_runTime = 0.0;
			_runCount = 0;
		}
	}
}

class SpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent>
{
public:
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
	{
		if (a_event == nullptr || a_event->object == nullptr)
			return RE::BSEventNotifyControl::kContinue;

		const auto& theCaster = a_event->object->As<RE::Actor>();
		if (theCaster != RE::PlayerCharacter::GetSingleton())
			return RE::BSEventNotifyControl::kContinue;

		if (static_cast<short>(MAINT::FORMS::GetSingleton().GlobMaintainModeEnabled->value) == 0)
			return RE::BSEventNotifyControl::kContinue;

		if (const auto& theSpell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell)) {
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

static void ReadConfiguration()
{
	logger::info("Maintained Map @ {}", MAINT::CONFIG::MAP_FILE);
	logger::info("Maintained Config @ {}", MAINT::CONFIG::CONFIG_FILE);

	static auto const& ini = MAINT::CONFIG::ConfigBase::GetSingleton(MAINT::CONFIG::CONFIG_FILE);

	const std::map<std::string, spdlog::level::level_enum> logMap = {
		{ "off", spdlog::level::level_enum::off },
		{ "info", spdlog::level::level_enum::info },
		{ "debug", spdlog::level::level_enum::debug },
	};
	if (!ini->HasKey("CONFIG", "LogLevel")) {
		ini->SetValue("CONFIG", "LogLevel", "off", "# Options: off, info, debug");
	}

	auto const confLogLevel = ini->GetValue("CONFIG", "LogLevel");
	logger::info("Set Log Level to {}", confLogLevel);
	if (auto const& it = logMap.find(confLogLevel); it != logMap.end())
		spdlog::set_level(it->second);
	else
		spdlog::set_level(spdlog::level::level_enum::debug);

	if (!ini->HasKey("CONFIG", "SilencePersistentSpellFX")) {
		ini->SetBoolValue("CONFIG", "SilencePersistentSpellFX", false, "# If true, will disable persistent spell visuals on maintained spells. This includes flesh spell FX, the aura of Cloak spells, pretty much everything else.");
	}
	MAINT::CONFIG::DoSilenceFX = ini->GetBoolValue("CONFIG", "SilencePersistentSpellFX");
	logger::info("FX Will {} silenced", MAINT::CONFIG::DoSilenceFX ? "be" : "not be");

	if (!ini->HasKey("CONFIG", "CostNeutralDuration")) {
		ini->SetLongValue("CONFIG", "CostNeutralDuration", 60, "# At this BASE spell duration, maintenance cost will be equal to its casting cost. Shorter spells will be more expensive, longer will be cheaper.\n# Reduce to make maintenance cheaper across the board.\n# Set to 0 to disable all cost scaling and only use the spell's casting cost.");
	}
	MAINT::CONFIG::CostBaseDuration = ini->GetLongValue("CONFIG", "CostNeutralDuration");
	logger::info("CostNeutralDuration is {}", MAINT::CONFIG::CostBaseDuration);

	if (!ini->HasKey("CONFIG", "AllowBoundWeapons")) {
		ini->SetBoolValue("CONFIG", "AllowBoundWeapons", true, "# Whether to allow maintained bound weapons (glitchy; only works with proper bound weapon spells, not scripted ones for bound shields or armor)");
	}
	MAINT::CONFIG::AllowBoundWeapons = ini->GetBoolValue("CONFIG", "AllowBoundWeapons");
	logger::info("AllowBoundWeapons is {}", MAINT::CONFIG::AllowBoundWeapons);

	if (!ini->HasKey("CONFIG", "CostReductionExponent")) {
		ini->SetDoubleValue("CONFIG", "CostReductionExponent", 0.0, "# Determines the impact of long durations on maintenance cost.\n# If this is set to 2.0 and a spell would last twice as long as CostNeutralDuration, its upkeep cost would be 1/4th compared to leaving this at 0.0\n# 1.0 would halve the cost. -1.0 would double it instead.\n# 0.0 = Disabled");
	}
	MAINT::CONFIG::CostReductionExponent = static_cast<float>(ini->GetDoubleValue("CONFIG", "CostReductionExponent"));
	logger::info("CostReductionExponent is {}", MAINT::CONFIG::CostReductionExponent);

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
			char* charData = static_cast<char*>(a_msg->data);
			std::string saveFile(charData, a_msg->dataLen);
			logger::info("Load : {}", saveFile);
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
			char* charData = static_cast<char*>(a_msg->data);
			std::string saveFile(charData, a_msg->dataLen);
			std::string saveFileWithExt = std::format("{}.ess", saveFile);
			logger::info("Save : {}", saveFileWithExt);
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