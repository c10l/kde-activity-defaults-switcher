// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

namespace ads {
class KcmBackend;
}

// The offscreen QML smoke test (ticket 22) loads the KCM plugin through
// KQuickConfigModuleLoader, which dlopens the plugin: the plugin gets its own
// copy of any object the test executable holds. This tiny shared library is the
// one address space both sides see, so the test can hand the plugin the
// KcmBackend built from its fakes (view the plugin's module as a thin shell
// over an injected backend - the same shape as production, where the shell
// builds the real backend).
namespace ads::kcm_smoke {

// The backend the plugin module should expose as its `backend` property. The
// caller retains ownership; it must outlive the loaded module.
void setBackend(KcmBackend *backend);
KcmBackend *backend();

} // namespace ads::kcm_smoke
