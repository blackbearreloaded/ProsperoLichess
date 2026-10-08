// ProsperoLichess - Signing in to Lichess with a phone or a personal token.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen in the house look (app/look.hpp):
//
//   - down the left, what this is in the display face and the three steps as
//     a lit timeline: the line fills as the sign-in advances, the step being
//     done breathes, a finished one carries a check, a failed one a cross;
//   - the stage beside it holds one thing at a time, each arriving from a few
//     pixels lower: the QR code on a white card that stands off the page on a
//     glow, with the time left as a ticker over a level that drains; the token
//     keyboard in the screen's accent; the wait; and the verdict, whose mark
//     pops into place.

#include "app/chrome.hpp"
#include "core/strings.hpp"
#include "lichess/json.hpp"
#include "lichess/session.hpp"
#include "modes/scenes.hpp"
#include "net/listener.hpp"
#include "net/pkce.hpp"
#include "net/url.hpp"
#include "platform/ps5/system.hpp"
#include "ui/components/button.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/text_field.hpp"
#include "ui/qr.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr const char *kClientId = "prosperolichess";
constexpr const char *kScopes =
    "board:play puzzle:read puzzle:write challenge:read challenge:write";
constexpr float kLoginSeconds = 300.0f; // how long a sign-in link is offered
constexpr float kWelcomeSeconds = 2.0f; // the success state closes by itself
constexpr float kCheckSeconds = 45.0f;  // an account check that never answers
constexpr const char *kTokenPrefix = "lip_";
constexpr int kMinTokenLength = 8;
constexpr int kStepCount = 3;

// The page's own layout, under the status bar: a column of words and steps on
// the left, the stage on the right.
constexpr float kLeft = app::kMargin;
constexpr float kLeftWidth = 424.0f;
// A line of the column may be this long: it ends 24 before the stage.
constexpr float kTitleRoom = kLeftWidth + 24.0f;
constexpr Rect kStage{kLeft + kLeftWidth + 48.0f, app::kTop,
                      app::kRight - kLeft - kLeftWidth - 48.0f, app::kBottom - app::kTop};
constexpr float kPad = 48.0f;
constexpr float kStageRight = kStage.x + kStage.w - kPad;
constexpr float kStageBottom = kStage.y + kStage.h - kPad;
constexpr float kColumnGap = 56.0f;
constexpr float kButton = 64.0f;
// The phone flow: the code is the stage's hero.
constexpr Rect kCode{kStage.x + kPad + 12.0f, kStage.y + kPad + 12.0f, 476.0f, 476.0f};
constexpr float kCodeColumn = kStage.x + kPad + 500.0f + kColumnGap;
// The token flow: a smaller code, the keys need the room.
constexpr float kTokenTop = kStage.y + 116.0f;
constexpr Rect kTokenCode{kStage.x + kPad + 12.0f, kTokenTop + 12.0f, 300.0f, 300.0f};
constexpr float kKeysColumn = kStage.x + kPad + 324.0f + kColumnGap;
// The timeline's markers.
constexpr float kNodeX = kLeft + 26.0f;
constexpr float kNodeTop = 560.0f;
constexpr float kNodePitch = 124.0f;
constexpr float kNode = 24.0f; // a marker's radius

// Text as it stands in a web page.
std::string html(std::string_view text)
{
    std::string out;
    for (const char c : text)
    {
        if (c == '&')
            out += "&amp;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else
            out.push_back(c);
    }
    return out;
}

// The page the phone shows once Lichess has sent it back to the console, in
// the console's language.
std::string reply_page()
{
    return std::string("<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
                       "content=\"width=device-width,initial-scale=1\"><title>ProsperoLichess"
                       "</title><style>body{font-family:system-ui,sans-serif;background:#0c1330;"
                       "color:#f5f3ff;display:flex;align-items:center;justify-content:center;"
                       "height:100vh;margin:0;text-align:center}h1{font-size:1.6em}"
                       "p{color:#a9a8c8}</style></head><body><div><h1>&#9822; ") +
           html(tr("Signed in to ProsperoLichess")) + "</h1><p>" +
           html(tr("You can put your phone away and return to your PS5.")) +
           "</p></div></body></html>";
}

// What the player does, in order. The second one explains a warning Lichess
// shows, which would otherwise stop a careful player.
constexpr const char *kScanStep = TR("Scan the code with your phone's camera. Your phone must be "
                                     "on the same network as this PS5.");
constexpr const char *kApproveStep = TR("Sign in to Lichess and press Authorize. Lichess will warn "
                                        "that the address is not secure: it is this console on "
                                        "your home network.");
constexpr const char *kDoneStep =
    TR("Your phone shows \"Signed in\" and ProsperoLichess continues by itself.");
constexpr const char *const kPhoneSteps[] = {kScanStep, kApproveStep, kDoneStep};

// The timeline's words, for the phone flow and for the token flow. A label is
// translated as a step's heading ("Done" is also a key and a button hint).
struct StepWords
{
    const char *label;
    const char *caption;
};
constexpr StepWords kPhoneWords[kStepCount] = {{TRC("step", "Scan"), TR("With your phone")},
                                               {TRC("step", "Authorize"), TR("At lichess.org")},
                                               {TRC("step", "Done"), TR("Back on the console")}};
constexpr StepWords kTokenWords[kStepCount] = {
    {TRC("step", "Create"), TR("A token at lichess.org")},
    {TRC("step", "Type"), TR("The token, here")},
    {TRC("step", "Done"), TR("Back on the console")}};

