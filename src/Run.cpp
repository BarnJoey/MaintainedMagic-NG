#include "Run.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <ranges>
#include <set>
#include <unordered_map>
#include <vector>

// ================= Utilities =================================================

namespace Maint
{

	RE::FormID lexical_cast_hex_to_formid(std::string_view hex)
	{
		std::string s(hex);
		if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
			s = s.substr(2);
		if (s.empty())
			throw std::invalid_argument("Empty hex string");
		std::istringstream iss(s);
		uint32_t out{};
		if (!(iss >> std::hex >> out))
			throw std::invalid_argument("Invalid hex string");
		return static_cast<RE::FormID>(out);
	}

	namespace
	{
		inline constexpr std::uint32_t SLOT_RIGHT_HAND = 0x13F42;
		inline constexpr std::uint32_t SLOT_LEFT_HAND = 0x13F43;

		inline const RE::BGSEquipSlot* LeftHandSlot()
		{
			static const auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(SLOT_LEFT_HAND);
			return slot;
		}
		inline const RE::BGSEquipSlot* RightHandSlot()
		{
			static const auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(SLOT_RIGHT_HAND);
			return slot;
		}

		inline bool IsBoundWeaponSpell(const RE::SpellItem* s)
		{
			return s && !s->effects.empty() &&
			       s->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kBoundWeapon);
		}
		inline bool IsSummonSpell(const RE::SpellItem* s)
		{
			return s && !s->effects.empty() &&
			       s->effects[0]->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kSummonCreature);
		}
		inline bool IsSelfDelivery(const RE::SpellItem* s)
		{
			return s && s->data.delivery == RE::MagicSystem::Delivery::kSelf;
		}
	}  // namespace

	// ================= CONFIG::ConfigBase ========================================

	Config::ConfigBase* Config::ConfigBase::GetSingleton(const std::string& path)
	{
		if (!cache_.contains(path)) {
			logger::info("Load INI: {}", path);
			cache_[path] = new ConfigBase(path);
		}
		return cache_.at(path);
	}
	bool Config::ConfigBase::HasKey(const std::string& section, const std::string& key) const
	{
		return ini_.KeyExists(section.c_str(), key.c_str());
	}
	bool Config::ConfigBase::HasSection(const std::string& section) const
	{
		return ini_.SectionExists(section.c_str());
	}
	const std::vector<std::pair<std::string, std::string>>
		Config::ConfigBase::GetAllKeyValuePairs(const std::string& section) const
	{
		std::vector<std::pair<std::string, std::string>> out;
		CSimpleIniA::TNamesDepend keys;
		ini_.GetAllKeys(section.c_str(), keys);
		for (auto& k : keys) {
			out.emplace_back(k.pItem, ini_.GetValue(section.c_str(), k.pItem));
		}
		return out;
	}
	const void Config::ConfigBase::GetAllSections(std::list<CSimpleIniA::Entry>* const& out) const
	{
		ini_.GetAllSections(*out);
	}
	void Config::ConfigBase::DeleteSection(const std::string& section) { ini_.Delete(section.c_str(), nullptr, true); }
	void Config::ConfigBase::DeleteKey(const std::string& section, const std::string& key) { ini_.Delete(section.c_str(), key.c_str()); }

	std::string Config::ConfigBase::GetValue(const std::string& s, const std::string& k) const { return ini_.GetValue(s.c_str(), k.c_str()); }
	long Config::ConfigBase::GetLongValue(const std::string& s, const std::string& k) const { return ini_.GetLongValue(s.c_str(), k.c_str()); }
	bool Config::ConfigBase::GetBoolValue(const std::string& s, const std::string& k) const { return ini_.GetBoolValue(s.c_str(), k.c_str()); }
	double Config::ConfigBase::GetDoubleValue(const std::string& s, const std::string& k) const { return ini_.GetDoubleValue(s.c_str(), k.c_str()); }

	void Config::ConfigBase::SetValue(const std::string& s, const std::string& k, const std::string& v, const std::string& c)
	{
		ini_.SetValue(s.c_str(), k.c_str(), v.c_str(), c.empty() ? nullptr : c.c_str());
	}
	void Config::ConfigBase::SetBoolValue(const std::string& s, const std::string& k, bool v, const std::string& c)
	{
		ini_.SetBoolValue(s.c_str(), k.c_str(), v, c.empty() ? nullptr : c.c_str());
	}
	void Config::ConfigBase::SetLongValue(const std::string& s, const std::string& k, long v, const std::string& c)
	{
		ini_.SetLongValue(s.c_str(), k.c_str(), v, c.empty() ? nullptr : c.c_str());
	}
	void Config::ConfigBase::SetDoubleValue(const std::string& s, const std::string& k, double v, const std::string& c)
	{
		ini_.SetDoubleValue(s.c_str(), k.c_str(), v, c.empty() ? nullptr : c.c_str());
	}
	void Config::ConfigBase::Save() { ini_.SaveFile(path_.c_str()); }

	// ================= FormsRepository ===========================================

	FormsRepository& FormsRepository::Get()
	{
		static FormsRepository inst;
		return inst;
	}

	FormsRepository::FormsRepository()
	{
		EquipSlotVoice = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x25BEE);
		KywdMagicCloak = RE::TESForm::LookupByID<RE::BGSKeyword>(0xB62E4);
		KywdMaintainedSpell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x801, "MaintainedMagic.esp"sv);
		KywdExcludeFromSystem = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x80A, "MaintainedMagic.esp"sv);
		SpelMagickaDebuffTemplate = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(0x802, "MaintainedMagic.esp"sv);
		GlobMaintainModeEnabled = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x805, "MaintainedMagic.esp"sv);
		GlobCleanupRequested = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x80F, "MaintainedMagic.esp"sv);
		FlstMaintainedSpellToggle = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSListForm>(0x80B, "MaintainedMagic.esp"sv);
		SpelMindCrush = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(0x80D, "MaintainedMagic.esp"sv);
	}

	void FormsRepository::SetOffset(RE::FormID o) { currentOffset_ = o; }

	void FormsRepository::LoadOffset(const Config::ConfigBase* ini, const std::string& saveFile)
	{
		constexpr auto getLargestInPair = [](const std::string& s) -> RE::FormID {
			auto p = s.find('~');
			if (p == std::string::npos)
				return 0;
			try {
				auto left = lexical_cast_hex_to_formid(s.substr(0, p));
				auto right = lexical_cast_hex_to_formid(s.substr(p + 1));
				return left > right ? left : right;
			} catch (...) {
				return 0;
			}
		};

		RE::FormID off{ 0 };
		for (const auto& [k, v] : ini->GetAllKeyValuePairs(std::format("MAP:{}", saveFile))) {
			(void)k;
			if (auto cur = getLargestInPair(v); cur > off)
				off = cur;
		}
		off &= ~FORMID_OFFSET_BASE;
		currentOffset_ = off;
		logger::info("Local OFFSET:  0x{:08X}", currentOffset_);
		logger::info("Global OFFSET: 0x{:08X}", FORMID_OFFSET_BASE);
	}

	RE::FormID FormsRepository::NextFormID() const
	{
		static RE::FormID local{};
		return FORMID_OFFSET_BASE + (++local) + currentOffset_;
	}

	const RE::TESRace* FormsRepository::WerewolfBeastRace() const
	{
		static const auto* r = RE::TESForm::LookupByID<RE::TESRace>(0x000CDD84);
		return r;
	}
	const RE::TESRace* FormsRepository::VampireBeastRace() const
	{
		static const auto* r = RE::TESForm::LookupByID<RE::TESRace>(0x0200283A);
		return r;
	}

	// ================= MaintainedRegistry ========================================

	MaintainedRegistry& MaintainedRegistry::Get()
	{
		static MaintainedRegistry r;
		return r;
	}
	void MaintainedRegistry::clear()
	{
		map_.clear();
		deferred_.clear();
	}
	bool MaintainedRegistry::empty() const { return map_.empty() && deferred_.empty(); }

	bool MaintainedRegistry::hasBase(RE::SpellItem* base) const { return map_.containsKey(base); }

	std::optional<Domain::MaintainedPair> MaintainedRegistry::getByBase(RE::SpellItem* base) const
	{
		if (auto it = map_.getValueOrNull(base))
			return it.value();
		return std::nullopt;
	}
	void MaintainedRegistry::insert(RE::SpellItem* base, Domain::MaintainedPair pair) { map_.insert(base, pair); }
	void MaintainedRegistry::eraseBase(RE::SpellItem* base) { map_.eraseKey(base); }

	const BiMap<RE::SpellItem*, Domain::MaintainedPair>::forward_map_t&
		MaintainedRegistry::map() const { return map_.GetForwardMap(); }

	void MaintainedRegistry::deferDispel(RE::SpellItem* maintained, RE::SpellItem* base)
	{
		deferred_.emplace(maintained, base);
	}
	bool MaintainedRegistry::isDeferred(RE::SpellItem* maintained, RE::SpellItem* base) const
	{
		return deferred_.find(std::make_pair(maintained, base)) != deferred_.end();
	}
	void MaintainedRegistry::forEachDeferred(const std::function<void(RE::SpellItem*, RE::SpellItem*, bool&)>& fn)
	{
		for (auto it = deferred_.begin(); it != deferred_.end();) {
			bool erase = false;
			fn(it->first, it->second, erase);
			if (erase)
				it = deferred_.erase(it);
			else
				++it;
		}
	}

	// ================= FXSilencer =================================================

	std::vector<RE::Effect*> FXSilencer::SilenceSpellFX(RE::SpellItem* const& spell)
	{
		std::vector<RE::Effect*> ret;
		if (!spell)
			return ret;
		ret.reserve(spell->effects.size());

		for (auto* eff : spell->effects) {
			const bool persists = eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			if (!persists)
				continue;

			using enum RE::EffectSetting::Archetype;
			switch (eff->baseEffect->GetArchetype()) {
			case kLight:
			case kBoundWeapon:
			case kDisguise:
			case kSummonCreature:
			case kNightEye:
			case kInvisibility:
			case kGuide:
			case kWerewolf:
			case kWerewolfFeed:
				spdlog::debug("{} fx will not be silenced", eff->baseEffect->GetName());
				break;
			default:
				eff->baseEffect->data.flags.reset(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
				ret.emplace_back(eff);
			}
		}
		return ret;
	}

	// ================= SpellFactory ==============================================

	RE::SpellItem* SpellFactory::CreateInfiniteFrom(RE::SpellItem* const& base)
	{
		static auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
		const auto* file = base->GetFile(0);
		const auto fileStr = file ? file->GetFilename() : "VIRTUAL";
		spdlog::info("Maintainify({})\n - FID {:08X}\n - File {}", base->GetName(), file ? base->GetLocalFormID() : base->GetFormID(), fileStr);

		auto* out = factory->Create();
		out->SetFormID(FormsRepository::Get().NextFormID(), false);

		out->fullName = std::format("Maintained {}", base->GetFullName());
		out->data = base->data;
		out->avEffectSetting = base->avEffectSetting;
		out->boundData = base->boundData;
		out->descriptionText = base->descriptionText;
		out->equipSlot = base->equipSlot;

		out->data.spellType = RE::MagicSystem::SpellType::kAbility;
		out->SetDelivery(RE::MagicSystem::Delivery::kSelf);
		out->SetCastingType(RE::MagicSystem::CastingType::kConstantEffect);

		for (uint32_t i = 0; i < base->numKeywords; ++i) out->AddKeyword(base->GetKeywordAt(i).value());

		for (auto* eff : base->effects) {
			if (eff->baseEffect->HasArchetype(RE::EffectSetting::Archetype::kCloak)) {
				out->AddKeyword(FormsRepository::Get().KywdMagicCloak);
				break;
			}
		}

		out->AddKeyword(FormsRepository::Get().KywdMaintainedSpell);
		out->effects = base->effects;
		return out;
	}

	RE::SpellItem* SpellFactory::CreateDebuffFrom(RE::SpellItem* const& base, float magnitude)
	{
		static auto* tmpl = FormsRepository::Get().SpelMagickaDebuffTemplate;
		static auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();

		const auto* file = base->GetFile(0);
		const auto fileStr = file ? file->GetFilename() : "VIRTUAL";
		spdlog::info("Debuffify({}, 0x{:08X}~{})", base->GetName(), file ? base->GetLocalFormID() : base->GetFormID(), fileStr);

		auto* out = factory->Create();
		out->SetFormID(FormsRepository::Get().NextFormID(), false);

		out->fullName = std::format("Maintained {}", base->GetFullName());
		out->data = RE::SpellItem::Data{ tmpl->data };
		out->avEffectSetting = tmpl->avEffectSetting;
		out->boundData = tmpl->boundData;

		out->equipSlot = FormsRepository::Get().EquipSlotVoice;
		out->data.spellType = RE::MagicSystem::SpellType::kAbility;
		out->SetDelivery(RE::MagicSystem::Delivery::kSelf);
		out->SetCastingType(RE::MagicSystem::CastingType::kConstantEffect);
		out->AddKeyword(FormsRepository::Get().KywdMaintainedSpell);

		out->effects.emplace_back(tmpl->effects.front());
		out->effects.back()->effectItem.magnitude = magnitude;

		return out;
	}

	// ================= MaintainedEffectsCache ====================================

	void MaintainedEffectsCache::rebuild(RE::Actor* actor)
	{
		cache_.clear();

		static const auto& mmDebufEffect = FormsRepository::Get().SpelMagickaDebuffTemplate->effects.front();
		const auto& effList = actor->AsMagicTarget()->GetActiveEffectList();

		for (auto* e : *effList) {
			if (auto* asSpl = e->spell ? e->spell->As<RE::SpellItem>() : nullptr;
				asSpl && e->effect->baseEffect != mmDebufEffect->baseEffect) {
				const bool hasKywd = asSpl->HasKeyword(FormsRepository::Get().KywdMaintainedSpell);
				const bool isBaseSpell = MaintainedRegistry::Get().hasBase(asSpl);

				if (isBaseSpell) {
					const auto& pair = MaintainedRegistry::Get().getByBase(asSpl).value();
					auto* mSpl = pair.infinite;
					cache_[mSpl].push_back(e);
				} else if (hasKywd) {
					e->elapsedSeconds = 0.0f;
					cache_[asSpl].push_back(e);
				}
			}
		}
	}
	const std::unordered_map<RE::SpellItem*, std::vector<RE::ActiveEffect*>>&
		MaintainedEffectsCache::GetFor(RE::Actor* actor)
	{
		const auto count = std::ranges::distance(actor->AsMagicTarget()->GetActiveEffectList()->begin(),
			actor->AsMagicTarget()->GetActiveEffectList()->end());
		if (count != cache_.size()) {
			rebuild(actor);
		}
		return cache_;
	}
	void MaintainedEffectsCache::Clear() {
		cache_.clear();
	}

	// ================= Policy / Calculations =====================================

	bool SpellEligibilityPolicy::IsMaintainable(RE::SpellItem* const& s, RE::Actor* const& caster)
	{
		if (!s || !caster)
			return false;

		if (s->As<RE::ScrollItem>()) {
			spdlog::info("Spell is Scroll");
			return false;
		}
		if (s->As<RE::EnchantmentItem>()) {
			spdlog::info("Spell is Enchantment");
			return false;
		}
		if (s->effects.empty()) {
			spdlog::info("Spell has no effects");
			return false;
		}
		if (s->data.castingType != RE::MagicSystem::CastingType::kFireAndForget) {
			spdlog::info("Not FF");
			return false;
		}

		if (const auto dur = s->effects.front()->GetDuration(); dur <= 5.0) {
			spdlog::info("Duration <= 5s");
			return false;
		}
		{
			const auto cWith = s->CalculateMagickaCost(caster);
			const auto cWithout = s->CalculateMagickaCost(nullptr);
			if (cWith <= 5.0 && cWithout <= 5.0) {
				spdlog::info("Cost <= 5");
				return false;
			}
		}

		if (s->HasKeyword(FormsRepository::Get().KywdMaintainedSpell)) {
			spdlog::info("Has Maintained kwd");
			return false;
		}
		if (s->HasKeyword(FormsRepository::Get().KywdExcludeFromSystem)) {
			spdlog::info("Has exclusion kwd");
			return false;
		}
		if (s->HasKeywordString("_m3HealerDummySpell")) {
			spdlog::info("Has Allylink kwd");
			return false;
		}

		const auto arche = s->effects[0]->baseEffect->GetArchetype();
		if (!IsSelfDelivery(s)) {
			if (arche == RE::EffectSetting::Archetype::kSummonCreature)
				return true;
			spdlog::info("Not self and not summon");
			return false;
		}

		if (arche == RE::EffectSetting::Archetype::kBoundWeapon) {
			if (!Config::AllowBoundWeapons) {
				spdlog::info("Bound weapon disallowed");
				return false;
			}

			// Prevent maintaining if both hands summon the same associated form.
			const auto leftObj = caster->GetEquippedObject(true);
			const auto rightObj = caster->GetEquippedObject(false);
			const auto leftSpell = leftObj ? leftObj->As<RE::SpellItem>() : nullptr;
			const auto rightSpell = rightObj ? rightObj->As<RE::SpellItem>() : nullptr;

			const auto assoc = s->effects[0]->baseEffect->data.associatedForm;
			const bool leftSame = leftSpell && !leftSpell->effects.empty() && leftSpell->effects[0]->baseEffect->data.associatedForm == assoc;
			const bool rightSame = rightSpell && !rightSpell->effects.empty() && rightSpell->effects[0]->baseEffect->data.associatedForm == assoc;

			if (leftSame && rightSame) {
				spdlog::info("Already dual-equipped bound");
				RE::DebugNotification(std::format("Only one instance of {} can be maintained.", s->GetName()).c_str());
				return false;
			}
		}

		return true;
	}

	float UpkeepCostCalculator::Calculate(RE::SpellItem* const& baseSpell, RE::Actor* const& caster)
	{
		static auto const& NEUTRAL = static_cast<float>(Config::CostBaseDuration);
		static auto const& EXP = Config::CostReductionExponent;

		spdlog::info("CalculateUpkeepCost()");
		const float baseCost = baseSpell->CalculateMagickaCost(caster);
		if (NEUTRAL == 0.0F)
			return baseCost;

		const auto& firstEff = baseSpell->effects.front();
		const auto baseDur = static_cast<float>(firstEff->GetDuration());
		const auto effDur32 = std::max<uint32_t>(1u, firstEff->GetDuration());

		float mult = effDur32 < NEUTRAL ? std::pow(NEUTRAL / effDur32, 2.0f) : std::sqrt(NEUTRAL / effDur32);
		const float magicaRegen = caster->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagickaRateMult);
		mult *= magicaRegen > 0.0f ? (500.f / (500.f + magicaRegen)) : 1.f;


		float finalDur = baseDur;
		for (auto* aeff : *caster->AsMagicTarget()->GetActiveEffectList()) {
			if (aeff->spell == baseSpell && aeff->GetCasterActor().get() == caster && aeff->effect == firstEff) {
				finalDur = aeff->duration;
				mult *= std::sqrt(baseDur / finalDur);
				spdlog::info("Neutral {} vs Base {} vs Real {} => Mult {}", NEUTRAL, baseDur, aeff->duration, mult);
				break;
			}
		}
		if (finalDur > baseDur) {
			mult *= std::pow(NEUTRAL / finalDur, EXP);
			spdlog::info("After exponent: {}", mult);
		}
		return std::round(baseCost * mult);
	}

	// ================= EffectRestorer ============================================

	ConcurrentQueue<RE::Effect*>& EffectRestorer::Q()
	{
		static ConcurrentQueue<RE::Effect*> q;
		return q;
	}
	void EffectRestorer::Push(RE::Effect* const& e) { Q().push(e); }
	void EffectRestorer::DrainAndRestore()
	{
		while (!Q().empty()) {
			Q().front()->baseEffect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
			Q().pop();
		}
	}

	// ================= ExperienceService =========================================

	void ExperienceService::AwardPlayerExperience(RE::PlayerCharacter* const& player)
	{
		if (Config::MaintainedExpMultiplier <= 0.0f)
			return;
		for (const auto& [base, _] : MaintainedRegistry::Get().map()) {
			const float baseCost = base->CalculateMagickaCost(nullptr);
			player->AddSkillExperience(base->GetAssociatedSkill(), baseCost * Config::MaintainedExpMultiplier);
		}
	}

	// ================= MaintenanceOrchestrator ===================================

	void MaintenanceOrchestrator::MaintainSpell(RE::SpellItem* const& baseSpell, RE::Actor* const& caster)
	{
		using namespace std;

		spdlog::info("MaintainSpell({}, 0x{:08X})", baseSpell->GetName(), baseSpell->GetFormID());

		if (!SpellEligibilityPolicy::IsMaintainable(baseSpell, caster)) {
			RE::DebugNotification(std::format("Cannot maintain {}.", baseSpell->GetName()).c_str());
			return;
		}

		const float baseCost = baseSpell->CalculateMagickaCost(caster);
		const float magCost = UpkeepCostCalculator::Calculate(baseSpell, caster);

		if (magCost > caster->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) + baseCost) {
			RE::DebugNotification(std::format("Need {} Magicka to maintain {}.", static_cast<uint32_t>(magCost), baseSpell->GetName()).c_str());
			return;
		}

		if (MaintainedRegistry::Get().hasBase(baseSpell)) {
			spdlog::info("\tAlready has constant version");
			return;
		}

		auto* maint = SpellFactory::CreateInfiniteFrom(baseSpell);
		auto* debuff = SpellFactory::CreateDebuffFrom(baseSpell, magCost);

		spdlog::info("\tRemoving base effects for {}", baseSpell->GetName());
		auto handle = caster->GetHandle();
		caster->AsMagicTarget()->DispelEffect(baseSpell, handle);
		caster->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIERS::ACTOR_VALUE_MODIFIER::kDamage,
			RE::ActorValue::kMagicka, baseCost);

		if (Config::DoSilenceFX) {
			spdlog::info("Silencing SpellFX");
			for (auto* e : FXSilencer::SilenceSpellFX(maint))
				UpdatePCHook::PushFXRestore(e);
		}

		spdlog::info("\tAdding constant effect (cost {})", magCost);
		if (IsBoundWeaponSpell(baseSpell)) {
			auto* eq = RE::ActorEquipManager::GetSingleton();
			if (caster->GetEquippedObject(true) == baseSpell->As<RE::TESForm>()) {
				eq->EquipSpell(caster, maint, LeftHandSlot());
			} else if (caster->GetEquippedObject(false) == baseSpell->As<RE::TESForm>()) {
				eq->EquipSpell(caster, maint, RightHandSlot());
			}
		} else {
			caster->AddSpell(maint);
		}
		caster->AddSpell(debuff);

		MaintainedRegistry::Get().insert(baseSpell, Domain::MaintainedPair{ maint, debuff });
		FormsRepository::Get().FlstMaintainedSpellToggle->AddForm(baseSpell);

		RE::DebugNotification(std::format("Maintaining {} for {} Magicka.", baseSpell->GetName(), static_cast<uint32_t>(magCost)).c_str());
	}

	void MaintenanceOrchestrator::PurgeAll()
	{
		spdlog::info("Purge()");
		for (const auto& [_, v] : MaintainedRegistry::Get().map()) {
			v.infinite->SetDelete(true);
			v.debuff->SetDelete(true);
		}
		FormsRepository::Get().FlstMaintainedSpellToggle->ClearData();
		MaintainedRegistry::Get().clear();
		UpkeepSupervisor::ClearCache();
	}

	void MaintenanceOrchestrator::BuildActiveSpellsCache()
	{
		spdlog::info("BuildActiveSpellsCache()");
		static const auto& player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			spdlog::error("\tPlayer is NULL");
			return;
		}

		// Repopulate toggle list
		for (auto* s : player->GetActorRuntimeData().addedSpells) {
			if (MaintainedRegistry::Get().hasBase(s))
				FormsRepository::Get().FlstMaintainedSpellToggle->AddForm(s);
		}

		// Restore debuff magnitudes from live aeffs
		const auto& effs = player->AsMagicTarget()->GetActiveEffectList();
		for (const auto& [base, p] : MaintainedRegistry::Get().map()) {
			(void)base;
			for (auto* a : *effs) {
				if (a->spell == p.debuff && a->GetCasterActor().get() == player && a->effect == p.debuff->effects.front()) {
					spdlog::info("Restoring {} magnitude to {}", p.debuff->GetName(), std::abs(a->GetMagnitude()));
					p.debuff->effects.front()->effectItem.magnitude = std::abs(a->GetMagnitude());
					break;
				}
			}
		}
	}

	// ================= UpkeepSupervisor ==========================================
	void UpkeepSupervisor::ClearCache(){
		cache_.Clear();
	}
	void UpkeepSupervisor::ForceMaintainedSpellUpdate(RE::Actor* const& actor)
	{
		if (MaintainedRegistry::Get().empty())
			return;

		// Timing (rolling avg over window)
		constexpr uint32_t WIN{ 100 };
		static double acc{ 0.0 };
		static uint32_t cnt{ 0 };
		const auto start = std::chrono::high_resolution_clock::now();

		// Apply deferred dispels (bound weapon hand state)
		MaintainedRegistry::Get().forEachDeferred([&](RE::SpellItem* maintained, RE::SpellItem* base, bool& erase) {
			auto* eq = RE::ActorEquipManager::GetSingleton();
			// 0=left, 1=right
			if (actor->GetActorRuntimeData().selectedSpells[0] == maintained) {
				spdlog::debug("Deferred restore (left): {}", maintained->GetName());
				eq->EquipSpell(actor, base, LeftHandSlot());
				erase = true;
			}
			if (actor->GetActorRuntimeData().selectedSpells[1] == maintained) {
				spdlog::debug("Deferred restore (right): {}", maintained->GetName());
				eq->EquipSpell(actor, base, RightHandSlot());
				erase = true;
			}
		});

		std::vector<std::pair<RE::SpellItem*, Domain::MaintainedPair>> toRemove;

		for (const auto& [base, pair] : MaintainedRegistry::Get().map()) {
			if (FormsRepository::Get().GlobCleanupRequested->value != 0) {
				spdlog::debug("Dispelled by player: {}", base->GetName());
				toRemove.emplace_back(base, pair);
				continue;
			}
			auto* m = pair.infinite;
			auto* d = pair.debuff;

			// Bound weapon validation
			if (IsBoundWeaponSpell(base)) {
				bool found = false;
				const auto* right = actor->GetEquippedObject(false);

				if (right && right->IsWeapon()) {
					const auto* weap = right->As<RE::TESObjectWEAP>();
					for (auto* eff : base->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto* rSpell = actor->GetActorRuntimeData().selectedSpells[1];
					if (rSpell == m && actor->HasSpell(d))
						continue;
				}

				const auto* left = actor->GetEquippedObject(true);
				if (left && left->IsWeapon()) {
					const auto* weap = left->As<RE::TESObjectWEAP>();
					for (auto* eff : base->effects) {
						if (weap->formID == eff->baseEffect->data.associatedForm->formID) {
							found = true;
							break;
						}
					}
					if (found)
						continue;
				} else {
					auto* lSpell = actor->GetActorRuntimeData().selectedSpells[0];
					if (lSpell == m && actor->HasSpell(d))
						continue;
				}
			}

			const auto& spell2ae = cache_.GetFor(actor);
			const auto it = spell2ae.find(m);
			if (it == spell2ae.end()) {
				spdlog::debug("{} not found on Actor", m->GetName());
				toRemove.emplace_back(base, pair);
				continue;
			}

			const auto& effSet = it->second;

			if (m->effects.size() < effSet.size()) {
				spdlog::trace("{} EFF mismatch: LESS", m->GetName());
				toRemove.emplace_back(base, pair);
				continue;
			} else if (m->effects.size() > effSet.size()) {
				spdlog::trace("{} EFF mismatch: MORE", m->GetName());

				const auto getUniques = [](const RE::BSTArray<RE::Effect*>& arr) {
					std::set<RE::TESForm*> uniq;
					std::vector<RE::Effect*> out;
					out.reserve(arr.size());
					for (auto* item : arr) {
						auto* assoc = item->baseEffect->data.associatedForm;
						if (assoc && uniq.insert(assoc).second)
							out.push_back(item);
					}
					return out;
				};
				const auto uniqueList = getUniques(m->effects);

				const auto wrongSrc = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->spell->As<RE::SpellItem>() != m;
				});
				if (wrongSrc != effSet.end()) {
					spdlog::debug("\tSource mismatch; found at least one: {} (0x{:08X})",
						(*wrongSrc)->spell->GetName(), (*wrongSrc)->spell->GetFormID());
					toRemove.emplace_back(base, pair);
					continue;
				}
				if (!uniqueList.empty() && uniqueList.size() > effSet.size()) {
					spdlog::debug("\tExclusives are missing");
					toRemove.emplace_back(base, pair);
					continue;
				}
			} else {
				constexpr uint32_t HUGE_DUR = 60 * 60 * 24 * 356;
				const auto wrongDur = std::find_if(effSet.begin(), effSet.end(), [&](RE::ActiveEffect* e) {
					return e->duration > 0.0 && static_cast<uint32_t>(e->duration - e->elapsedSeconds) < HUGE_DUR;
				});
				if (wrongDur != effSet.end()) {
					spdlog::debug("EFF duration mismatch");
					toRemove.emplace_back(base, pair);
					continue;
				}
			}

			const auto active = std::find_if(effSet.begin(), effSet.end(), [](RE::ActiveEffect* e) {
				return !e->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled);
			});
			if (active == effSet.end()) {
				spdlog::debug("{} has zero actives", m->GetName());
				toRemove.emplace_back(base, pair);
			}
		}

		if (!toRemove.empty()) {
			for (const auto& [base, pair] : toRemove) {
				auto* m = pair.infinite;
				auto* d = pair.debuff;
				spdlog::info("Dispelling missing/invalid {} (0x{:08X})", m->GetName(), m->GetFormID());

				if (IsBoundWeaponSpell(m) && !MaintainedRegistry::Get().isDeferred(m, base)) {
					spdlog::debug("Deferring cleanup of {}", m->GetName());
					MaintainedRegistry::Get().deferDispel(m, base);
				}

				if (actor->HasSpell(d)) {
					actor->RemoveSpell(m);
					actor->RemoveSpell(d);
					if (Config::InstantDispel) {
						static auto handle = actor->GetHandle();
						actor->AsMagicTarget()->DispelEffect(base, handle);
					}
					RE::DebugNotification(std::format("{} is no longer being maintained.", base->GetName()).c_str());
				}
				MaintainedRegistry::Get().eraseBase(base);
			}

			FormsRepository::Get().FlstMaintainedSpellToggle->ClearData();
			for (const auto& [spl, _] : MaintainedRegistry::Get().map())
				FormsRepository::Get().FlstMaintainedSpellToggle->AddForm(spl);
			FormsRepository::Get().GlobCleanupRequested->value = 0;
		}

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> dur = end - start;
		acc += dur.count();
		++cnt;
		if (cnt == WIN) {
			const auto ms = (1000.0 * acc) / cnt;
			spdlog::info("ForceMaintainedSpellUpdate() avg time: {:.3f}ms", ms);
			acc = 0.0;
			cnt = 0;
		}
	}

	void UpkeepSupervisor::CheckUpkeepValidity(RE::Actor* const& actor)
	{
		if (MaintainedRegistry::Get().empty())
			return;

		const float av = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
		if (av >= 0)
			return;

		static auto* mindCrush = FormsRepository::Get().SpelMindCrush;

		if (actor->GetRace() == FormsRepository::Get().WerewolfBeastRace() ||
			actor->GetRace() == FormsRepository::Get().VampireBeastRace() ||
			actor->AsMagicTarget()->HasMagicEffect(mindCrush->effects[0]->baseEffect)) {
			return;
		}

		spdlog::debug("Triggered Mind Crush");

		const auto& map = MaintainedRegistry::Get().map();
		const float totalDrain = std::accumulate(map.begin(), map.end(), 0.0f,
			[](float cur, const std::pair<RE::SpellItem* const, Domain::MaintainedPair>& e) {
				return cur + e.second.debuff->effects.front()->GetMagnitude();
			});

		for (const auto& [base, pair] : MaintainedRegistry::Get().map()) {
			if (IsBoundWeaponSpell(base)) {
				MaintainedRegistry::Get().deferDispel(pair.infinite, base);
			}
		}

		actor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)
			->CastSpellImmediate(mindCrush, false, actor, 1.0, true, totalDrain, nullptr);
	}

	// ================= SaveMappingService ========================================

	void SaveMappingService::Load(const std::string& identifier)
	{
		spdlog::info("LoadSavegameMapping({})", identifier);

		constexpr auto parseKey = [](const std::string& s) -> std::pair<std::string, RE::FormID> {
			auto t = s.find('~');
			if (t == std::string::npos)
				return { {}, 0 };
			return { s.substr(0, t), lexical_cast_hex_to_formid(s.substr(t + 1)) };
		};
		constexpr auto parseVals = [](const std::string& s) -> std::pair<RE::FormID, RE::FormID> {
			auto t = s.find('~');
			if (t == std::string::npos)
				return { 0, 0 };
			try {
				return { lexical_cast_hex_to_formid(s.substr(0, t)),
					lexical_cast_hex_to_formid(s.substr(t + 1)) };
			} catch (...) {
				return { 0, 0 };
			}
		};

		const auto* ini = Config::ConfigBase::GetSingleton(Config::MAP_FILE);
		auto section = std::format("MAP:{}", identifier);

		for (const auto& [k, v] : ini->GetAllKeyValuePairs(section)) {
			const auto& [plugin, formid] = parseKey(k);
			const auto& [maintSpellFID, debuffFID] = parseVals(v);

			RE::SpellItem* base = plugin != "VIRTUAL" ? RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(formid, plugin) : RE::TESForm::LookupByID<RE::SpellItem>(formid);

			if (!base) {
				spdlog::error("Base Spell {}~{:08X} not found", plugin, formid);
				continue;
			}

			auto* inf = SpellFactory::CreateInfiniteFrom(base);
			auto* debuff = SpellFactory::CreateDebuffFrom(base, 0.0f);
			if (maintSpellFID != 0x0)
				inf->SetFormID(maintSpellFID, false);
			if (debuffFID != 0x0)
				debuff->SetFormID(debuffFID, false);

			MaintainedRegistry::Get().insert(base, Domain::MaintainedPair{ inf, debuff });
		}
	}

	void SaveMappingService::Store(const std::string& saveIdentifierWithExt)
	{
		spdlog::info("StoreSavegameMapping({})", saveIdentifierWithExt);
		static auto* ini = Config::ConfigBase::GetSingleton(Config::MAP_FILE);

		const auto section = std::format("MAP:{}", saveIdentifierWithExt);
		ini->DeleteSection(section);

		for (const auto& [base, pair] : MaintainedRegistry::Get().map()) {
			const auto* file = base->GetFile(0);
			const auto fname = file ? file->GetFilename() : "VIRTUAL"sv;
			const auto key = std::format("{}~0x{:08X}", fname, file ? base->GetLocalFormID() : base->GetFormID());
			const auto value = std::format("0x{:08X}~0x{:08X}", pair.infinite->GetFormID(), pair.debuff->GetFormID());
			ini->SetValue(section, key, value, std::format("# {}", base->GetName()));
		}
		ini->Save();
	}

	// ================= UpdatePCHook ==============================================

	void UpdatePCHook::Install()
	{
		REL::Relocation<std::uintptr_t> pcVTable{ RE::VTABLE_PlayerCharacter[0] };
		UpdatePC = pcVTable.write_vfunc(0xAD, UpdatePCMod);
	}
	void UpdatePCHook::ResetEffCheckTimer() { TimerActiveEffCheck = 0.0f; }
	void UpdatePCHook::PushFXRestore(RE::Effect* const& eff)
	{
		EffectRestorationQueue.push(eff);
		ResetEffCheckTimer();
	}

	void UpdatePCHook::UpdatePCMod(RE::PlayerCharacter* pc, float delta)
	{
		UpdatePC(pc, delta);

		TimerActiveEffCheck += delta;
		TimerExperienceAward += delta;

		if (TimerActiveEffCheck >= 0.50f) {
			UpkeepSupervisor::ForceMaintainedSpellUpdate(pc);
			UpkeepSupervisor::CheckUpkeepValidity(pc);
			EffectRestorer::DrainAndRestore();
			TimerActiveEffCheck = 0.0f;
		}
		if (TimerExperienceAward >= 300) {
			ExperienceService::AwardPlayerExperience(pc);
			TimerExperienceAward = 0.0f;
		}
	}

	// ================= Legacy public API =========================================

	void ForceMaintainedSpellUpdate(RE::Actor* const& a) { UpkeepSupervisor::ForceMaintainedSpellUpdate(a); }
	void AwardPlayerExperience(RE::PlayerCharacter* const& pc) { ExperienceService::AwardPlayerExperience(pc); }
	void CheckUpkeepValidity(RE::Actor* const& a) { UpkeepSupervisor::CheckUpkeepValidity(a); }

	// ================= Events ====================================================

	class SpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* e, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
		{
			if (!e || !e->object)
				return RE::BSEventNotifyControl::kContinue;

			auto* caster = e->object->As<RE::Actor>();
			if (caster != RE::PlayerCharacter::GetSingleton())
				return RE::BSEventNotifyControl::kContinue;

			if (static_cast<short>(FormsRepository::Get().GlobMaintainModeEnabled->value) == 0)
				return RE::BSEventNotifyControl::kContinue;

			if (auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(e->spell)) {
				MaintenanceOrchestrator::MaintainSpell(spell, caster);
				UpdatePCHook::ResetEffCheckTimer();
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		static SpellCastEventHandler& GetSingleton()
		{
			static SpellCastEventHandler s;
			return s;
		}
		static void Install()
		{
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSpellCastEvent>(&GetSingleton());
		}
	};

	// ================= Lifecycle / Messaging =====================================

	static void ReadConfiguration()
	{
		spdlog::info("Maintained Map @ {}", Config::MAP_FILE);
		spdlog::info("Maintained Config @ {}", Config::CONFIG_FILE);

		static auto* ini = Config::ConfigBase::GetSingleton(Config::CONFIG_FILE);

		const std::map<std::string, spdlog::level::level_enum> logMap{
			{ "off", spdlog::level::off },
			{ "info", spdlog::level::info },
			{ "debug", spdlog::level::debug },
		};

		if (!ini->HasKey("CONFIG", "LogLevel"))
			ini->SetValue("CONFIG", "LogLevel", "info", "# Options: off, info, debug");

		const auto confLog = ini->GetValue("CONFIG", "LogLevel");
		spdlog::info("Set Log Level to {}", confLog);
		if (auto it = logMap.find(confLog); it != logMap.end())
			spdlog::set_level(it->second);
		else
			spdlog::set_level(spdlog::level::debug);

		if (!ini->HasKey("CONFIG", "SilencePersistentSpellFX")) {
			ini->SetBoolValue("CONFIG", "SilencePersistentSpellFX", false,
				"# If true, will disable persistent spell visuals on maintained spells. This includes flesh spell FX, the aura of Cloak spells, pretty much everything else.");
		}
		Config::DoSilenceFX = ini->GetBoolValue("CONFIG", "SilencePersistentSpellFX");
		spdlog::info("FX Will {} silenced", Config::DoSilenceFX ? "be" : "not be");

		if (!ini->HasKey("CONFIG", "CostNeutralDuration")) {
			ini->SetLongValue("CONFIG", "CostNeutralDuration", 60,
				"# At this BASE spell duration, maintenance cost will be equal to its casting cost. Shorter spells will be more expensive, longer will be cheaper.\n"
				"# Reduce to make maintenance cheaper across the board.\n"
				"# Set to 0 to disable all cost scaling and only use the spell's casting cost.");
		}
		Config::CostBaseDuration = ini->GetLongValue("CONFIG", "CostNeutralDuration");
		spdlog::info("CostNeutralDuration is {}", Config::CostBaseDuration);

		if (!ini->HasKey("CONFIG", "AllowBoundWeapons")) {
			ini->SetBoolValue("CONFIG", "AllowBoundWeapons", true,
				"# Whether to allow maintained bound weapons.\n"
				"# Sometimes a little glitchy. Only works with proper bound weapon spells, not script-based ones for bound shields or armor.\n"
				"# Must be re-summoned every time after unsheathing weapons (this is instant and free).\n"
				"# Maintenance will break if the bound weapon or bound weapon spell is removed from your hand.");
		}
		Config::AllowBoundWeapons = ini->GetBoolValue("CONFIG", "AllowBoundWeapons");
		spdlog::info("AllowBoundWeapons is {}", Config::AllowBoundWeapons);

		if (!ini->HasKey("CONFIG", "InstantDispel")) {
			ini->SetBoolValue("CONFIG", "InstantDispel", true,
				"# When true, will instantly dispel a maintained spell's effects when it is recast rather than applying the standard spell with its normal duration.");
		}
		Config::InstantDispel = ini->GetBoolValue("CONFIG", "InstantDispel");
		spdlog::info("InstantDispel is {}", Config::InstantDispel);

		if (!ini->HasKey("CONFIG", "CostReductionExponent")) {
			ini->SetDoubleValue("CONFIG", "CostReductionExponent", 0.0,
				"# Determines the impact of spell durations on their maintenance cost.\n"
				"# Basically, final cost will be multiplied by (CostNeutralDuration / Effective Spell Duration)^CostReductionExponent\n"
				"# Leave at 0.0 to just use basic scaling (see CostNeutralDuration)");
		}
		Config::CostReductionExponent = static_cast<float>(ini->GetDoubleValue("CONFIG", "CostReductionExponent"));
		spdlog::info("CostReductionExponent is {}", Config::CostReductionExponent);

		if (!ini->HasKey("CONFIG", "MaintainedExpMultiplier")) {
			ini->SetDoubleValue("CONFIG", "MaintainedExpMultiplier", 1.0,
				"# Every 300 seconds, receive experience for each maintained spell equal to one spell cast, multiplied by this value.\n"
				"# 0.5 is half, 2.0 is double, 0 to disable passive experience entirely.");
		}
		Config::MaintainedExpMultiplier = static_cast<float>(ini->GetDoubleValue("CONFIG", "MaintainedExpMultiplier"));
		spdlog::info("MaintainedExpMultiplier is {}", Config::MaintainedExpMultiplier);

		if (!ini->HasKey("CONFIG", "CleanupMissingSaves")) {
			ini->SetBoolValue("CONFIG", "CleanupMissingSaves", false,
				"# If true, will clean up the internal database and remove data belonging to missing save files to prevent bloat.\n"
				"# CAUTION: If the save-file is restored (recovered from a backup/recycle bin) after a database cleanup, that character may have reduced magicka and require manual fixing through the console!");
		}
		Config::CleanupMissingSaves = ini->GetBoolValue("CONFIG", "CleanupMissingSaves");
		spdlog::info("CleanupMissingSaves is {}", Config::CleanupMissingSaves);

		ini->Save();
	}

	static void DoDatabaseMaintenance(const std::string current)
	{
		if (!Config::CleanupMissingSaves)
			return;
		spdlog::info("DoDatabaseMaintenance()");

		const auto& allLocalSaves = RE::BGSSaveLoadManager::GetSingleton()->saveGameList;

		if (allLocalSaves.empty())
			return;

		const auto& ini = Config::ConfigBase::GetSingleton(Config::MAP_FILE);
		std::list<CSimpleIniA::Entry> allMappings;
		ini->GetAllSections(&allMappings);

		if (allMappings.size() == allLocalSaves.size())
			return;

		for (const auto& mapEntry : allMappings) {
			if (allLocalSaves.end() != std::find_if(allLocalSaves.begin(), allLocalSaves.end(),
										   [&](const auto& saveFileName) {
											   const auto mapString = std::format("MAP:{}.ess", saveFileName->fileName.c_str());
											   return std::strcmp(mapEntry.pItem, mapString.c_str()) == 0;
										   })) {
				continue;
			}

			if (std::strcmp(mapEntry.pItem, current.c_str()) != 0) {
				spdlog::info("DoDatabaseMaintenance() : Deleting missing mapping for {}", mapEntry.pItem);
				ini->DeleteSection(mapEntry.pItem);
			}
		}
		ini->Save();
	}

	void OnInit(SKSE::MessagingInterface::Message* const msg)
	{
		switch (msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			ReadConfiguration();
			break;

		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			if (msg->dataLen > 0) {
				auto* bytes = static_cast<char*>(msg->data);
				std::string saveFile(bytes, msg->dataLen);
				spdlog::info("Load : {}", saveFile);

				MaintenanceOrchestrator::PurgeAll();
				FormsRepository::Get().LoadOffset(Config::ConfigBase::GetSingleton(Config::MAP_FILE), saveFile);
				SaveMappingService::Load(saveFile);
				DoDatabaseMaintenance(saveFile);
			}
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
			MaintenanceOrchestrator::BuildActiveSpellsCache();
			break;

		case SKSE::MessagingInterface::kSaveGame:
			if (msg->dataLen > 0) {
				auto* bytes = static_cast<char*>(msg->data);
				std::string saveFile(bytes, msg->dataLen);
				std::string saveWithExt = std::format("{}.ess", saveFile);
				spdlog::info("Save : {}", saveWithExt);
				SaveMappingService::Store(saveWithExt);
			}
			break;

		default:
			break;
		}
	}

	bool Load()
	{
		SpellCastEventHandler::Install();
		UpdatePCHook::Install();
		return true;
	}

}  // namespace MAINT

// ----- Global entry points expected by the loader (forward to MAINT) -----
bool Load()
{
	return Maint::Load();
}

void OnInit(SKSE::MessagingInterface::Message* const msg)
{
	Maint::OnInit(msg);
}