#pragma once

#include "Bimap.hpp"
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

	// ===== Utilities =============================================================

	[[nodiscard]] RE::FormID lexical_cast_hex_to_formid(std::string_view hex);

	// A tiny threadsafe queue (keeps original semantics: references escape locks).
	template <class T>
	class ConcurrentQueue
	{
		mutable std::mutex m_;
		std::queue<T> q_;

	public:
		void push(const T& v)
		{
			std::lock_guard _{ m_ };
			q_.push(v);
		}
		[[nodiscard]] bool empty() const
		{
			std::lock_guard _{ m_ };
			return q_.empty();
		}
		T& front()
		{
			std::lock_guard _{ m_ };
			return q_.front();
		}
		const T& front() const
		{
			std::lock_guard _{ m_ };
			return q_.front();
		}
		void pop()
		{
			std::lock_guard _{ m_ };
			q_.pop();
		}
		bool try_pop(T& out)
		{
			std::lock_guard _{ m_ };
			if (q_.empty())
				return false;
			out = std::move(q_.front());
			q_.pop();
			return true;
		}
	};

	// ===== Global config values (backed by INI) ==================================

	namespace Config
	{
		inline constexpr const char* CONFIG_FILE = "Data/SKSE/Plugins/MaintainedMagicNG.Config.ini";

		inline std::string SAVES_PATH = "disabled";
		inline bool DoSilenceFX = false;
		inline long CostBaseDuration = 60;
		inline float CostReductionExponent = 0.0f;
		inline bool AllowBoundWeapons = true;
		inline float MaintainedExpMultiplier = 1.0f;
		inline bool CleanupMissingSaves = false;
		inline bool InstantDispel = true;

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
		struct MaintainedPair
		{
			InfiniteSpell* infinite{};
			DebuffSpell* debuff{};
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
		static RE::SpellItem* CreateInfiniteFrom(RE::SpellItem* const& base, std::optional<RE::FormID> aFormID = std::nullopt);
		static RE::SpellItem* CreateDebuffFrom(RE::SpellItem* const& base, float const& magnitude, std::optional<RE::FormID> aFormID = std::nullopt);
	};

	class FXSilencer
	{
	public:
		// Disable FXPersist on effects that are safe to silence and return the ones we changed.
		static std::vector<RE::Effect*> SilenceSpellFX(RE::SpellItem* const& spell);
	};

	// ===== State / Registries ====================================================

	class MaintainedRegistry
	{
	public:
		static MaintainedRegistry& Get();

		void clear();
		bool empty() const;

		bool hasBase(RE::SpellItem* base) const;
		std::optional<Domain::MaintainedPair> getByBase(RE::SpellItem* base) const;

		void insert(RE::SpellItem* base, Domain::MaintainedPair pair);
		void eraseBase(RE::SpellItem* base);

		const BiMap<RE::SpellItem*, Domain::MaintainedPair>::forward_map_t& map() const;

		// Deferred cleanups for bound weapons
		void deferDispel(RE::SpellItem* maintained, RE::SpellItem* base);
		bool isDeferred(RE::SpellItem* maintained, RE::SpellItem* base) const;
		void forEachDeferred(const std::function<void(RE::SpellItem*, RE::SpellItem*, bool& erase)>& fn);

	private:
		BiMap<RE::SpellItem*, Domain::MaintainedPair> map_;
		std::set<std::pair<RE::SpellItem*, RE::SpellItem*>> deferred_;
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
		static void Push(RE::Effect* const& e);
		static void DrainAndRestore();  // called by player update hook
	private:
		static ConcurrentQueue<RE::Effect*>& Q();
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
		static void ClearCache();
	private:
		static inline MaintainedEffectsCache cache_;
	};

	class MaintenanceOrchestrator
	{
	public:
		static void MaintainSpell(RE::SpellItem* const& baseSpell, RE::Actor* const& caster);
		static void PurgeAll();                // clear registry + FLST, delete temp forms
		static void BuildActiveSpellsCache();  // rebuild toggles + restore debuff magnitudes
	};

	// ===== Hooks / Integration ===================================================

	class UpdatePCHook
	{
	public:
		static void Install();
		static void ResetEffCheckTimer();
		static void PushFXRestore(RE::Effect* const& eff);

	private:
		static void UpdatePCMod(RE::PlayerCharacter* pc, float delta);

		static inline REL::Relocation<decltype(UpdatePCMod)> UpdatePC;
		static inline std::atomic<float> TimerActiveEffCheck{ 0.0f };
		static inline std::atomic<float> TimerExperienceAward{ 0.0f };
		static inline ConcurrentQueue<RE::Effect*> EffectRestorationQueue{};
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