// The two ends of a token's helper line, with a dot between them.
constexpr const char *kDot = "  \xC2\xB7  ";

// look::kicker for a text that has `room` and no more: a translation is
// often longer than the English words.
float kicker_fit(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
                 float baseline, float room, Color color, Align align = Align::left,
                 float size = 16.0f)
{
    return ui::text_fit(list, fonts.semibold, ui::upper(text), x, baseline, size, room, color,
                        align, 3.0f);
}

ui::KeyboardKey wide_key(ui::KeyKind kind, int span)
{
    ui::KeyboardKey key;
    key.kind = kind;
    key.span = span;
    return key;
}

// What a Lichess token is made of: letters in both cases, digits and the
// underscore of its "lip_" prefix. No space, no symbols to get lost in.
ui::KeyboardLayout token_layout()
{
    ui::KeyboardLayout layout;
    layout.name = "abc";
    layout.columns = 10;
    layout.add_row("1234567890").add_row("qwertyuiop").add_row("asdfghjkl_").add_row("zxcvbnm");
    layout.add_row({wide_key(ui::KeyKind::shift, 3), wide_key(ui::KeyKind::backspace, 3),
                    wide_key(ui::KeyKind::done, 4)});
    return layout;
}

float node_y(int index)
{
    return kNodeTop + static_cast<float>(index) * kNodePitch;
}

// A spring's step toward its target; under reduced motion the value snaps.
void ease(tween::Spring &spring, float dt, float omega, bool calm)
{
    if (calm)
        spring.snap(spring.target);
    else
        spring.update(dt, omega);
}

class AccountScene final : public app::Scene
{
  public:
    AccountScene()
    {
        const Color accent = look::accent(look::Section::account);
        show_steps(false, 0, true);

        use_token_.label = tr("Use a token instead");
        use_token_.style.role = ui::ButtonRole::secondary;
        use_token_.set_bounds({kCodeColumn, kStageBottom - kButton, 320.0f, kButton});
        use_token_.set_active(true);

        busy_.style.kind = ui::SpinnerKind::arc;
        busy_.style.color = accent;
        busy_.set_bounds({kStage.cx() - 48.0f, kStage.cy() - 138.0f, 96.0f, 96.0f});

        field_.style.password = true; // a token is a password: never readable on screen
        field_.set_label(tr("Personal token"));
        field_.set_placeholder("lip_...");
        field_.set_helper(fill(tr("Starts with {0}"), {kTokenPrefix}));
        field_.set_bounds(
            {kKeysColumn, kTokenTop, kStageRight - kKeysColumn, field_.preferred_height()});
        field_.set_active(true);
        keys_.set_layouts({token_layout()});
        keys_.style.done_label = tr("Done");
        keys_.style.panel = false;
        keys_.style.auto_capital = false;
        keys_.style.bindings.backspace = Action::west;
        keys_.style.bindings.shift = Action::north;
        keys_.style.bindings.done = Action::menu;
        const float keys_y = field_.bounds().y + field_.bounds().h + 16.0f;
        keys_.set_bounds({kKeysColumn, keys_y, kStageRight - kKeysColumn, 5.0f * 84.0f});
        // The keys, the field and the button wear the screen's accent.
        look::tint(accent, use_token_, field_, keys_);
        shown_.snap(1.0f);
    }

    ~AccountScene() override
    {
        listener_.stop();
        if (session_ != nullptr && request_ != 0)
            session_->cancel(request_);
    }

    void enter(app::Context &ctx) override
    {
        session_ = ctx.lichess;
        since_ = 0.0f;
        // The button is as wide as it was drawn for, or as a longer label (a
        // translation) needs.
        use_token_.set_bounds(
            {kCodeColumn, kStageBottom - kButton,
             std::clamp(use_token_.preferred_width(*ctx.fonts), 320.0f, kStageRight - kCodeColumn),
             kButton});
        if (session_ == nullptr)
            return;
        if (stage_ != Stage::none)
            return;
        if (session_->signed_in())
        {
            // Opened while an account is signed in: there is nothing to do
            // here but say so.
            outcome_title_ = fill(tr("Signed in as {0}"), {session_->username()});
            outcome_body_ = tr("Your ratings and your games in progress are on the Profile page, "
                               "and signing out is there too.");
            outcome_action_.clear();
            set_stage(Stage::signed_in);
            set_step(kStepCount, true);
        }
        else
        {
            begin_oauth();
        }
    }

    app::Transition update(app::Context &ctx, const InputFrame &input, float dt) override
    {
        ctx.calm(use_token_, busy_, field_, keys_);
        calm_ = ctx.reduced_motion();
        since_ += dt;
        app::Transition transition = step(ctx, input, dt);

        shown_.target = 1.0f;
        ease(shown_, dt, 12.0f, calm_);
        animate(dt);
        use_token_.update(dt);
        busy_.update(dt);
        field_.update(dt);
        keys_.update(dt);
        return transition;
    }

