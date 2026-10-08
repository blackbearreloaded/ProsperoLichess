// ProsperoLichess - The Watch page and Lichess TV: which stream is asked for, and when.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/scenes.hpp"
#include "modes/tv_feed.hpp"
#include "play_test_app.hpp"

namespace
{

using namespace pch;

class WatchTvTest : public play_tests::AppFixture
{
  protected:
    void TearDown() override
    {
        modes::set_tv_stand_in(nullptr);
    }

    // The channel the session streams (it notes it when it opens the stream).
    const std::string &streaming() const
    {
        return app_->session().tv().channel;
    }

    // Puts a game on air the way the stream would: first the game, then moves.
    void broadcast(const char *channel)
    {
        game_ = {};
        game_.valid = true;
        game_.id = "testgame";
        game_.channel = channel;
        game_.white = "white";
        game_.black = "black";
        game_.white_clock = 60;
        game_.black_clock = 60;
        send();
    }
    void move(const char *uci)
    {
        chess::Move played;
        ASSERT_TRUE(chess::parse_uci(game_.position, uci, &played)) << uci;
        game_.position = game_.position.after(played);
        game_.last_move = played;
        send();
    }
    void send()
    {
        ++game_.version;
        game_.received = app_->session().now();
        modes::set_tv_stand_in(&game_);
    }

