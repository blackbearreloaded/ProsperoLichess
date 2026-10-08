# ProsperoLichess - Translator notes: the home screen, the left menu, the status strip, the title.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# English text -> a note (see notes.py). A text with a context has the key
# "context\x04Text".

NOTES = {
    # ---- the left menu and the strip along the top ----
    "Home": "Entry of the left menu and the page's name in the top strip. Menu entries must stay "
            "short: about 12 letters.",
    "Puzzles": "Entry of the left menu (about 12 letters); also the label over the player's puzzle "
               "rating and over the number of puzzles on the console.",
    "Play": "Entry of the left menu: the page where a game is started. About 12 letters.",
    "Watch": "Entry of the left menu: the page with live games (Lichess TV). About 9 letters: a "
             "mark stands after it.",
    "Profile": "Entry of the left menu: the player's account and games. About 12 letters.",
    "Settings": "Entry of the left menu. About 12 letters.",
    "Open": "Button hint in the left menu: go into the highlighted page.",
    "Choose": "Button hint: move the highlight from one entry to the next.",
    "Move": "Button hint for the directional buttons: move the highlight. Not a chess move.",
    "Online": "State in the top strip: connected to Lichess and signed in. One or two short words.",
    "Offline": "No connection to Lichess: the state in the top strip and the small heading of "
               "the menu's card; also the kind of a game that needs no connection (Pass & Play). "
               "Short: about 10 letters.",
    "Not signed in": "State in the top strip, and the title of the menu's card: connected, but "
                     "no Lichess account is signed in. About 16 letters.",
    "Signing in": "State in the top strip while the account is being checked. Short.",
    "Reconnecting": "State in the top strip while a lost connection is being restored. Short.",
    "Connection trouble": "State in the top strip: a request to Lichess failed. Short.",
    "Guest": "Small heading of the menu's card when nobody is signed in: the player is a guest.",
    "No connection": "Title of the menu's card while Lichess cannot be reached. About 16 letters.",
    "Puzzles still work": "Second line of the menu's card while offline: the puzzles stored on "
                          "the console can still be played. About 22 letters a line, two lines "
                          "at most.",
    "Sign in under Profile": "Second line of the menu's card: where to sign in (Profile is the "
                             "menu entry). About 22 letters a line, two lines at most.",
    "Your move": "Small heading of the menu's card, and the large line on a game's card: it is "
                 "the player's turn in a game. About 10 letters a line; two lines are possible "
                 "on the game's card.",
    "{0} game waits": "Title of the menu's card: {0} is 1, the number of games in which it is "
                      "the player's turn. About 16 letters.",
    "{0} games wait": "Title of the menu's card: {0} is the number of games (2 or more) in "
                      "which it is the player's turn. About 16 letters.",
    "vs {0}": "{0} is the opponent's name, or \"Stockfish level 3\". \"vs\" as in \"versus\".",
    "vs {0} and more": "Second line of the menu's card when several games wait: {0} is the "
                       "opponent of the first one.",
    "Stockfish level {0}": "The computer opponent: {0} is its strength, 1 to 8. Stockfish is a "
                           "name.",
    "Update available": "Title of the announcement that a newer version of the app exists.",
    "Version {0} is on homebrew.page": "{0} is a version number such as 01.000.010; "
                                       "homebrew.page is the web site that has it.",
    # ---- notices in the top strip (one line each) ----
    "Back online": "Notice: the connection to Lichess is back.",
    "Reconnected": "Notice: a live connection (a game, the TV feed) works again.",
    "Connection lost. Online modes will return when it is back.": "Notice, one line: keep it "
                                                                  "as short as the English.",
    "Connection problem. Retrying…": "Notice: one request failed and is sent again.",
    "Connection to Lichess lost. Reconnecting…": "Notice while the account's live connection "
                                                     "is being restored.",
    "Lichess TV lost the feed. Reconnecting…": "Notice while the live game's connection is "
                                                  "being restored. Lichess TV is a name.",
    "Lichess asked us to slow down. Resuming in {0} s": "Notice: Lichess limits how often the "
                                                        "app may ask. {0} is a number of seconds; "
                                                        "\"s\" is the unit.",
    "{0} challenges you": "{0} is a player's name: that player invites you to a game.",
    "Anonymous": "Stands for the name of a player on Lichess TV who has no account name.",
    # ---- the speeds of a game ----
    "Bullet": "A speed of play, as lichess.org names it in your language.",
    "Blitz": "A speed of play, as lichess.org names it in your language.",
    "Rapid": "A speed of play, as lichess.org names it in your language; also the label over the "
             "player's rapid rating.",
    "Classical": "A speed of play, as lichess.org names it in your language.",
    "Correspondence": "A speed of play (days for each move), as lichess.org names it in your "
                      "language.",
    # ---- the home page: today's puzzle ----
    "Daily puzzle": "Small heading over the puzzle of the day.",
    "White to play": "Large title of the daily puzzle: White has the move. About 16 letters.",
    "Black to play": "Large title of the daily puzzle: Black has the move. About 16 letters.",
    "One new puzzle every day, shared by every player on Lichess. Find the best move.":
        "Under the daily puzzle's title. Three lines at most: about 140 letters.",
    "Solve puzzles of rising difficulty. One mistake ends the run, and you get a single skip.":
        "What Puzzle Streak is, under its title. Three lines at most: about 140 letters.",
    "Rating": "Label over a number: how hard the puzzle is.",
    "Played": "Label over a number: how many times the puzzle has been played.",
    "Solve": "Button: open the daily puzzle to solve it.",
    "Training": "Button: open puzzle training (endless puzzles at the player's level).",
    "Offline puzzles": "Small heading shown instead of the daily puzzle when there is no "
                       "connection.",
    "Puzzle Streak": "Name of a puzzle mode. Write it as lichess.org does in your language (most "
                     "keep the English name).",
    "Puzzle Storm": "Name of a puzzle mode. Write it as lichess.org does in your language (most "
                    "keep the English name).",
    "Start": "Button: begin a Puzzle Streak run.",
    "Storm": "Short for Puzzle Storm: a button that starts it, and the small heading of its card. "
             "About 10 letters.",
    "Streak": "Short for Puzzle Streak, on its card. About 10 letters.",
    "Best streak": "Label over a number: the longest run of solved puzzles in Puzzle Streak. "
                   "About 16 letters.",
    "Best storm": "Label over a number: the best score in Puzzle Storm. About 16 letters.",
    "Puzzles solved": "Label over a number: puzzles solved on this console.",
    # ---- the home page: the figures at the right ----
    "Lichess rating": "Under a rating: it is the player's rating on Lichess.",
    "Provisional rating": "Under a rating: Lichess is not yet sure of it (few games played).",
    "No rating yet": "Under a dash: the player has no rating of this kind.",
    "On this console": "Under a record: it was set on this console (it is not from Lichess).",
    # ---- the home page: the row of cards ----
    "heading\x04Continue": "Small heading over the row of cards on the home page: games and modes "
                           "to go on with.",
    "Your game": "Small heading on the card of one of the player's online games. About 10 letters.",
    "Waiting": "On a game's card: the opponent has the move. About 10 letters a line, two lines "
               "at most.",
    "Your turn": "Under a game's card: the player has the move.",
    "Their turn": "Under a game's card: the opponent has the move.",
    "Your turn · {0}": "Under a game's card: {0} is the game's speed (Rapid, "
                            "Correspondence...). About 34 letters with it.",
    "Their turn · {0}": "Under a game's card: {0} is the game's speed (Rapid, "
                             "Correspondence...). About 34 letters with it.",
    "No clock": "On a game's card: the game has no time limit. About 13 letters.",
    "{0}d {1}h left": "Time left to move: {0} days and {1} hours, with one-letter units "
                      "(\"2d 4h left\"). About 13 letters.",
    "{0}h {1}m left": "Time left to move: {0} hours and {1} minutes, with one-letter units "
                      "(\"3h 20m left\"). About 13 letters.",
    "time\x04{0} left": "Time left to move: {0} is a clock such as 5:07 (minutes and seconds).",
    "Pass & Play": "Name of the mode in which two players share one controller.",
    "New game": "On the Pass & Play card: nothing is saved, a new game starts. About 10 letters "
                "a line, two lines at most.",
    "Resume": "On the Pass & Play card: a saved game goes on. About 10 letters.",
    "Your game is saved": "Under the Pass & Play card. About 34 letters.",
    "Two players, one controller": "Under the Pass & Play card. About 34 letters.",
    "Race the clock": "Under the Puzzle Storm card. About 34 letters.",
    "One mistake ends the run": "Under the Puzzle Streak card. About 34 letters.",
    "Best {0}": "On a card: {0} is the player's best score in that mode. About 13 letters.",
    "Lichess TV": "Name of Lichess's channel of live games. Write it as lichess.org does.",
    "The best games, live": "Under the Lichess TV card. About 34 letters.",
    "Live": "On the Lichess TV card: a game is on air. About 10 letters.",
    "LIVE": "A small badge on the Lichess TV card: on air. In capitals; very short.",
    "Top rated": "On the Lichess TV card: the game shown is the highest rated one. About 13 "
                 "letters.",
    "{0} vs {1}": "{0} and {1} are the names of the two players of a game.",
    # ---- the opening title ----
    "Chess on lichess.org, native on PS5": "The line under the app's name on the opening title.",
    "An unofficial client": "On the opening title: the app is not made by Lichess.",
    "Connected to lichess.org": "On the opening title, once Lichess has answered.",
    "Checking the connection": "On the opening title, while Lichess has not answered yet.",
}