    void draw(app::Context &ctx, app::Frame &frame) const override
    {
        gfx::DrawList &list = frame.scene;
        ui::Canvas canvas = app::canvas_for(ctx, list);
        const Color accent = look::accent(look::Section::account);

        draw_words(ctx, list);
        draw_timeline(ctx, list);

        // ---- the stage
        {
            const float in = rise(1);
            list.push_opacity(in);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in));
            look::panel(list, kStage, 0.0f, accent);
            list.pop_transform();
            list.pop_opacity();
        }

        // What the stage holds changes with the step of the sign-in: the new
        // content fades in from a few pixels lower.
        const float t = tween::clamp01(shown_.value) * rise(2);
        list.push_opacity(t);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, calm_ ? 0.0f : 16.0f * (1.0f - t));
        switch (stage_)
        {
        case Stage::oauth:
            draw_phone(ctx, canvas);
            break;
        case Stage::token:
            draw_token(ctx, canvas);
            break;
        case Stage::none:
        case Stage::exchanging:
        case Stage::checking:
            draw_busy(ctx, canvas);
            break;
        case Stage::failed:
        case Stage::welcome:
        case Stage::signed_in:
            draw_outcome(ctx, list);
            break;
        }
        list.pop_transform();
        list.pop_opacity();

        draw_stage_hints(ctx, list);
    }

    look::Mood mood() const override
    {
        return look::mood(look::Section::account);
    }

    const char *name() const override
    {
        return "account";
    }

  private:
    enum class Stage
    {
        none,
        oauth,      // QR shown, waiting for the phone
        exchanging, // code received, fetching the token
        checking,   // personal token entered, checking it
        token,      // typing a personal token
        failed,
        welcome,   // signed in a moment ago
        signed_in, // opened with an account already signed in
    };

    // ---- the flow ------------------------------------------------------------

    app::Transition step(app::Context &ctx, const InputFrame &input, float dt)
    {
        if (session_ == nullptr)
            return leave_on_back(ctx, input);
        lichess::Session &session = *session_;

        if (session.signed_in() && stage_ != Stage::welcome && stage_ != Stage::signed_in)
        {
            listener_.stop();
            field_.clear();
            outcome_title_ = fill(tr("Signed in as {0}"), {session.username()});
            outcome_body_ = tr("Rated puzzles, online games and your games in progress are ready.");
            outcome_action_ = tr("Continue");
            set_stage(Stage::welcome);
            set_step(kStepCount, false);
            timer_ = kWelcomeSeconds;
            ctx.cue(audio::Cue::new_record);
            if (ctx.notify)
                ctx.notify(outcome_title_, app::Note::success);
            return app::Transition::stay();
        }

        switch (stage_)
        {
        case Stage::welcome:
            timer_ -= dt;
            if (timer_ <= 0.0f || input.is_pressed(Action::confirm) ||
                input.is_pressed(Action::back))
                return app::Transition::pop();
            return app::Transition::stay();
        case Stage::signed_in:
            if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
            {
                ctx.cue(audio::Cue::back);
                return app::Transition::pop();
            }
            return app::Transition::stay();
        case Stage::oauth:
        {
            timer_ -= dt;
            std::string path;
            std::string query;
            if (listener_.poll(&path, &query))
                on_callback(ctx, query);
            if (stage_ != Stage::oauth)
                break;
            if (timer_ <= 0.0f)
            {
                fail(ctx, tr("The sign-in link expired. Try again for a new one."));
                break;
            }
            // The button is all there is to move to: a direction is answered
            // with a nudge.
            if (input.nav != Direction::none && !input.nav_repeat)
                nudge_.trigger();
            // Square is the shortcut the old screen had; Cross presses the button.
            const bool shortcut = input.is_pressed(Action::west);
            if (shortcut)
            {
                use_token_.press();
                ctx.cue(audio::Cue::tab);
            }
            if (shortcut || use_token_.handle(input, *ctx.feedback) == ui::Event::activated)
            {
                listener_.stop();
                begin_token();
                return app::Transition::stay();
            }
            break;
        }
        case Stage::exchanging:
            if (!pending_error_.empty())
            {
                fail(ctx, pending_error_);
                break;
            }
            [[fallthrough]];
        case Stage::checking:
            checking_for_ += dt;
            if (stage_ == Stage::exchanging && !token_sent_)
                break; // the token request is still out
            if (session.signing_in() && checking_for_ < kCheckSeconds)
                break;
            // Not signed in (that was handled above) and no longer trying.
            if (session.signing_in() || session.account_error_status() <= 0)
                fail(ctx, tr("Could not reach Lichess. Check the connection and try again."));
            else if (by_token_)
                fail(ctx, tr("Lichess did not accept that token."));
            else
                fail(ctx, fill(tr("Lichess did not accept the sign-in ({0})."),
                               {std::to_string(session.account_error_status())}));
            break;
        case Stage::token:
            return type_token(ctx, input);
        case Stage::failed:
            if (input.is_pressed(Action::confirm))
            {
                ctx.cue(audio::Cue::select);
                begin_oauth();
                return app::Transition::stay();
            }
            break;
        case Stage::none:
            break;
        }
        return leave_on_back(ctx, input);
    }

    app::Transition leave_on_back(app::Context &ctx, const InputFrame &input)
    {
        if (!input.is_pressed(Action::back))
            return app::Transition::stay();
        ctx.cue(audio::Cue::back);
        return app::Transition::pop();
    }

    app::Transition type_token(app::Context &ctx, const InputFrame &input)
    {
        keys_.set_length(field_.length());
        const ui::Event event = keys_.handle(input, *ctx.feedback);
        for (int i = 0; i < keys_.erased(); ++i)
            field_.backspace();
        if (!keys_.typed().empty())
            field_.insert(keys_.typed());
        if (event == ui::Event::changed)
        {
            field_.set_error({});
            // Typing past the prefix says the token exists: the first step is done.
            set_step(field_.text() == kTokenPrefix || field_.text().empty() ? 0 : 1, false);
        }
        field_.set_helper(
            fill(tr("Starts with {0}"), {kTokenPrefix}) + kDot +
            plural(TR("{0} character typed"), TR("{0} characters typed"), field_.length()));

        if (event == ui::Event::activated)
        {
            if (field_.length() < kMinTokenLength)
            {
                field_.set_error(fill(tr("A token has at least {0} characters"),
                                      {std::to_string(kMinTokenLength)}));
                ctx.cue(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                return app::Transition::stay();
            }
            by_token_ = true;
            token_sent_ = true;
            checking_for_ = 0.0f;
            session_->sign_in(field_.text(), true);
            // The session has it now; the screen forgets it.
            field_.clear();
            set_stage(Stage::checking);
            set_step(2, false);
        }
        else if (event == ui::Event::cancelled)
        {
            field_.clear();
            // Back to the QR code, or out when this console cannot offer one.
            if (!phone_flow_)
                return app::Transition::pop();
            begin_oauth();
        }
        return app::Transition::stay();
    }

    void set_stage(Stage stage)
    {
        stage_ = stage;
        shown_.value = 0.0f;
        shown_.velocity = 0.0f;
        staged_ = 0.0f;
        // A verdict arrives with ceremony.
        verdict_.start(0.7f);
    }

    // The step the timeline is on: 0-based, kStepCount once every step is done.
    void set_step(int step, bool snap)
    {
        if (step != step_)
            bump_.trigger();
        step_ = step;
        if (snap)
            placed_ = false;
    }

    // token: the three steps of the token flow instead of the phone flow's.
    void show_steps(bool token, int step, bool snap)
    {
        token_steps_ = token;
        failed_step_ = -1;
        set_step(step, snap);
    }

    void fail(app::Context &ctx, std::string message)
    {
        listener_.stop();
        pending_error_.clear();
        outcome_title_ = tr("Sign-in did not finish");
        outcome_body_ = std::move(message);
        outcome_action_ = tr("Try again");
        failed_step_ = std::clamp(step_, 0, kStepCount - 1);
        set_stage(Stage::failed);
        ctx.cue(audio::Cue::error);
    }

    void begin_oauth()
    {
        pending_error_.clear();
        token_sent_ = false;
        by_token_ = false;
        listener_.stop();
        std::string error;
        ip_ = net::lan_ipv4();
        if (ip_.empty() || !listener_.start(0, &error))
        {
            sys::log("[PCH] sign-in listener unavailable: %s", error.c_str());
            phone_flow_ = false;
            begin_token();
            return;
        }
        phone_flow_ = true;
        listener_.set_reply(reply_page());
        verifier_ = net::make_verifier();
        state_ = net::make_state();
        redirect_ = "http://" + ip_ + ":" + std::to_string(listener_.port()) + "/callback";
        const std::string url = net::with_query(std::string(lichess::kBaseUrl) + "/oauth",
                                                {{"response_type", "code"},
                                                 {"client_id", kClientId},
                                                 {"redirect_uri", redirect_},
                                                 {"code_challenge_method", "S256"},
                                                 {"code_challenge", net::challenge_s256(verifier_)},
                                                 {"scope", kScopes},
                                                 {"state", state_}});
        qr_.encode(url);
        address_ = ip_ + ":" + std::to_string(listener_.port());
        timer_ = kLoginSeconds;
        set_stage(Stage::oauth);
        show_steps(false, 0, false);
        sys::log("[PCH] sign-in waiting on %s:%u", ip_.c_str(), listener_.port());
    }

    void begin_token()
    {
        const std::string url =
            std::string(lichess::kBaseUrl) +
            "/account/oauth/token/create?scopes[]=board:play&scopes[]=puzzle:read"
            "&scopes[]=puzzle:write&scopes[]=challenge:read&scopes[]=challenge:write"
            "&description=ProsperoLichess%20PS5";
        qr_.encode(url);
        field_.set_text(kTokenPrefix);
        field_.set_error({});
        keys_.set_shift(ui::KeyboardShift::off);
        keys_.set_focus(1, 0);
        keys_.enter();
        set_stage(Stage::token);
        show_steps(true, 0, false);
    }

    void on_callback(app::Context &ctx, const std::string &query)
    {
        const auto state = net::query_param(query, "state");
        if (!state || *state != state_)
        {
            sys::log("[PCH] sign-in callback ignored (state mismatch)");
            return;
        }
        if (const auto error = net::query_param(query, "error"))
        {
            // The phone got as far as Lichess: the refusal belongs to the second step.
            set_step(1, true);
            fail(ctx, *error == "access_denied"
                          ? std::string(tr("Sign-in was cancelled on the phone."))
                          : fill(tr("Lichess reported an error: {0}"), {*error}));
            return;
        }
        const auto code = net::query_param(query, "code");
        if (!code)
            return;
        listener_.stop();
        set_stage(Stage::exchanging);
        set_step(2, false);
        checking_for_ = 0.0f;
        token_sent_ = false;
        ctx.cue(audio::Cue::notify);
        lichess::Session *session = session_;
        request_ = session->post("/api/token",
                                 net::form_encode({{"grant_type", "authorization_code"},
                                                   {"code", *code},
                                                   {"code_verifier", verifier_},
                                                   {"redirect_uri", redirect_},
                                                   {"client_id", kClientId}}),
                                 [this, session](const lichess::HttpResult &r)
                                 {
                                     request_ = 0;
                                     lichess::Document doc(r.body);
                                     const std::string token = doc.root()["access_token"].str();
                                     if (!r.ok() || token.empty())
                                     {
                                         pending_error_ =
                                             fill(tr("Lichess did not issue a token ({0})."),
                                                  {std::to_string(r.status)});
                                         return;
                                     }
                                     // The token goes to the session and nowhere else.
                                     session->sign_in(token, false);
                                     token_sent_ = true;
                                 });
        // No request went out (no connection at all): do not wait for an answer.
        if (request_ == 0 && pending_error_.empty() && !token_sent_)
            pending_error_ = tr("Could not reach Lichess. Check the connection and try again.");
    }

    static std::string minutes(float seconds)
    {
        const int whole = std::max(0, static_cast<int>(seconds));
        char text[16];
        std::snprintf(text, sizeof(text), "%d:%02d", whole / 60, whole % 60);
        return text;
    }

    // ---- motion --------------------------------------------------------------

    // The timeline follows the step: its line fills, its markers light.
    void animate(float dt)
    {
        staged_ += dt;
        const float quick = calm_ ? 60.0f : 14.0f;
        // The line reaches the marker of the step in hand, and its end once
        // every step is done.
        line_.target = static_cast<float>(std::clamp(step_, 0, kStepCount - 1));
        for (int i = 0; i < kStepCount; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            done_[at].target = i < step_ ? 1.0f : 0.0f;
            current_[at].target = i == step_ ? 1.0f : 0.0f;
        }
        if (!placed_)
        {
            placed_ = true;
            line_.snap(line_.target);
            for (std::size_t i = 0; i < done_.size(); ++i)
            {
                done_[i].snap(done_[i].target);
                current_[i].snap(current_[i].target);
            }
        }
        ease(line_, dt, 9.0f, calm_);
        for (std::size_t i = 0; i < done_.size(); ++i)
        {
            done_[i].update(dt, quick);
            current_[i].update(dt, quick);
        }
        bump_.update(dt, 7.0f);
        nudge_.update(dt, 9.0f);
        verdict_.update(dt);
    }

    // 0..1 for the index-th part of the screen since it was opened.
    float rise(int index) const
    {
        return calm_ ? 1.0f : look::rise(since_, index, 0.07f, 0.5f);
    }

    // 0..1 for the index-th part of what the stage holds since it changed.
    float staged(int index) const
    {
        return calm_ ? 1.0f : look::rise(staged_, index, 0.07f, 0.45f);
    }

    template <typename Draw> void part(gfx::DrawList &list, float in, const Draw &draw) const
    {
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, look::settle(in, 16.0f));
        draw();
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- drawing -------------------------------------------------------------

    // What this screen is, down the left.
    void draw_words(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::account);
        part(list, rise(0),
             [&]()
             {
                 const Rect tile{kLeft, app::kTop + 8.0f, 44.0f, 44.0f};
                 list.rounded_rect(tile, 13.0f, accent.with_alpha(0.16f));
                 app::draw_nav_icon(list, app::NavIcon::profile, tile.inset(10.0f), accent);
                 kicker_fit(list, fonts, tr("Lichess account"), tile.x + tile.w + 16.0f,
                            tile.cy() + 6.0f, kLeftWidth - tile.w - 16.0f, accent);
             });
        part(list, rise(1),
             [&]()
             {
                 // One text on two lines: the translation says where it breaks
                 // (one that does not is broken where the column ends).
                 const std::string_view title = tr("Sign in\nto Lichess");
                 const std::size_t cut = title.find('\n');
                 if (cut == std::string_view::npos)
                 {
                     ui::paragraph(list, fonts.display, title, kLeft - 3.0f, app::kTop + 134.0f,
                                   64.0f, kTitleRoom, 72.0f, look::kInk, 2);
                     return;
                 }
                 ui::text_fit(list, fonts.display, title.substr(0, cut), kLeft - 3.0f,
                              app::kTop + 134.0f, 64.0f, kTitleRoom, look::kInk);
                 ui::text_fit(list, fonts.display, title.substr(cut + 1), kLeft - 3.0f,
                              app::kTop + 206.0f, 64.0f, kTitleRoom, look::kInk);
             });
        part(list, rise(2),
             [&]()
             {
                 // Four lines stand over the steps (the English words take
                 // three); a longer text is set smaller, on five.
                 const char *words = tr("Rated puzzles, online games and your games in progress, "
                                        "on this console");
                 const float room = kLeftWidth - 24.0f;
                 const bool small = fonts.regular.font->wrap(words, 24.0f, room).size() > 4;
                 ui::paragraph(list, fonts.regular, words, kLeft, app::kTop + 262.0f,
                               small ? 21.0f : 24.0f, room, small ? 30.0f : 34.0f,
                               look::kInk.with_alpha(look::kMuted), small ? 5 : 4);
             });
    }

    // The three steps: a line that fills, markers that light.
    void draw_timeline(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::account);
        const StepWords *words = token_steps_ ? kTokenWords : kPhoneWords;
        const float top = node_y(0);
        const float length = node_y(kStepCount - 1) - top;
        const float in = rise(3);
        list.push_opacity(in);
        list.rounded_rect({kNodeX - 2.0f, top, 4.0f, length}, 2.0f, look::kInk.with_alpha(0.12f));
        const float reach = tween::clamp01(line_.value / static_cast<float>(kStepCount - 1));
        if (reach > 0.001f)
            list.rounded_rect({kNodeX - 2.0f, top, 4.0f, length * reach}, 2.0f, accent);
        list.pop_opacity();

        for (int i = 0; i < kStepCount; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            const float cy = node_y(i);
            const bool failed = failed_step_ == i;
            const float done = failed ? 0.0f : done_[at].value;
            const float current = failed ? 1.0f : current_[at].value;
            const float lit = std::max(done, current);
            const Color c = failed ? look::kBad : accent;
            part(list, rise(3 + i),
                 [&]()
                 {
                     // The step in hand is a little larger, breathes, and
                     // pops when the sign-in reaches it.
                     const float pop = i == step_ || failed ? bump_.value : 0.0f;
                     const float radius = kNode * (1.0f + 0.12f * current + 0.22f * pop);
                     if (current > 0.01f)
                     {
                         const float breath = calm_ ? 0.5f : ui::breathe(ctx.time);
                         list.glow({kNodeX - radius, cy - radius, 2.0f * radius, 2.0f * radius},
                                   radius, 18.0f, c.with_alpha((0.22f + 0.2f * breath) * current));
                     }
                     const float filled = failed ? 1.0f : done;
                     list.circle(kNodeX, cy, radius,
                                 gfx::mix(look::kNight, gfx::mix(look::kNight, c, 0.22f), current));
                     if (filled > 0.01f)
                         list.circle(kNodeX, cy, radius, c.with_alpha(filled));
                     list.ring(kNodeX, cy, radius, 3.0f,
                               gfx::mix(look::kInk.with_alpha(0.22f), c, lit));
                     if (failed)
                     {
                         const float u = radius * 0.34f;
                         list.line(kNodeX - u, cy - u, kNodeX + u, cy + u, 3.5f, look::kNight);
                         list.line(kNodeX - u, cy + u, kNodeX + u, cy - u, 3.5f, look::kNight);
                     }
                     else if (done > 0.5f)
                     {
                         const float u = radius * 0.42f;
                         list.line(kNodeX - u, cy + u * 0.1f, kNodeX - u * 0.25f, cy + u * 0.8f,
                                   3.5f, look::kNight);
                         list.line(kNodeX - u * 0.25f, cy + u * 0.8f, kNodeX + u, cy - u * 0.7f,
                                   3.5f, look::kNight);
                     }
                     else
                     {
                         char number[4];
                         std::snprintf(number, sizeof(number), "%d", i + 1);
                         look::figure(list, fonts, number, kNodeX, cy + 8.0f, 22.0f,
                                      gfx::mix(look::kInk.with_alpha(look::kFaint), c, current),
                                      Align::center);
                     }
                     const float x = kNodeX + kNode + 28.0f;
                     const float room = kLeft + kTitleRoom - x;
                     ui::text_fit(list, fonts.semibold, trc("step", words[i].label), x, cy - 3.0f,
                                  27.0f, room,
                                  look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, lit)));
                     ui::text_fit(
                         list, fonts.regular, tr(words[i].caption), x, cy + 25.0f, 20.0f, room,
                         look::kInk.with_alpha(tween::lerp(look::kFaint, look::kMuted, lit)));
                 });
        }
    }

    // The code on a white card that stands off the page, on a light that
    // breathes while the phone is waited for.
    void draw_code(const app::Context &ctx, gfx::DrawList &list, const Rect &code,
                   bool waiting) const
    {
        const Color accent = look::accent(look::Section::account);
        const Rect card = code.inset(-12.0f);
        const float breath = calm_ || !waiting ? 0.5f : ui::breathe(ctx.time, 3.2f);
        list.shadow({card.x, card.y + 18.0f, card.w, card.h}, 26.0f, 44.0f,
                    Color::rgb(0x000000, 0.5f));
        list.glow(card, 26.0f, 46.0f, accent.with_alpha(0.2f + 0.16f * breath));
        list.rounded_rect(card, 26.0f, Color::rgb(0xffffff));
        qr_.draw(list, code);
    }

    void draw_phone(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::account);
        part(list, staged(0), [&]() { draw_code(ctx, list, kCode, true); });

        // ---- how long the link is still good for
        part(list, staged(2),
             [&]()
             {
                 const float x = kCode.x - 12.0f;
                 const float y = kCode.y + kCode.h + 12.0f + 40.0f;
                 const bool low = timer_ < 30.0f;
                 look::live_dot(list, x + 7.0f, y + 12.0f, 6.0f, low ? look::kBad : accent,
                                ctx.time, calm_);
                 kicker_fit(list, fonts, tr("Waiting for your phone"), x + 26.0f, y + 18.0f,
                            500.0f - 26.0f, low ? look::kBad : accent);
                 look::ticker(list, fonts, minutes(timer_), x - 3.0f, y + 90.0f, 60.0f,
                              low ? look::kBad : look::kInk);
                 look::level(list, {x, y + 112.0f, 500.0f, 8.0f}, timer_ / kLoginSeconds, accent,
                             low);
             });

        // ---- what to do, in order
        const float x = kCodeColumn;
        const float width = kStageRight - x;
        part(list, staged(1),
             [&]()
             {
                 // A heading too long for one line even when shrunk (a
                 // translation) takes two, and the steps start lower.
                 const char *heading = tr("Scan the code with your phone");
                 float baseline = kStage.y + kPad + 104.0f;
                 if (fonts.display.measure(heading, 36.0f) * 0.78f <= width)
                     ui::text_fit(list, fonts.display, heading, x - 2.0f, kStage.y + kPad + 40.0f,
                                  36.0f, width, look::kInk);
                 else
                     baseline = ui::paragraph(list, fonts.display, heading, x - 2.0f,
                                              kStage.y + kPad + 36.0f, 32.0f, width, 40.0f,
                                              look::kInk, 2) +
                                24.0f;
                 // The steps end above the rule over the console's address: text
                 // that needs more lines than the room has is set smaller.
                 const float limit = use_token_.bounds().y - 138.0f - 26.0f;
                 // The baseline of the steps' last line at a size.
                 const auto last_line = [&](float size, float line)
                 {
                     float lines = 0.0f;
                     for (const char *step : kPhoneSteps)
                         lines += static_cast<float>(
                             fonts.regular.font->wrap(tr(step), size, width - 46.0f).size());
                     return baseline + (lines - 1.0f) * line + 2.0f * 24.0f;
                 };
                 float size = 23.0f;
                 float line = 33.0f;
                 while (size > 17.0f && last_line(size, line) > limit)
                 {
                     size -= 2.0f;
                     line -= 3.0f;
                 }
                 for (int i = 0; i < 3; ++i)
                 {
                     char number[4];
                     std::snprintf(number, sizeof(number), "%d", i + 1);
                     list.circle(x + 15.0f, baseline - 8.0f, 15.0f, accent.with_alpha(0.18f));
                     look::figure(list, fonts, number, x + 15.0f, baseline - 1.0f, 19.0f, accent,
                                  Align::center);
                     baseline = ui::paragraph(list, fonts.regular, tr(kPhoneSteps[i]), x + 46.0f,
                                              baseline, size, width - 46.0f, line,
                                              look::kInk.with_alpha(look::kMuted), 6) +
                                24.0f;
                 }
             });
        part(list, staged(3),
             [&]()
             {
                 const float y = use_token_.bounds().y - 138.0f;
                 look::rule(list, x, y, width);
                 kicker_fit(list, fonts, tr("Console address"), x, y + 42.0f, width,
                            look::kInk.with_alpha(look::kFaint), Align::left, 14.0f);
                 look::ticker(list, fonts, fonts.mono.font->fit(address_, 28.0f, width), x,
                              y + 84.0f, 28.0f, look::kInk);
                 list.push_transform(1.0f, 0.0f, 0.0f,
                                     calm_ ? 0.0f : ui::shake(nudge_.value, since_, 7.0f), 0.0f);
                 use_token_.draw(canvas);
                 list.pop_transform();
             });
    }

    void draw_token(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        part(list, staged(0),
             [&]()
             {
                 draw_code(ctx, list, kTokenCode, false);
                 const float x = kTokenCode.x - 12.0f;
                 const float y = kTokenCode.y + kTokenCode.h + 12.0f;
                 ui::text_fit(list, fonts.display, tr("Create a token"), x - 1.0f, y + 62.0f, 32.0f,
                              324.0f, look::kInk);
                 // The stage has room under it for more lines than the three
                 // the English words take.
                 ui::paragraph(
                     list, fonts.regular,
                     tr("Scan to create a personal token at lichess.org, then type it here."), x,
                     y + 104.0f, 22.0f, 324.0f, 31.0f, look::kInk.with_alpha(look::kMuted), 6);
             });
        part(list, staged(1), [&]() { field_.draw(canvas); });
        part(list, staged(2), [&]() { keys_.draw(canvas); });
    }

    void draw_busy(const app::Context &ctx, ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        const ui::Fonts &fonts = *ctx.fonts;
        const Color accent = look::accent(look::Section::account);
        const Rect ring = busy_.bounds();
        look::halo(list, ring.inset(-120.0f), accent, 0.14f);
        busy_.draw(canvas);
        const float room = kStage.w - 2.0f * kPad;
        ui::text_fit(list, fonts.display, tr("Signing in"), kStage.cx(), kStage.cy() + 36.0f, 52.0f,
                     room, look::kInk, Align::center);
        ui::text_fit(list, fonts.regular,
                     by_token_ ? tr("Checking the token with Lichess")
                               : tr("Your phone approved it. Fetching your account"),
                     kStage.cx(), kStage.cy() + 84.0f, 24.0f, room,
                     look::kInk.with_alpha(look::kMuted), Align::center);
    }

    // Done, or not: the mark pops into place, a ring leaves it, the words
    // follow.
    void draw_outcome(const app::Context &ctx, gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = *ctx.fonts;
        const bool good = stage_ != Stage::failed;
        const Color tone = good ? look::kGood : look::kBad;
        const float cx = kStage.cx();
        const float cy = kStage.cy() - 128.0f;
        constexpr float kMark = 62.0f;
        const float t = calm_ || !verdict_.running ? 1.0f : verdict_.progress();
        const float pop = tween::back_out(tween::clamp01(t / 0.6f));
        look::halo(list, {cx - 220.0f, cy - 220.0f, 440.0f, 440.0f}, tone, 0.16f * pop);
        if (t < 1.0f)
            list.ring(cx, cy, kMark + 16.0f + 70.0f * tween::cubic_out(t), 3.0f,
                      tone.with_alpha(0.7f * (1.0f - t)));
        list.push_transform(tween::lerp(0.35f, 1.0f, pop), cx, cy, 0.0f, 0.0f);
        list.circle(cx, cy, kMark + 16.0f, tone.with_alpha(0.16f));
        app::draw_verdict(list, cx, cy, kMark, good, tone, look::kNight);
        list.pop_transform();

        part(list, staged(2),
             [&]()
             {
                 ui::text_fit(list, fonts.display, outcome_title_, cx, kStage.cy() + 30.0f, 52.0f,
                              kStage.w - 2.0f * kPad, look::kInk, Align::center);
             });
        part(list, staged(3),
             [&]()
             {
                 ui::paragraph(list, fonts.regular, outcome_body_, cx, kStage.cy() + 84.0f, 24.0f,
                               920.0f, 35.0f, look::kInk.with_alpha(look::kMuted), 3,
                               Align::center);
             });
        if (outcome_action_.empty())
            return;
        part(list, staged(4),
             [&]()
             {
                 // What Cross does, as a glyph and a word.
                 constexpr float kGlyph = 38.0f;
                 constexpr float kSize = 26.0f;
                 const float y = kStage.cy() + 206.0f;
                 const float width = ui::button_width(ui::Button::cross, kGlyph) + 14.0f +
                                     fonts.semibold.measure(outcome_action_, kSize);
                 const float x = cx - width * 0.5f;
                 ui::draw_button(list, fonts, ui::GlyphStyle::dark(), ui::Button::cross, x, y,
                                 kGlyph);
                 ui::text(list, fonts.semibold, outcome_action_,
                          x + ui::button_width(ui::Button::cross, kGlyph) + 14.0f,
                          y + kSize * 0.35f, kSize, look::kInk);
                 // The welcome leaves by itself: the line shows how soon.
                 if (stage_ == Stage::welcome)
                     look::level(list, {cx - 110.0f, y + 44.0f, 220.0f, 6.0f},
                                 timer_ / kWelcomeSeconds, tone);
             });
    }

    void draw_stage_hints(const app::Context &ctx, gfx::DrawList &list) const
    {
        switch (stage_)
        {
        case Stage::oauth:
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Use a token")},
                                      {ui::Button::circle, TR("Back")}};
            app::draw_hints(ctx, list, hints, 2);
            break;
        }
        case Stage::token:
        {
            const ui::Hint hints[] = {
                {ui::Button::cross, TR("Type")},
                {ui::Button::square, TR("Delete")},
                {ui::Button::triangle, TR("Shift")},
                {ui::Button::options, TR("Done")},
                {ui::Button::circle, phone_flow_ ? TR("QR code") : TR("Back")}};
            app::draw_hints(ctx, list, hints, 5);
            break;
        }
        case Stage::failed:
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Try again")},
                                      {ui::Button::circle, TR("Back")}};
            app::draw_hints(ctx, list, hints, 2);
            break;
        }
        case Stage::welcome:
        {
            const ui::Hint hints[] = {{ui::Button::cross, TR("Continue")}};
            app::draw_hints(ctx, list, hints, 1);
            break;
        }
        case Stage::none:
        case Stage::exchanging:
        case Stage::checking:
        case Stage::signed_in:
        {
            const ui::Hint hints[] = {{ui::Button::circle, TR("Back")}};
            app::draw_hints(ctx, list, hints, 1);
            break;
        }
        }
    }

    lichess::Session *session_ = nullptr;
    Stage stage_ = Stage::none;
    net::CallbackListener listener_;
    ui::QrCode qr_;

    ui::PushButton use_token_;
    ui::Spinner busy_;
    ui::TextField field_; // holds the token while it is typed, masked
    ui::Keyboard keys_;
    std::string outcome_title_; // failed, signed in
    std::string outcome_body_;
    std::string outcome_action_; // what Cross does there; empty for nothing

    // ---- the timeline ----
    int step_ = 0;             // the step in hand; kStepCount once all are done
    int failed_step_ = -1;     // the step the sign-in stopped at, or -1
    bool token_steps_ = false; // the token flow's words instead of the phone flow's

    // ---- motion ----
    float since_ = 0.0f;  // seconds since the screen was opened: parts arrive by it
    float staged_ = 0.0f; // seconds since the stage's content changed
    bool calm_ = false;   // reduced motion
    bool placed_ = false; // the timeline's springs were snapped
    tween::Spring shown_; // 0 -> 1 as a stage's content arrives
    tween::Spring line_;  // how far the timeline's line has filled, in steps
    std::array<tween::Spring, kStepCount> done_;
    std::array<tween::Spring, kStepCount> current_;
    ui::Pulse bump_;       // the marker that just became current pops
    ui::Pulse nudge_;      // a direction with nowhere to go
    tween::Timer verdict_; // the verdict's mark arriving

    std::string ip_;
    std::string address_; // "192.168.1.20:36196": where the phone reaches this console
    std::string verifier_;
    std::string state_;
    std::string redirect_;
    std::string pending_error_; // set by the token request's answer, shown by update()
    std::uint64_t request_ = 0;
    bool phone_flow_ = false; // this console could open its listener
    bool by_token_ = false;   // the account check is for a typed token
    bool token_sent_ = false; // the session has a token and is checking it
    float timer_ = 0.0f;
    float checking_for_ = 0.0f;
};

} // namespace

std::unique_ptr<app::Scene> make_account()
{
    return std::make_unique<AccountScene>();
}

} // namespace pch::modes
