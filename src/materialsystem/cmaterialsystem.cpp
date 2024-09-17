//===========================================================================//
//
// Purpose: 
//
//===========================================================================//
#include "core/stdafx.h"
#include "tier0/crashhandler.h"
#include "tier0/commandline.h"
#include "tier1/cvar.h"
#include "tier1/keyvalues.h"
#include "rtech/pak/pakstate.h"
#include "engine/cmodel_bsp.h"
#include "engine/sys_engine.h"
#include "engine/sys_dll2.h"
#include "geforce/reflex.h"
#include "radeon/antilag.h"
#ifndef MATERIALSYSTEM_NODX
#include "gameui/imgui_system.h"
#include "materialsystem/cmaterialglue.h"
#endif // !MATERIALSYSTEM_NODX
#include "materialsystem/cmaterialsystem.h"

#ifndef MATERIALSYSTEM_NODX
PCLSTATS_DEFINE()
#endif // MATERIALSYSTEM_NODX

bool CMaterialSystem::Connect(CMaterialSystem* thisptr, const CreateInterfaceFn factory)
{
	const bool result = CMaterialSystem__Connect(thisptr, factory);
	return result;
}

void CMaterialSystem::Disconnect(CMaterialSystem* thisptr)
{
	CMaterialSystem__Disconnect(thisptr);
}

#ifndef MATERIALSYSTEM_NODX
static bool s_useLowLatency = false;
#endif

