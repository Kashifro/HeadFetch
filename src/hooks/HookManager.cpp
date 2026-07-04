#include "hooks/HookManager.h"

#include "mod/HeadFetchMod.h"
#include "signatures/Signatures.h"

#include <EGL/egl.h>
// Header path inferred from preloader-android's Mod.hpp/Signature.hpp
// naming convention; not directly confirmed. Adjust if the real path
// differs (see HOOK_REPORT.md).
#include <pl/cpp/Hook.hpp>
#include <pl/cpp/Mod.hpp>
#include <pl/cpp/PreloaderInput.hpp>
#include <pl/cpp/Signature.hpp>

namespace headfetch::Hooks {

namespace {

using HudRenderFn = void (*)(void*, void*, void*, void*, int);
HudRenderFn g_origHudRender = nullptr;
bool g_hudHookInstalled = false;

void hudRenderDetour(void* self, void* ctx, void* client, void* owner, int pass) {
	HeadFetchMod::getInstance().onHudRender();
	if (g_origHudRender) { g_origHudRender(self, ctx, client, owner, pass); }
}

using PacketReadFn = void* (*)(void*, void*, void*);
PacketReadFn g_origPacketRead = nullptr;
bool g_packetHookInstalled = false;

void* packetReadDetour(void* result, void* self, void* stream) {
	void* ret = g_origPacketRead ? g_origPacketRead(result, self, stream) : result;
	HeadFetchMod::getInstance().onPacketRead(self);
	return ret;
}

using EglSwapFn = EGLBoolean (*)(EGLDisplay, EGLSurface);
EglSwapFn g_origEglSwap = nullptr;
bool g_swapHookInstalled = false;

EGLBoolean eglSwapDetour(EGLDisplay dpy, EGLSurface surface) {
	HeadFetchMod::getInstance().onSwapBuffers(dpy, surface);
	return g_origEglSwap ? g_origEglSwap(dpy, surface) : EGL_FALSE;
}

using EglMakeCurrentFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
EglMakeCurrentFn g_origEglMakeCurrent = nullptr;
bool g_makeCurrentHookInstalled = false;

EGLBoolean eglMakeCurrentDetour(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
	HeadFetchMod::getInstance().onMakeCurrent(dpy, draw, ctx);
	return g_origEglMakeCurrent ? g_origEglMakeCurrent(dpy, draw, read, ctx) : EGL_FALSE;
}

bool onTouch(int action, int pointerId, float x, float y) {
	return HeadFetchMod::getInstance().onTouch(action, pointerId, x, y);
}

bool onKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown) {
	return HeadFetchMod::getInstance().onKeyEvent(keyCode, unicodeChar, isKeyDown);
}

} // namespace

