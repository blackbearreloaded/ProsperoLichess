// ProsperoLichess - Animated in-place update screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/scene.hpp"
#include "net/self_update_service.hpp"

namespace pch::modes
{

std::unique_ptr<app::Scene> make_update(update::Offer offer);

} // namespace pch::modes
