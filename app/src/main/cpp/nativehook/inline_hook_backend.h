/* SPDX-License-Identifier: AGPL-3.0-or-later */
/*
 * NativeHookRuntime inline hook backend: the LSPosed/Vector NativeAPI boundary.
 *
 * LSPosed and Vector inject the same two entry points (hookFunc/unhookFunc) at
 * native_init time - the `NativeAPIEntries` shape MiuiBackGestureHook
 * formalizes in its native_api.h (Apache-2.0). This wrapper is deliberately
 * independent of which loader supplies them:
 *  - a backend that reports success without a continuation pointer has left
 *    the target branching through an out-parameter we do not own; the caller
 *    must treat the slot as refused, not installed (hook_bank.h enforces it);
 *  - trampoline pages of file-backed images can be reclaimed and refilled by
 *    the kernel; registering them with the optional page guard (page_guard.h)
 *    before installation closes the reclaim-vs-patch race.
 *
 * The engine behind the entry points is whatever LSPosed/Vector ships; this layer
 * has no dependency on any specific inline hook implementation.
 */
#pragma once

#include "hook_bank.h"
#include "page_guard.h"

#include <cstdint>

namespace nhk {

struct NativeApiEntries {
    int (*hook_func)(void *target, void *replacement, void **original);
    int (*unhook_func)(void *target);
};

/**
 * Process-wide backend handle. The entry points arrive once, during
 * native_init; every feature resolves them through this singleton, which is
 * exactly the role the desktop's global `hook_function` pointer used to play.
 */
class NativeInlineBackend {
public:
    static NativeInlineBackend &instance() {
        static NativeInlineBackend backend;
        return backend;
    }

    void install_entries(const NativeApiEntries &entries) { entries_ = entries; }

    const NativeApiEntries &entries() const { return entries_; }

    /** Feature hook: fills the install and uninstall functions of a hook_bank host. */
    template<size_t kPatchWords>
    bool attach(InlineHookHost<kPatchWords> &host) const {
        host.hook_install = [](void *target, void *replacement, void **original) -> int {
            const NativeApiEntries *api = NativeInlineBackend::instance().live();
            return api != nullptr ? api->hook_func(target, replacement, original) : -1;
        };
        host.hook_uninstall = [](void *target) -> int {
            const NativeApiEntries *api = NativeInlineBackend::instance().live();
            return api != nullptr && api->unhook_func != nullptr ? api->unhook_func(target) : -1;
        };
        return entries_.hook_func != nullptr && entries_.unhook_func != nullptr;
    }

private:
    NativeInlineBackend() = default;
    const NativeApiEntries *live() const {
        return entries_.hook_func != nullptr ? &entries_ : nullptr;
    }
    NativeApiEntries entries_{};
};

// Source-compatible name for consumers written before Vector support was explicit.
using LsposedInlineBackend = NativeInlineBackend;

} // namespace nhk
