#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#define IMGUI_DEFINE_MATH_OPERATORS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#define MANAGER(T) T::Manager::GetSingleton()

#include "RE/Skyrim.h"
#include "REX/REX/Singleton.h"
#include "SKSE/SKSE.h"

#include <codecvt>
#include <dxgi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <ClibUtil/RNG.hpp>
#include <ClibUtil/simpleINI.hpp>
#include <ClibUtil/string.hpp>

#include <DirectXMath.h>
#include <DirectXTex.h>
#include <ankerl/unordered_dense.h>
#include <freetype/freetype.h>
#include <glaze/glaze.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <srell.hpp>
#include <xbyak/xbyak.h>

#include "ImGui/Backend/imgui_impl_win32.h"
#include "imgui_internal.h"
#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_dx11.h>
#include <imgui_stdlib.h>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;
using namespace clib_util;
using namespace string::literals;
using namespace RE::literals;

namespace logger = SKSE::log;

using EventResult = RE::BSEventNotifyControl;

using KEY = RE::BSWin32KeyboardDevice::Key;
using GAMEPAD_DIRECTX = RE::BSWin32GamepadDevice::Key;
using GAMEPAD_ORBIS = RE::BSPCOrbisGamepadDevice::Key;
using MOUSE = RE::BSWin32MouseDevice::Key;

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

template <class K, class D>
using Map = ankerl::unordered_dense::map<K, D>;

struct string_hash
{
	using is_transparent = void;  // enable heterogeneous overloads
	using is_avalanching = void;  // mark class as high quality avalanching hash

	[[nodiscard]] std::uint64_t operator()(std::string_view str) const noexcept
	{
		return ankerl::unordered_dense::hash<std::string_view>{}(str);
	}
};

template <class D>
using StringMap = ankerl::unordered_dense::map<std::string, D, string_hash, std::equal_to<>>;

namespace stl
{
	using namespace SKSE::stl;

	// a_write_branch replaces a tail-call JMP (E9) instead of a CALL (E8). The VR build
	// needs it for the PopHUDMode site that the engine compiled as a tail call (see Hooks.cpp).
	template <class T>
	void write_thunk_call(std::uintptr_t a_src, bool a_write_branch = false)
	{
		auto& trampoline = SKSE::GetTrampoline();
		T::func = a_write_branch ?
		              trampoline.write_branch<5>(a_src, T::thunk) :
		              trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(T::idx, T::thunk);
	}

	inline bool IsVR()
	{
		return REL::Module::IsVR();
	}
}

// Single cross-runtime CommonLibSSE-NG DLL: offsets and engine fields resolve at load time.
// OFFSET_3 supplies a distinct VR byte offset; OFFSET defaults VR to the SE value.
#define OFFSET(se, ae) REL::VariantOffset(se, ae, se)
#define OFFSET_3(se, ae, vr) REL::VariantOffset(se, ae, vr)

// Engine fields that NG keeps in RUNTIME_DATA blocks are reached through its
// runtime-detecting accessors. These aliases name the accessor for each struct.
#define RENDERER_DATA(a_obj) (a_obj)->GetRuntimeData()
#define MAIN_DATA(a_obj)     (a_obj)->GetRuntimeData()

#include "Compatibility.h"
#include "Translation.h"
#include "Version.h"
