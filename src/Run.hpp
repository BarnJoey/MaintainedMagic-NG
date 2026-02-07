#pragma once

#include <SimpleIni.h>

#include <atomic>
#include <chrono>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Maint
{
	// ===== Global config values (backed by INI) ==================================

	namespace Config
	{
		inline constexpr const char* PLUGIN_CONFIG = "Data/SKSE/Plugins/MaintainedMagicNG.Config.ini";
		inline constexpr const char* MCM_DEFAULTS = "Data/MCM/Config/MaintainedMagic/settings.ini";
		inline constexpr const char* MCM_USER = "Data/MCM/Settings/MaintainedMagic.ini";

		constexpr float kDefaultFXRestoreDelay = 0.75f;

		inline std::string SAVES_PATH = "disabled";
		inline bool DoSilenceFX = false;
		inline long CostBaseDuration = 60;  // seconds, “neutral” duration
		inline float UpkeepDurationExponent = 0.45f;  // driven by difficulty slider
		inline float UpkeepAsymptoteKnee = 0.4f;
		inline bool AllowBoundWeapons = true;
		inline float MaintainedExpMultiplier = 1.0f;
		inline bool InstantDispel = true;

		inline float ConjureRecastDelay = 20.0f;

		inline float MagickaRegenPenalty = 500.0f;    // softness constant


		// Simple wrapper over SimpleIni with multi-instance cache by path.
		class ConfigBase
		{
			CSimpleIniA ini_;
			std::string path_;
			static inline std::map<std::string, ConfigBase*> cache_;
			explicit ConfigBase(std::string p) :
				path_(std::move(p))
			{
				ini_.SetUnicode();
				ini_.LoadFile(path_.c_str());
			}

		public:
			static ConfigBase* GetSingleton(const std::string& path);
			void Reload();

			bool HasKey(const std::string& section, const std::string& key) const;
			bool HasSection(const std::string& section) const;

			const std::vector<std::pair<std::string, std::string>> GetAllKeyValuePairs(const std::string& section) const;
			const void GetAllSections(std::list<CSimpleIniA::Entry>* const& out) const;

			void DeleteSection(const std::string& section);
			void DeleteKey(const std::string& section, const std::string& key);

			std::string GetValue(const std::string& section, const std::string& key) const;
			long GetLongValue(const std::string& section, const std::string& key) const;
			bool GetBoolValue(const std::string& section, const std::string& key) const;
			double GetDoubleValue(const std::string& section, const std::string& key) const;

			void SetValue(const std::string& section, const std::string& key, const std::string& value, const std::string& comment = {});
			void SetBoolValue(const std::string& section, const std::string& key, bool value, const std::string& comment = {});
			void SetLongValue(const std::string& section, const std::string& key, long value, const std::string& comment = {});
			void SetDoubleValue(const std::string& section, const std::string& key, double value, const std::string& comment = {});
			void Save();
			ConfigBase(ConfigBase const&) = delete;
			void operator=(ConfigBase const&) = delete;
		};
	}  // namespace CONFIG

	// ===== Domain Types ==========================================================

	namespace Domain
	{
		using InfiniteSpell = RE::SpellItem;
		using DebuffSpell = RE::SpellItem;

		enum class FxSilenceMode : std::uint8_t
		{
			kNone = 0,
			kPersistToggle,
			kShaderFallback
		};

		struct EffectShaderAlphaState
		{
			float fillPersistent{ 1.0f };
			float fillFull{ 1.0f };
			float edgePersistent{ 1.0f };
			float edgeFull{ 1.0f };

			bool valid{ false };
		};

		struct SilencedEffect
		{
			// ---- Identity ----
			RE::Effect* effect{ nullptr };             // runtime effect
			RE::EffectSetting* baseEffect{ nullptr };  // defensive pointer

			// ---- Silence state ----
			FxSilenceMode silenceMode{ FxSilenceMode::kNone };

			// ---- Persist toggle restore ----
			bool hadFXPersist{ false };

			// ---- Shader fallback restore ----
			RE::TESEffectShader* shader{ nullptr };
			EffectShaderAlphaState shaderAlpha{};
		};

		struct MaintainedPair
		{
			InfiniteSpell* infinite{ nullptr };
			DebuffSpell* debuff{ nullptr };

			// ---- FX tracking ----
			std::vector<SilencedEffect> silencedEffects;

			// ---- Conjuration metadata ----
			bool isConjureMinion{ false };

			// ---- Recast state ----
			float recastRemaining{ 0.0f };  // seconds; <= 0 means inactive
			bool recastQueued{ false };

			// Convenience
			bool HasSilencedFX() const noexcept
			{
				return !silencedEffects.empty();
			}

			bool NeedsRecastUpdate() const noexcept
			{
				return isConjureMinion && recastQueued;
			}
		};

		// Provide ordering & equality so MaintainedPair can be a map key
		inline bool operator==(const MaintainedPair& a, const MaintainedPair& b) noexcept
		{
			return a.infinite == b.infinite && a.debuff == b.debuff;
		}

		inline bool operator<(const MaintainedPair& a, const MaintainedPair& b) noexcept
		{
			// Order by addresses (stable and cheap)
			if (a.infinite != b.infinite) {
				return std::less<InfiniteSpell*>{}(a.infinite, b.infinite);
			}
			return std::less<DebuffSpell*>{}(a.debuff, b.debuff);
			// Alternatively:
			// return std::tie(a.infinite, a.debuff) < std::tie(b.infinite, b.debuff);
		}
	}  // namespace Domain

	// ===== Catalogs / Repositories ==============================================

	class FormsRepository
	{
	public:
		static FormsRepository& Get();

		// Shared handles/keywords/globals
		RE::BGSEquipSlot* EquipSlotVoice{};
		RE::BGSKeyword* KywdMagicCloak{};
		RE::BGSKeyword* KywdMaintainedSpell{};
		RE::BGSKeyword* KywdExcludeFromSystem{};
		RE::SpellItem* SpelMagickaDebuffTemplate{};
		RE::TESGlobal* GlobMaintainModeEnabled{};
		RE::TESGlobal* GlobCleanupRequested{};
		RE::BGSListForm* FlstMaintainedSpellToggle{};
		RE::SpellItem* SpelMindCrush{};

		// Beast races
		const RE::TESRace* WerewolfBeastRace() const;
		const RE::TESRace* VampireBeastRace() const;

	private:
		mutable RE::FormID serial_{ 0 };

		FormsRepository();
		FormsRepository(const FormsRepository&) = delete;
		FormsRepository& operator=(const FormsRepository&) = delete;
	};

	// ===== Policy / Calculators ==================================================

	class SpellEligibilityPolicy
	{
	public:
		static bool IsMaintainable(RE::SpellItem* const& spell, RE::Actor* const& caster);
	};

	class UpkeepCostCalculator
	{
	public:
		static float Calculate(RE::SpellItem* const& baseSpell, RE::Actor* const& caster);
	};

	// ===== Factories / Builders ==================================================

	class SpellFactory
	{
	public:
		static RE::Effect* CloneEffectWithoutVisuals(const RE::Effect* src);
		static RE::SpellItem* CreateInfiniteFrom(RE::SpellItem* const& base, std::optional<RE::FormID> aFormID = std::nullopt);
		static RE::SpellItem* CreateDebuffFrom(RE::SpellItem* const& base, float const& magnitude, std::optional<RE::FormID> aFormID = std::nullopt);
	};

	class FXSilencer
	{
	public:
		// Disable FXPersist on effects that are safe to silence and return the ones we changed.
		static void SilenceSpellFX(Domain::MaintainedPair& pair);
		static void UnsilenceSpellFX(Domain::MaintainedPair& pair);
	};

	// ===== State / Registries ====================================================

	class MaintainedRegistry
	{
	public:
		static MaintainedRegistry& Get();

		// ===============================
		// Maintained spell tracking
		// ===============================
		void clear();
		bool empty();

		bool hasBase(RE::SpellItem* base);

		// Mutable access to pair
		Domain::MaintainedPair* getByBase(RE::SpellItem* base);

		void insert(RE::SpellItem* base, Domain::MaintainedPair pair);
		void eraseBase(RE::SpellItem* base);

		// Direct map access for iteration
		std::unordered_map<RE::SpellItem*, Domain::MaintainedPair>& map();

		// ===============================
		// Silenced spell policy
		// ===============================
		void clearSilencedSpells();

		void addSilencedSpell(const std::string& baseSpellName);
		void removeSilencedSpell(const std::string& baseSpellName);

		bool shouldSilenceSpell(const std::string& baseSpellName);
		bool shouldSilenceSpell(const RE::SpellItem* spell);

		std::unordered_set<std::string>& silencedSpells();

		// ===============================
		// Deferred cleanups
		// ===============================
		void deferDispel(RE::SpellItem* maintained, RE::SpellItem* base);
		bool isDeferred(RE::SpellItem* maintained, RE::SpellItem* base);

		void forEachDeferred(
			const std::function<void(RE::SpellItem*, RE::SpellItem*, bool& erase)>& fn);

	private:
		std::unordered_map<RE::SpellItem*, Domain::MaintainedPair> map_;
		std::set<std::pair<RE::SpellItem*, RE::SpellItem*>> deferred_;
		std::unordered_set<std::string> silencedSpells_;

		std::uint32_t LastMaxSummonCount_ = 1;
	};

	// ===== FormID Allocator ====================================================

	class Allocator
	{
	public:
		static constexpr RE::FormID FORMID_OFFSET_BASE = 0xFF03F000;

		static constexpr std::uint32_t MIN_LOCAL_ID = 1;
		static constexpr std::uint32_t MAX_LOCAL_ID = 64;
		static constexpr std::uint32_t TOTAL_IDS = 64;

		// ----------------------------
		// Allocation interface
		// ----------------------------

		static Allocator& Get();

		std::optional<RE::FormID> AllocateFormID();
		std::optional<RE::FormID> AllocateSpecificFormID(RE::FormID fullFormID);
		void FreeFormID(RE::FormID fullFormID);

		void ReconcileWithCache();

		bool IsAllocated(RE::FormID fullFormID) const;
		std::size_t GetFreeFormIDCount() const;
		void Clear();

	private:
		// ----------------------------
		// Bitmask helpers
		// ----------------------------

		static constexpr std::uint64_t FULL_MASK = ~0ull;

		std::uint64_t _allocatedMask{ 0 };

		static std::uint32_t FindFirstFreeIndex(std::uint64_t mask);
		static constexpr std::uint64_t BitForIndex(std::uint32_t index);

		void SetIndexAllocated(std::uint32_t index);
		void ClearIndexAllocated(std::uint32_t index);
		bool IsIndexAllocated(std::uint32_t index) const;

		void MarkReferenced(std::uint64_t& mask, RE::FormID fullFormID) const;

		// ----------------------------
		// ID mapping
		// ----------------------------

		static constexpr RE::FormID IndexToLocalID(std::uint32_t index);
		static constexpr std::uint32_t LocalIDToIndex(RE::FormID localID);
		static constexpr RE::FormID MakeFullFormID(RE::FormID localID);
		static constexpr RE::FormID ExtractLocalID(RE::FormID fullFormID);
		static constexpr bool IsInManagedRange(RE::FormID fullFormID);
	};

	class MaintainedEffectsCache
	{
	public:
		const std::unordered_map<RE::SpellItem*, std::vector<RE::ActiveEffect*>>&
			GetFor(RE::Actor* actor);
		void Clear();

	private:
		std::unordered_map<RE::SpellItem*, std::vector<RE::ActiveEffect*>> cache_{};
		ptrdiff_t lastCount_{ 0 };
		void rebuild(RE::Actor* actor);
	};

	// ===== Orchestration / Application Services =================================

	class EffectRestorer
	{
	public:
		// Adds an effect to be restored after a delay (seconds)
		static void Push(
			RE::Effect* effect,
			float delaySeconds = Maint::Config::kDefaultFXRestoreDelay);

		// Called every player update; decrements timers and restores FX when ready
		static void Update(float deltaSeconds);

		// Optional: clears all pending restores without restoring (safety / shutdown)
		static void Clear();

	private:
		struct RestoreEntry
		{
			RE::Effect* effect{ nullptr };
			float remaining{ 0.0f };  // seconds until kFXPersist is restored
		};

		// Container holding all pending FX restores
		static std::vector<RestoreEntry>& Pending();
	};

	class ExperienceService
	{
	public:
		static void AwardPlayerExperience(RE::PlayerCharacter* const& player);
	};

	class UpkeepSupervisor
	{
	public:
		static void ForceMaintainedSpellUpdate(RE::Actor* const& actor);
		static void CheckUpkeepValidity(RE::Actor* const& actor);

		static void UpdateConjureWatch(RE::Actor* actor);
		static void UpdateConjureRecasts(RE::Actor* player, float deltaSeconds);
		static bool TryRecastSummon(RE::Actor* actor, RE::SpellItem* spell);
		static void SetEvictionTick(RE::Actor* actor);

		static void ClearCache();
	private:
		static inline MaintainedEffectsCache cache_;

		static inline int evictionWindowTicks_ = 0;

		static inline std::unordered_set<RE::SpellItem*> evictionSnapshot_;
	};

	class MaintenanceOrchestrator
	{
	public:
		static void MaintainSpell(RE::SpellItem* const& baseSpell, RE::Actor* const& caster);
		static void PurgeAll();                // clear registry + FLST, delete temp forms
		static void BuildActiveSpellsCache();  // rebuild toggles + restore debuff magnitudes
		static void ApplySilencedFXPostLoad();
	};

	// ===== Hooks / Integration ===================================================

	class UpdatePCHook
	{
	public:
		static void Install();

	private:
		static void UpdatePCMod(RE::PlayerCharacter* pc, float delta);

		static inline REL::Relocation<decltype(UpdatePCMod)> UpdatePC;
		static inline std::atomic<float> TimerConjureWatch{ 0.0f };
		static inline std::atomic<float> TimerActiveEffCheck{ 0.0f };
		static inline std::atomic<float> TimerExperienceAward{ 0.0f };
	};

	// Legacy public C-style API (kept for external call sites if any)
	void ForceMaintainedSpellUpdate(RE::Actor* const&);
	void AwardPlayerExperience(RE::PlayerCharacter* const& player);
	void CheckUpkeepValidity(RE::Actor* const&);

	// ===== Lifecycle (messaging) =================================================

	void OnInit(SKSE::MessagingInterface::Message* const a_msg);
	bool Load();

}  // namespace MAINT

// Global shims (declarations) — optional
bool Load();
void OnInit(SKSE::MessagingInterface::Message* const);