    lichess::TvGame game_;
};

TEST_F(WatchTvTest, ChannelsAreTheOnesTheSessionAccepts)
{
    const auto channels = modes::tv_channels();
    ASSERT_EQ(channels.size(), 6u);
    EXPECT_STREQ(channels[0].id, ""); // the featured game
    EXPECT_EQ(modes::tv_channel_index("rapid"), 2);
    EXPECT_EQ(modes::tv_channel_index("chess960"), 5);
    EXPECT_EQ(modes::tv_channel_index("no-such-channel"), 0);
}

TEST_F(WatchTvTest, ClocksCountDownForTheSideToMove)
{
    lichess::TvGame game;
    game.white_clock = 30;
    game.black_clock = 45;
    game.received = 10.0;
    EXPECT_EQ(modes::tv_clock_ms(game, chess::Color::white, 12.5), 27500);
    EXPECT_EQ(modes::tv_clock_ms(game, chess::Color::black, 12.5), 45000);
    EXPECT_EQ(modes::tv_clock_ms(game, chess::Color::white, 100.0), 0);
    game.black_clock = -1;
    EXPECT_EQ(modes::tv_clock_ms(game, chess::Color::black, 12.5), -1);
}

TEST_F(WatchTvTest, TheLastMoveGetsItsNotationWhenThePositionShownLeadsToIt)
{
    const chess::Position before = chess::Position::start();
    chess::Move move;
    ASSERT_TRUE(chess::parse_uci(before, "g1f3", &move));
    lichess::TvGame game;
    game.position = before.after(move);
    game.last_move = move;
    EXPECT_EQ(modes::tv_last_san(before, game), "Nf3");
    EXPECT_EQ(modes::tv_move_squares(move), "g1-f3");
    // Seen from another position (moves were missed) only its squares are known.
    EXPECT_EQ(modes::tv_last_san(game.position, game), "");
    EXPECT_EQ(modes::tv_move_squares({}), "");
}

TEST_F(WatchTvTest, MaterialCountsWhatEachSideHasLeft)
{
    chess::Position position;
    ASSERT_TRUE(chess::Position::from_fen("4k3/8/8/8/8/8/PPPP4/RN2K3 w - - 0 1", &position));
    EXPECT_EQ(modes::tv_material(position, chess::Color::white), 5 + 3 + 4);
    EXPECT_EQ(modes::tv_material(position, chess::Color::black), 0);
    EXPECT_EQ(modes::tv_material(chess::Position::start(), chess::Color::black), 39);
}

TEST_F(WatchTvTest, EveryChannelHasASignOfItsOwn)
{
    const int count = static_cast<int>(modes::tv_channels().size());
    for (int i = 0; i < count; ++i)
    {
        gfx::DrawList list;
        modes::draw_tv_channel_sign(list, i, {0.0f, 0.0f, 48.0f, 48.0f},
                                    modes::tv_channel_color(i));
        EXPECT_FALSE(list.empty()) << modes::tv_channels()[static_cast<std::size_t>(i)].label;
        EXPECT_GT(modes::tv_channel_color(i).a, 0.9f);
    }
}

TEST_F(WatchTvTest, TheStreamFollowsTheListOnceItRests)
{
    sign_in();
    open_page(modes::kPageWatch);
    ASSERT_EQ(scene(), "watch");
    EXPECT_EQ(streaming(), "");
    // Walking down the list does not open a stream per row ...
    nav(Direction::down);
    idle(3);
    nav(Direction::down);
    idle(3);
    EXPECT_EQ(streaming(), "");
    // ... the row it stops on does.
    idle(40);
    EXPECT_EQ(streaming(), "rapid");
    EXPECT_GT(shapes_, 80u);
}

TEST_F(WatchTvTest, LeavingForTheRailRestoresTheFeaturedGame)
{
    sign_in();
    open_page(modes::kPageWatch);
    nav(Direction::down);
    idle(40);
    ASSERT_EQ(streaming(), "blitz");
    press(Action::back);
    idle(5);
    EXPECT_EQ(streaming(), "");
    // From the rail another page is one step away: nothing else was opened.
    nav(Direction::down);
    idle(10);
    EXPECT_EQ(scene(), "profile");
    EXPECT_EQ(streaming(), "");
}

TEST_F(WatchTvTest, ConfirmOpensTheFullScreenAndTheChannelSurvivesTheWayBack)
{
    sign_in();
    open_page(modes::kPageWatch);
    nav(Direction::down);
    press(Action::confirm); // before the list has rested: the full screen tunes in itself
    idle(40);
    ASSERT_EQ(scene(), "tv");
    EXPECT_EQ(streaming(), "blitz");
    press(Action::page_next);
    EXPECT_TRUE(asked(audio::Cue::tab));
    idle(40);
    EXPECT_EQ(streaming(), "rapid");
    press(Action::back);
    idle(60);
    EXPECT_EQ(scene(), "watch");
    // The page shows what the full screen was on; the stream was not restarted.
    EXPECT_EQ(streaming(), "rapid");
    idle(60);
    EXPECT_EQ(streaming(), "rapid");
}

TEST_F(WatchTvTest, HomeTakesTheFeaturedGameBackAfterTheFullScreen)
{
    sign_in();
    idle(10);
    app_->open(modes::make_tv("bullet"));
    idle(40);
    ASSERT_EQ(scene(), "tv");
    EXPECT_EQ(streaming(), "bullet");
    press(Action::back);
    idle(60);
    EXPECT_EQ(scene(), "home");
    EXPECT_EQ(streaming(), "");
}

TEST_F(WatchTvTest, ChannelsWrapOnTheFullScreen)
{
    sign_in();
    app_->open(modes::make_tv(""));
    idle(30);
    press(Action::page_prev);
    idle(40);
    EXPECT_EQ(streaming(), "chess960");
    nav(Direction::right);
    idle(40);
    EXPECT_EQ(streaming(), "");
}

TEST_F(WatchTvTest, MovesOfTheGameOnAirArePlayedWithTheirSound)
{
    sign_in();
    broadcast("");
    app_->open(modes::make_tv(""));
    idle(40);
    EXPECT_FALSE(asked(audio::Cue::move));
    move("e2e4");
    frame();
    EXPECT_TRUE(asked(audio::Cue::move));
    idle(30);
    move("d7d5");
    frame();
    EXPECT_TRUE(asked(audio::Cue::move));
    idle(30);
    move("e4d5");
    frame();
    EXPECT_TRUE(asked(audio::Cue::capture));
    // The same update seen again is not played twice.
    frame();
    EXPECT_FALSE(asked(audio::Cue::capture));

    // A game of another channel is not this screen's: it shows nothing of it.
    press(Action::page_next);
    idle(40);
    move("d8d5");
    frame();
    EXPECT_FALSE(asked(audio::Cue::capture));
    // Square turns the board.
    press(Action::west);
    EXPECT_TRUE(asked(audio::Cue::toggle));
}

TEST_F(WatchTvTest, ANewGameIsShownAtOnceWithoutASound)
{
    sign_in();
    broadcast("");
    app_->open(modes::make_tv(""));
    idle(40);
    move("e2e4");
    idle(30);
    game_.id = "nextgame";
    game_.position = chess::Position::start();
    game_.last_move = {};
    send();
    frame();
    EXPECT_FALSE(asked(audio::Cue::move));
    EXPECT_GT(shapes_, 100u);
}

TEST_F(WatchTvTest, OfflineBothScreensSaySoAndLetGo)
{
    open_page(modes::kPageWatch);
    EXPECT_EQ(scene(), "watch");
    EXPECT_GT(shapes_, 30u);
    press(Action::confirm); // nothing to open
    idle(10);
    EXPECT_EQ(scene(), "watch");

    app_->open(modes::make_tv(""));
    idle(40);
    EXPECT_EQ(scene(), "tv");
    EXPECT_GT(shapes_, 30u);
    press(Action::back);
    idle(60);
    EXPECT_EQ(scene(), "watch");
}

TEST_F(WatchTvTest, ThePreviewShowsTheGameOnAir)
{
    sign_in();
    open_page(modes::kPageWatch);
    idle(30);
    const std::size_t bones = shapes_;
    broadcast("");
    move("e2e4");
    idle(60);
    // A board with thirty-two pieces is more shapes than its placeholder.
    EXPECT_GT(shapes_, bones + 30u);
}

} // namespace