//-----------------------------------------------------------------------------
// Purpose: initialization of the material system
//-----------------------------------------------------------------------------
InitReturnVal_t CMaterialSystem::Init(CMaterialSystem* thisptr)
{
#ifdef MATERIALSYSTEM_NODX
	// Only load the 'startup.rpak' file, as 'common_early.rpak' has assets
	// that references assets in 'startup.rpak'.
	PakHandle_t pakHandle = g_pakLoadApi->LoadAsync("startup.rpak", AlignedMemAlloc(), 5, 0);
	g_pakLoadApi->WaitAsync(pakHandle, nullptr);

	// Trick: return INIT_FAILED to disable the loading of hardware
	// configuration data, since we don't need it on the dedi.
	return INIT_FAILED;
#else
	// Initialize as usual.
	s_useLowLatency = !CommandLine()->CheckParm("-gfx_disableLowLatency");

	GeForce_EnableLowLatencySDK(s_useLowLatency);
	Radeon_EnableLowLatencySDK(s_useLowLatency);

	if (s_useLowLatency)
	{
		GeForce_InitLowLatencySDK();
		Radeon_InitLowLatencySDK();

		PCLSTATS_INIT(0);
		g_PCLStatsAvailable = true;
	}

	return CMaterialSystem__Init(thisptr);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: shutdown of the material system
//-----------------------------------------------------------------------------
int CMaterialSystem::Shutdown(CMaterialSystem* thisptr)
{
#ifndef MATERIALSYSTEM_NODX
	if (s_useLowLatency)
	{
		if (g_PCLStatsAvailable)
			PCLSTATS_SHUTDOWN();

		Radeon_ShutdownLowLatencySDK();
		GeForce_ShutdownLowLatencySDK();
	}
#endif

	return CMaterialSystem__Shutdown(thisptr);
}

#ifndef MATERIALSYSTEM_NODX
//---------------------------------------------------------------------------------
// Purpose: loads and processes STBSP files
// (overrides level name if stbsp field has value in prerequisites file)
// Input  : *pszLevelName - 
//---------------------------------------------------------------------------------
void StreamDB_Init(const char* pszLevelName)
{
	KeyValues* pSettingsKV = Mod_GetLevelSettings(pszLevelName);

	if (pSettingsKV)
	{
		KeyValues* pStreamKV = pSettingsKV->FindKey("StreamDB");

		if (pStreamKV)
		{
			const char* pszColumnName = pStreamKV->GetString();
			Msg(eDLL_T::MS, "StreamDB_Init: Loading override STBSP file '%s.stbsp'\n", pszColumnName);

			v_StreamDB_Init(pszColumnName);
			return;
		}
	}

	Msg(eDLL_T::MS, "StreamDB_Init: Loading STBSP file '%s.stbsp'\n", pszLevelName);
	v_StreamDB_Init(pszLevelName);
}

//---------------------------------------------------------------------------------
// Purpose: draw frame
//---------------------------------------------------------------------------------
void* __fastcall DispatchDrawCall(int64_t a1, uint64_t a2, int a3, int a4, int64_t a5, int a6, uint8_t a7, int64_t a8, uint32_t a9, uint32_t a10, int a11, __m128* a12, int a13, int64_t a14)
{
	// This only happens when the BSP is in a horrible condition (bad depth buffer draw calls!)
	// but allows you to load BSP's with virtually all missing shaders/materials and models 
	// being replaced with 'material_for_aspect/error.rpak' and 'mdl/error.rmdl'.
	if (!*s_pRenderContext)
		return nullptr;

	return v_DispatchDrawCall(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

//---------------------------------------------------------------------------------
// Purpose: run IDXGISwapChain::Present
//---------------------------------------------------------------------------------
ssize_t SpinPresent(void)
{
	ImguiSystem()->RenderFrame();

	const ssize_t val = v_SpinPresent();
	return val;
}

void* CMaterialSystem::SwapBuffers(CMaterialSystem* pMatSys)
{
	ImguiSystem()->SampleFrame();
	ImguiSystem()->SwapBuffers();

	return CMaterialSystem__SwapBuffers(pMatSys);
}

//-----------------------------------------------------------------------------
// Purpose: finds a material
// Input  : *pMatSys - 
//			*pMaterialName - 
//			nMaterialType - 
//			nUnk - 
//			bComplain - 
// Output : pointer to material
//-----------------------------------------------------------------------------
static ConVar mat_alwaysComplain("mat_alwaysComplain", "0", FCVAR_RELEASE | FCVAR_MATERIAL_SYSTEM_THREAD, "Always complain when a material is missing");

CMaterialGlue* CMaterialSystem::FindMaterialEx(CMaterialSystem* pMatSys, const char* pMaterialName, uint8_t nMaterialType, int nUnk, bool bComplain)
{
	CMaterialGlue* pMaterial = CMaterialSystem__FindMaterialEx(pMatSys, pMaterialName, nMaterialType, nUnk, bComplain);

	if ((bComplain || mat_alwaysComplain.GetBool()) && pMaterial->IsErrorMaterial())
	{
		Error(eDLL_T::MS, NO_ERROR, "Material \"%s\" not found; replacing with \"%s\".\n", pMaterialName, pMaterial->GetName());
	}
	return pMaterial;
}

//-----------------------------------------------------------------------------
// Purpose: get screen size
// Input  : *pMatSys - 
// Output : Vector2D screen size
//-----------------------------------------------------------------------------
Vector2D CMaterialSystem::GetScreenSize(CMaterialSystem* pMatSys)
{
	Vector2D vecScreenSize;

	CMaterialSystem__GetScreenSize(pMatSys, &vecScreenSize.x, &vecScreenSize.y);

	return vecScreenSize;
}
#endif // !MATERIALSYSTEM_NODX

///////////////////////////////////////////////////////////////////////////////
void VMaterialSystem::Detour(const bool bAttach) const
{
	DetourSetup(&CMaterialSystem__Init, &CMaterialSystem::Init, bAttach);
	DetourSetup(&CMaterialSystem__Shutdown, &CMaterialSystem::Shutdown, bAttach);

	DetourSetup(&CMaterialSystem__Connect, &CMaterialSystem::Connect, bAttach);
	DetourSetup(&CMaterialSystem__Disconnect, &CMaterialSystem::Disconnect, bAttach);

#ifndef MATERIALSYSTEM_NODX
	DetourSetup(&CMaterialSystem__SwapBuffers, &CMaterialSystem::SwapBuffers, bAttach);
	DetourSetup(&CMaterialSystem__FindMaterialEx, &CMaterialSystem::FindMaterialEx, bAttach);

	DetourSetup(&v_StreamDB_Init, &StreamDB_Init, bAttach);
	DetourSetup(&v_DispatchDrawCall, &DispatchDrawCall, bAttach);
	DetourSetup(&v_SpinPresent, &SpinPresent, bAttach);
#endif // !MATERIALSYSTEM_NODX
}
