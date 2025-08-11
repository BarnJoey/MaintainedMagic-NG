#pragma once

#pragma warning(push)
#if defined(FALLOUT4)
#	include "F4SE/F4SE.h"
#	include "RE/Fallout.h"
#	define SKSE F4SE
#	define SKSEAPI F4SEAPI
#	define SKSEPlugin_Load F4SEPlugin_Load
#	define SKSEPlugin_Query F4SEPlugin_Query
#else
#	define SKSE_SUPPORT_XBYAK
#	include "RE/Skyrim.h"
#	include "SKSE/SKSE.h"
#	include <xbyak/xbyak.h>
#endif
#pragma warning(pop)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>  // file sink (used in Debug & Release)
#include <spdlog/sinks/msvc_sink.h>        // VS Output (Debug-only mirror)
#include <spdlog/spdlog.h>

using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(14);
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, std::size_t idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <std::size_t idx, class T>
	void write_vfunc(REL::VariantID id)
	{
		REL::Relocation<std::uintptr_t> vtbl{ id };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <class T>
	void write_thunk_jmp(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(14);
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_branch<5>(a_src, T::thunk);
	}
}

namespace logger = SKSE::log;
namespace WinAPI = SKSE::WinAPI;

namespace util
{
	using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"

// Forward decls provided by your sources
bool Load();
void OnInit(SKSE::MessagingInterface::Message* const a_msg);

namespace
{
	inline std::shared_ptr<spdlog::logger> MakeUnifiedLogger()
	{
		// Resolve the canonical SKSE logging folder (works across SE/AE/VR)
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= std::format("{}.log", Plugin::NAME);  // keep the same filename convention

		// Always write to file in both Debug and Release
		auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), /*truncate=*/true);

		std::vector<spdlog::sink_ptr> sinks;
		sinks.emplace_back(fileSink);

#ifndef NDEBUG
		// Mirror logs to the VS Output window in Debug
		sinks.emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

		auto loggerPtr = std::make_shared<spdlog::logger>("global log"s, sinks.begin(), sinks.end());

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		loggerPtr->set_level(level);
		loggerPtr->flush_on(spdlog::level::info);
		loggerPtr->set_pattern("%v"s);  // match your old pattern

		return loggerPtr;
	}
}

inline void InitializeLog()
{
	auto loggerPtr = MakeUnifiedLogger();
	spdlog::set_default_logger(std::move(loggerPtr));
	spdlog::info("Logging initialized for {}", Plugin::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!WinAPI::IsDebuggerPresent()) {};
#endif

	InitializeLog();
	logger::info("Loaded plugin");
	SKSE::Init(a_skse);

	SKSE::GetMessagingInterface()->RegisterListener(OnInit);

	return Load();
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData v;
	v.PluginName(Plugin::NAME.data());
	v.PluginVersion(Plugin::VERSION);
	v.UsesAddressLibrary(true);
	v.HasNoStructUse();
	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
	pluginInfo->name = SKSEPlugin_Version.pluginName;
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->version = SKSEPlugin_Version.pluginVersion;
	return true;
}