bool installAll(pl::mod::NativeMod& self) {
	bool requiredOk = true;

	if (auto addr = Signatures::resolveHudCursorRender(); addr) {
		auto rc = pl::hook::hook(
			reinterpret_cast<void*>(addr),
			reinterpret_cast<void*>(&hudRenderDetour),
			reinterpret_cast<void**>(&g_origHudRender),
			pl::hook::PriorityNormal);
		g_hudHookInstalled = (rc == 0);
		if (!g_hudHookInstalled) {
			self.getLogger().warn("Failed to install HudCursorRenderer::render hook (rc={})", rc);
		}
	} else {
		self.getLogger().warn("Could not resolve HudCursorRenderer::render; panel will stay visible longer after leaving the HUD");
	}

	if (auto addr = Signatures::resolvePlayerListPacketRead(); addr) {
		auto rc = pl::hook::hook(
			reinterpret_cast<void*>(addr),
			reinterpret_cast<void*>(&packetReadDetour),
			reinterpret_cast<void**>(&g_origPacketRead),
			pl::hook::PriorityNormal);
		g_packetHookInstalled = (rc == 0);
		if (!g_packetHookInstalled) {
			self.getLogger().error("Failed to install PlayerListPacket read hook (rc={})", rc);
			requiredOk = false;
		}
	} else {
		self.getLogger().error("Could not resolve the PlayerListPacket read handler; no player list will be available");
		requiredOk = false;
	}

	if (auto addr = Signatures::resolveEglSwapBuffers(); addr) {
		auto rc = pl::hook::hook(
			reinterpret_cast<void*>(addr),
			reinterpret_cast<void*>(&eglSwapDetour),
			reinterpret_cast<void**>(&g_origEglSwap),
			pl::hook::PriorityNormal);
		g_swapHookInstalled = (rc == 0);
		if (!g_swapHookInstalled) {
			self.getLogger().error("Failed to install eglSwapBuffers hook (rc={})", rc);
			requiredOk = false;
		}
	} else {
		self.getLogger().error("Could not resolve eglSwapBuffers in libEGL.so");
		requiredOk = false;
	}

	if (auto addr = Signatures::resolveEglMakeCurrent(); addr) {
		auto rc = pl::hook::hook(
			reinterpret_cast<void*>(addr),
			reinterpret_cast<void*>(&eglMakeCurrentDetour),
			reinterpret_cast<void**>(&g_origEglMakeCurrent),
			pl::hook::PriorityNormal);
		g_makeCurrentHookInstalled = (rc == 0);
		if (!g_makeCurrentHookInstalled) {
			self.getLogger().error("Failed to install eglMakeCurrent hook (rc={})", rc);
			requiredOk = false;
		}
	} else {
		self.getLogger().error("Could not resolve eglMakeCurrent in libEGL.so");
		requiredOk = false;
	}

	// NOTE: GetPreloaderInput()'s exact namespace was not confirmed against
	// the preloader-android header at write time; the API docs only show it
	// as a bare call. If the build fails to resolve this symbol, check
	// pl/cpp/PreloaderInput.hpp for the enclosing namespace and qualify
	// accordingly (see HOOK_REPORT.md).
	if (auto* input = GetPreloaderInput(); input) {
		input->RegisterTouchCallback(&onTouch);
		input->RegisterKeyEventCallback(&onKeyEvent);
	} else {
		self.getLogger().warn("PreloaderInput interface unavailable; the on-screen toggle button and Tab key will not work");
	}

	return requiredOk;
}

// The documented C API is `pl_unhook(PLFuncPtr target, PLFuncPtr detour)`;
// the C++ wrapper's exact signature was not shown in the fetched docs. This
// assumes a mirrored `pl::hook::unhook(void*, void*)` matching the `hook()`
// call shape used above -- verify against pl/cpp/Hook.hpp and switch to the
// `pl_unhook` C API directly (with the same reinterpret_cast<void*> style)
// if it does not compile.
void removeAll(pl::mod::NativeMod& self) {
	if (g_hudHookInstalled) {
		if (auto addr = Signatures::resolveHudCursorRender(); addr) {
			pl::hook::unhook(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(&hudRenderDetour));
		}
		g_hudHookInstalled = false;
	}
	if (g_packetHookInstalled) {
		if (auto addr = Signatures::resolvePlayerListPacketRead(); addr) {
			pl::hook::unhook(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(&packetReadDetour));
		}
		g_packetHookInstalled = false;
	}
	if (g_swapHookInstalled) {
		if (auto addr = Signatures::resolveEglSwapBuffers(); addr) {
			pl::hook::unhook(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(&eglSwapDetour));
		}
		g_swapHookInstalled = false;
	}
	if (g_makeCurrentHookInstalled) {
		if (auto addr = Signatures::resolveEglMakeCurrent(); addr) {
			pl::hook::unhook(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(&eglMakeCurrentDetour));
		}
		g_makeCurrentHookInstalled = false;
	}
	// PreloaderInput has no unregister API (see HOOK_REPORT.md); the
	// callbacks stay registered but early-out safely since HeadFetchMod's
	// state is idle after disable().
	(void)self;
}

} // namespace headfetch::Hooks
