// ProsperoLichess - Host self-update lifecycle regression.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/self_update_service.hpp"

#include <gtest/gtest.h>

namespace pch::update
{
namespace
{

TEST(SelfUpdateService, PreviewDownloadsAppliesAndFinishes)
{
    preview("01.000.010", 32u << 20);
    Offer offer;
    ASSERT_TRUE(take_offer(&offer));
    EXPECT_TRUE(offer.installable);
    EXPECT_EQ(offer.version, "01.000.010");
    EXPECT_EQ(offer.size, 32u << 20);

    ASSERT_TRUE(begin());
    Progress progress;
    for (int frame = 0; frame < 260 && progress.phase != Phase::ready; ++frame)
        progress = poll();
    ASSERT_EQ(progress.phase, Phase::ready);
    EXPECT_TRUE(apply());
    finish();
    EXPECT_EQ(poll().phase, Phase::idle);
}

TEST(SelfUpdateService, CancellationReachesTerminalState)
{
    preview("01.000.011");
    Offer offer;
    ASSERT_TRUE(take_offer(&offer));
    ASSERT_TRUE(begin());
    cancel();
    Progress progress;
    for (int frame = 0; frame < 12 && progress.phase != Phase::cancelled; ++frame)
        progress = poll();
    EXPECT_EQ(progress.phase, Phase::cancelled);
    finish();
}

} // namespace
} // namespace pch::update
