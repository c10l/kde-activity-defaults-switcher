// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kcm_smoke_support.h"

namespace ads::kcm_smoke {

namespace {
KcmBackend *s_backend = nullptr;
}

void setBackend(KcmBackend *backend)
{
    s_backend = backend;
}

KcmBackend *backend()
{
    return s_backend;
}

} // namespace ads::kcm_smoke
