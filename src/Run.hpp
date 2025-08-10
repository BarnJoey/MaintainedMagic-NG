#pragma once

#include "Bimap.h"
#include <SimpleIni.h>

namespace MAINT
{
	void ForceMaintainedSpellUpdate(RE::Actor* const&);
	void AwardPlayerExperience(RE::PlayerCharacter* const& player);
	void CheckUpkeepValidity(RE::Actor* const&);

	const auto lexical_cast_hex_to_formid(const std::string& hex_string)
	{
		if (hex_string.substr(0, 2) != "0x") {
			throw std::invalid_argument("Input string is not a valid hexadecimal format (should start with '0x').");
		}

		std::istringstream iss(hex_string.substr(2));
		uint32_t result;
		if (!(iss >> std::hex >> result)) {
			throw std::invalid_argument("Failed to convert hexadecimal string to integer.");
		}
		return static_cast<RE::FormID>(result);
	}

	template <typename Data>
	class ConcurrentQueue
	{
	private:
		std::queue<Data> theQueue;
		mutable std::mutex theMutex;

	public:
		void push(const Data& data)
		{
			std::lock_guard<std::mutex> guard(theMutex);
			theQueue.push(data);
		}

		bool empty() const
		{
			std::lock_guard<std::mutex> guard(theMutex);
			return theQueue.empty();
		}

		Data& front()
		{
			std::lock_guard<std::mutex> guard(theMutex);
			return theQueue.front();
		}

		Data const& front() const
		{
			std::lock_guard<std::mutex> guard(theMutex);
			return theQueue.front();
		}

		void pop()
		{
			std::lock_guard<std::mutex> guard(theMutex);
			theQueue.pop();
		}
	};

	namespace CONFIG
	{
		std::string const& MAP_FILE		= "Data/SKSE/Plugins/MaintainedMagicNG.ini";
		std::string const& CONFIG_FILE	= "Data/SKSE/Plugins/MaintainedMagicNG.Config.ini";
		inline bool DoSilenceFX;
		inline long CostBaseDuration;
		inline float CostReductionExponent; 
		inline bool AllowBoundWeapons;
		class ConfigBase
		{
		private:
			static inline std::map<std::string, ConfigBase*> MultiConfigMap;

			CSimpleIniA Ini;
			std::string IniPath;
			ConfigBase(std::string const& iniPath) :
				IniPath(iniPath)
			{
				Ini.SetUnicode();
				Ini.LoadFile(iniPath.c_str());
			}

		public:
			static ConfigBase* GetSingleton(std::string const& iniPath)
			{
				if (!MultiConfigMap.contains(iniPath)) {
					logger::info("Load INI: {}", iniPath);
					ConfigBase* instance = new ConfigBase(iniPath);
					MultiConfigMap[iniPath] = instance;
				}
				auto const& ret = MultiConfigMap.at(iniPath);
				return ret;
			}

			bool HasKey(const std::string& section, const std::string& key)
			{
				return Ini.KeyExists(section.c_str(), key.c_str());
			}

			bool HasSection(const std::string& section)
			{
				return Ini.SectionExists(section.c_str());
			}

			const std::vector<std::pair<std::string, std::string>> GetAllKeyValuePairs(const std::string& section) const
			{
				std::vector<std::pair<std::string, std::string>> ret;
				CSimpleIniA::TNamesDepend keys;
				Ini.GetAllKeys(section.c_str(), keys);
				for (auto& key : keys) {
					auto val = Ini.GetValue(section.c_str(), key.pItem);
					ret.push_back({ key.pItem, val });
				}

				return ret;
			}

			void DeleteSection(const std::string& section)
			{
				Ini.Delete(section.c_str(), nullptr, true);
			}

			void DeleteKey(const std::string& section, const std::string& key)
			{
				Ini.Delete(section.c_str(), key.c_str());
			}

			std::string GetValue(const std::string& section, const std::string& key)
			{
				return Ini.GetValue(section.c_str(), key.c_str());
			}

			long GetLongValue(const std::string& section, const std::string& key)
			{
				return Ini.GetLongValue(section.c_str(), key.c_str());
			}

			bool GetBoolValue(const std::string& section, const std::string& key)
			{
				return Ini.GetBoolValue(section.c_str(), key.c_str());
			}

			double GetDoubleValue(const std::string& section, const std::string& key)
			{
				return Ini.GetDoubleValue(section.c_str(), key.c_str());
			}

			void SetValue(const std::string& section, const std::string& key, const std::string& value, const std::string& comment = std::string())
			{
				Ini.SetValue(section.c_str(), key.c_str(), value.c_str(), comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void SetBoolValue(const std::string& section, const std::string& key, const bool value, const std::string& comment = std::string())
			{
				Ini.SetBoolValue(section.c_str(), key.c_str(), value, comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void SetLongValue(const std::string& section, const std::string& key, const long value, const std::string& comment = std::string())
			{
				Ini.SetLongValue(section.c_str(), key.c_str(), value, comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void SetDoubleValue(const std::string& section, const std::string& key, const double value, const std::string& comment = std::string())
			{
				Ini.SetDoubleValue(section.c_str(), key.c_str(), value, comment.length() > 0 ? comment.c_str() : (const char*)0);
			}

			void Save()
			{
				Ini.SaveFile(IniPath.c_str());
			}

			ConfigBase(ConfigBase const&) = delete;
			void operator=(ConfigBase const&) = delete;
		};
	}

	namespace CACHE
	{
		typedef RE::SpellItem InfiniteSpell;
		typedef RE::SpellItem DebuffSpell;
		typedef std::pair<InfiniteSpell*, DebuffSpell*> MaintainedSpell;

		inline BiMap<RE::SpellItem*, MaintainedSpell> SpellToMaintainedSpell;
		inline std::set<std::pair<RE::SpellItem*, RE::SpellItem*>> DeferredDispelList;
	}

	class FORMS
	{
	public:
		static constexpr RE::FormID FORMID_OFFSET_BASE = 0xFF03F000;
		RE::FormID CurrentOffset;

		void SetOffset(RE::FormID offset)
		{
			CurrentOffset = offset;
		}
		void LoadOffset(const CONFIG::ConfigBase* config, const std::string& saveFile)
		{
			constexpr auto getLargestFormIDFromPair = [](const std::string& part) -> RE::FormID {
				std::size_t tildePos = part.find("~");

				if (tildePos == std::string::npos)
					return 0x0;
				try {
					auto leftID = lexical_cast_hex_to_formid(part.substr(0, tildePos));
					auto rightID = lexical_cast_hex_to_formid(part.substr(tildePos + 1));
					return leftID > rightID ? leftID : rightID;
				} catch (...) {
					return 0x0;
				}
			};

			RE::FormID off = 0x0;
			for (const auto& [k, v] : config->GetAllKeyValuePairs(std::format("MAP:{}", saveFile))) {
				auto cur = getLargestFormIDFromPair(v);
				if (cur > off)
					off = cur;
			}
			off &= ~FORMID_OFFSET_BASE;
			CurrentOffset = off;
			logger::info("Local OFFSET: 0x{:08X}", CurrentOffset);
			logger::info("Global OFFSET: 0x{:08X}", FORMID_OFFSET_BASE);
		}
		RE::FormID NextFormID() const
		{
			static RE::FormID current = 0;
			return FORMID_OFFSET_BASE + (++current) + CurrentOffset;
		}
		static FORMS& GetSingleton()
		{
			static FORMS instance;
			return instance;
		}

		//RE::BGSEquipSlot* EquipSlotRight = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F42);
		RE::BGSEquipSlot* EquipSlotVoice = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x25BEE);
		RE::BGSKeyword* KywdMagicCloak = RE::TESForm::LookupByID<RE::BGSKeyword>(0xB62E4);
		RE::BGSKeyword* KywdMaintainedSpell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x801, "MaintainedMagic.esp"sv);
		RE::BGSKeyword* KywdExcludeFromSystem = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x80A, "MaintainedMagic.esp"sv);
		RE::SpellItem* SpelMagickaDebuffTemplate = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(0x802, "MaintainedMagic.esp"sv);
		RE::TESGlobal* GlobMaintainModeEnabled = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x805, "MaintainedMagic.esp"sv);
		RE::BGSListForm* FlstMaintainedSpellToggle = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSListForm>(0x80B, "MaintainedMagic.esp"sv);
		RE::SpellItem* SpelMindCrush = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(0x80D, "MaintainedMagic.esp"sv);

	private:
		// Private constructor to prevent instantiation
		FORMS() {}

		// Delete copy constructor and assignment operator to prevent cloning
		FORMS(const FORMS&) = delete;
		FORMS& operator=(const FORMS&) = delete;
	};

	class UpdatePCHook
	{
	public:
		static void Install()
		{
			REL::Relocation<std::uintptr_t> pcVTable{ RE::VTABLE_PlayerCharacter[0] };
			UpdatePC = pcVTable.write_vfunc(0xAD, UpdatePCMod);
		}

		static void ResetEffCheckTimer()
		{
			TimerActiveEffCheck = 0.0f;
		}

		static void PushFXRestore(RE::Effect* const& eff)
		{
			EffectRestorationQueue.push(eff);
			ResetEffCheckTimer();
		}

	private:
		UpdatePCHook()
		{
			TimerActiveEffCheck = TimerExperienceAward = 0.0f;
		}

		static void UpdatePCMod(RE::PlayerCharacter* pc, float delta)
		{
			UpdatePC(pc, delta);
			TimerActiveEffCheck += delta;
			TimerExperienceAward += delta;
			if (TimerActiveEffCheck >= 0.50f) {
				MAINT::ForceMaintainedSpellUpdate(pc);
				MAINT::CheckUpkeepValidity(pc);
				while (!EffectRestorationQueue.empty()) {
					EffectRestorationQueue.front()->baseEffect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kFXPersist);
					EffectRestorationQueue.pop();
				}
				TimerActiveEffCheck = 0.0f;
			}
			if (TimerExperienceAward >= 300) {
				MAINT::AwardPlayerExperience(pc);
				TimerExperienceAward = 0.0f;
			}
		}

		static inline REL::Relocation<decltype(UpdatePCMod)> UpdatePC;

		static inline std::atomic<float> TimerActiveEffCheck;
		static inline std::atomic<float> TimerExperienceAward;
		static inline ConcurrentQueue<RE::Effect*> EffectRestorationQueue;
	};
}
