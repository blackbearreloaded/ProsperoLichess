# ProsperoLichess - What a translator cannot tell from a text alone: Play, Profile, signing in.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# English text -> a note, as in notes.py. A text in a context is "context\x04Text".
# Lengths are a guide: a text that is longer is first drawn smaller, then cut.

# The names of Lichess' speeds: the words lichess.org uses in the language.
SPEED = "Name of a Lichess speed, as lichess.org calls it. On tiles in capitals: about 15 letters."

NOTES = {
    # ---- button hints (one or two words each: a row holds up to five) ----
    "Choose": "Button hint: take the highlighted time control, or the highlighted option.",
    "Mode": "Button hint for L1 and R1: go to the next way to play (Online, Computer, Friend).",
    "Move": "Button hint for the directional buttons: move the highlight.",
    "Resume": "Button hint: open the highlighted game in progress again.",
    "Hold to sign out": "Button hint, and what the Sign out button says after a short press: it must "
                        "be held down. About 20 letters.",
    "Use a token": "Button hint: sign in by typing a personal token instead of scanning the code.",
    "Type": "Button hint on the token keyboard: type the highlighted key.",
    "Delete": "Button hint on the token keyboard: delete the last character.",
    "Shift": "Button hint on the token keyboard: capital letters.",
    "Done": "Button hint and key of the token keyboard: the token is typed, check it. One short word.",
    "QR code": "Button hint: go back from the token keyboard to the code that a phone scans.",
    "Continue": "Button hint and label: close the screen that says signing in worked.",
    "Try again": "Button hint and label: start signing in again after it failed.",

    # ---- Play: the three ways to play ----
    "way to play\x04Online": "Tab of the Play page: games against other players on Lichess. "
                             "About 20 letters.",
    "Computer": "Tab of the Play page: games against Stockfish. About 20 letters.",
    "Friend": "Tab of the Play page: two players on this console. About 20 letters.",
    "Rapid, classical, correspondence": "Under the Online tab: the speeds on offer, in lower case "
                                        "after the first. About 35 letters.",
    "Stockfish plays on lichess.org": "Under the Computer tab. About 35 letters.",
    "Two players, no account needed": "Under the Friend tab. About 35 letters.",
    "Play": "The Play page's name; also the button and the button hint that start a game. "
            "About 12 letters.",

    # ---- Play: time controls ----
    "Bullet": SPEED,
    "Blitz": SPEED,
    "Rapid": SPEED,
    "Classical": SPEED,
    "Correspondence": SPEED,
    "{0} day": "A correspondence time control, as a large figure on a tile: \"1 day\". Very short.",
    "{0} days": "A correspondence time control, as a large figure on a tile: \"3 days\". "
                "{0} is 2 or more. Very short.",
    "Unlimited": "A time control: a game against the computer without a clock. A large figure on a "
                 "tile: one word.",
    "+{0} s per move": "On a time control's tile: the seconds added to the clock after each move "
                       "(\"s\" is seconds). About 16 characters.",
    "No increment": "On a time control's tile: no seconds are added after a move. About 16 letters.",
    "Per move": "On the tile \"3 days\": the days are for every single move. About 16 letters.",
    "No clock": "A game without a clock. About 16 letters.",
    "{0} minutes each": "The chosen time control in words: each player has {0} minutes (5 or more).",
    "{0} minutes each, +{1} s per move": "The chosen time control in words: each player has {0} "
                                         "minutes (5 or more), and {1} seconds are added after "
                                         "each move. About 40 characters.",
    "{0} day for every move": "The chosen correspondence time control in words; {0} is 1.",
    "{0} days for every move": "The chosen correspondence time control in words; {0} is 2 or more.",
    "speed, time control\x04{0} {1}": "What a search is for: {0} is the speed (Rapid), {1} the time "
                                      "control (10+5). Change the order if the language asks for it.",

    # ---- Play: the ticket for the next game ----
    "Next game": "Heading of the panel that shows what is about to be played. About 16 letters.",
    "Rated": "The game counts for the rating. A choice, a tag in capitals and a word after a speed "
             "(\"Rapid · Rated\"). About 12 letters.",
    "Casual": "The game does not count for the rating: the opposite of Rated. About 12 letters.",
    "The result changes your rating": "Under the choice Rated. About 36 letters.",
    "Play without rating points": "Under the choice Casual. About 36 letters.",
    "Your bullet rating": "Heading over the player's rating at this speed. In capitals on screen. "
                          "About 28 letters.",
    "Your blitz rating": "Heading over the player's rating at this speed. In capitals on screen. "
                         "About 28 letters.",
    "Your rapid rating": "Heading over the player's rating at this speed. In capitals on screen. "
                         "About 28 letters.",
    "Your classical rating": "Heading over the player's rating at this speed. In capitals on "
                             "screen. About 28 letters.",
    "Your correspondence rating": "Heading over the player's rating at this speed. In capitals on "
                                  "screen. About 28 letters.",
    "{0} game": "How many games a rating rests on; {0} is 1.",
    "{0} games": "How many games a rating rests on; {0} is a number such as 412 or 1,204. "
                 "About 14 characters.",
    "No games yet": "In place of the number of games: none were played at this speed.",
    "No rating yet": "In place of a rating the account does not have. About 20 letters.",
    "Your first games at this speed set it.": "Under \"No rating yet\": \"it\" is the rating. "
                                              "One line of about 45 letters.",
    "Find opponent": "Button and button hint: ask Lichess for an opponent. About 18 letters.",
    "Stockfish level {0}": "The computer opponent; {0} is its strength, 1 to 8.",
    "Level {0}": "A strength of the computer; {0} is 1 to 8.",
    "Strength": "Heading over the computer's level. About 18 letters.",
    "of {0}": "Stands after the chosen level, a large figure: \"3 of 8\". {0} is the highest level. "
              "Very short: about 5 characters.",
    "From 1, the gentlest, to 8, the strongest": "Under the computer's level. One line of about "
                                                 "45 letters.",
    "Your colour": "Heading over the choice of side against the computer. About 24 letters.",
    "Random": "A choice of side against the computer: Lichess picks White or Black. About 14 letters.",
    "White": "The side with the white pieces. About 14 letters.",
    "Black": "The side with the black pieces. About 14 letters.",
    "Lichess picks your side": "Beside the choice Random. About 24 letters.",
    "You move first": "Beside the choice White. About 24 letters.",
    "Stockfish moves first": "Beside the choice Black. About 24 letters.",
    "a random side": "After the speed and a dot, in lower case: \"Rapid · a random side\".",
    "you play White": "After the speed and a dot, in lower case: \"Rapid · you play White\".",
    "you play Black": "After the speed and a dot, in lower case: \"Rapid · you play Black\".",

    # ---- Play: two players on this console, and tabs that cannot be used ----
    "Pass & Play": "The name of the game for two players who share one controller. A large title "
                   "and a button: about 14 letters.",
    "On this console": "Heading over \"Pass & Play\". In capitals on screen.",
    "move number\x04Move": "Small heading over the number of the move a saved game is at "
                           "(\"Move 5\"). About 10 letters.",
    "Players": "Small heading over the figure 2. About 12 letters.",
    "Controllers": "Small heading over the figure 1: one controller is enough. About 12 letters.",
    "Internet": "Small heading over \"Not needed\". About 12 letters.",
    "Not needed": "Under \"Internet\": the game works without a connection. About 14 letters.",
    "To move": "Under White or Black: it is this side's turn. About 18 letters.",
    "Waiting": "Under White or Black: this side waits for the other to move. About 18 letters.",
    "Moves first": "Under White, before a game starts. About 18 letters.",
    "Replies": "Under Black, before a game starts: Black answers White's first move. "
               "About 18 letters.",
    "A game is in progress. Its menu starts a new one.": "Over the button Resume game. One line of "
                                                         "about 60 letters.",
    "Resume game": "Button: go on with the saved game. About 16 letters.",
    "New game": "Button: start a game. About 16 letters.",
    "Two players share one controller and take turns. The board turns to face whoever is to move, "
    "and the game is kept when you leave.": "Under \"Pass & Play\". At most four lines of about 50 "
                                            "letters.",
    "No connection": "Heading: the console is not connected to the internet. In capitals on screen.",
    "Finding an opponent needs the internet connection. Pass & Play works without it.":
        "\"Pass & Play\" is the game for two players on this console: word it as in its own entry.",
    "Stockfish plays on lichess.org, which needs the internet connection. Pass & Play works "
    "without it.": "\"Pass & Play\" is the game for two players on this console: word it as in its "
                   "own entry.",
    "You are offline": "Large title of at most two lines, each of about 20 letters.",
    "Sign in to play online": "Large title of at most two lines, each of about 20 letters.",
    "Sign in to play the computer": "Large title of at most two lines, each of about 20 letters.",
    "Signing in": "Large title while the account is being checked. About 16 letters.",
    "Sign in": "Button and button hint: open the screen where one signs in to Lichess. "
               "About 14 letters.",
    "Lichess account": "Heading. In capitals on screen. About 24 letters.",

    # ---- Profile ----
    "Profile": "The Profile page's name. About 12 letters.",
    "Games played": "Small heading over a number of games. In capitals on screen: about 14 letters.",
    "Time played": "Small heading over the time spent playing. In capitals on screen: about 14 "
                   "letters.",
    "{0}d {1}h": "Time spent playing: {0} days and {1} hours, each unit as one letter. A large "
                 "figure: about 8 characters.",
    "{0}h {1}m": "Time spent playing: {0} hours and {1} minutes, each unit as one letter. A large "
                 "figure: about 8 characters.",
    "{0}m": "Time spent playing: {0} minutes, the unit as one letter.",
    "share of games\x04Won": "Under a percentage in the middle of a ring: the share of games the "
                             "player won. In capitals on screen: about 6 letters.",
    "No games": "In the middle of a ring, in place of a percentage: the account has no games. "
                "In capitals on screen, on one line or two: words of 8 letters at most.",
    "{0} won": "Legend of the results: {0} is a number of games (\"1,301 won\"). Lower case, the "
               "word about 7 letters. For an account without games the line stands without {0}.",
    "{0} drawn": "Legend of the results: {0} is a number of games (\"243 drawn\"). Lower case, the "
                 "word about 7 letters. For an account without games the line stands without {0}.",
    "{0} lost": "Legend of the results: {0} is a number of games (\"1,122 lost\"). Lower case, the "
                "word about 7 letters. For an account without games the line stands without {0}.",
    "Puzzles": "Name of the puzzle rating, beside the speeds. In capitals on a tile: about 15 "
               "letters.",
    "{0} solved": "Under the puzzle rating: {0} is the number of puzzles solved (\"1,960 solved\"). "
                  "Word it so that it reads well with any number.",
    "provisional": "After the number of games and a dot, in lower case: Lichess is not yet sure of "
                   "this rating (it shows a question mark). \"9 games · provisional\".",
    "Sign out": "Button: remove the account from this console. It has to be held down. "
                "About 14 letters.",
    "Signed out": "A short message at the top after signing out.",
    "Games in progress": "Heading over the player's unfinished games, and the title of a tile. "
                         "About 24 letters.",
    "Your move": "Over a game in which it is the player's turn. In capitals on screen, on one line "
                 "or two: words of 8 letters at most.",
    "Their move": "Over a game in which it is the opponent's turn. In capitals on screen, on one "
                  "line or two: words of 8 letters at most.",
    "{0}d {1}h left": "What a game's clock still holds: {0} days and {1} hours, each unit as one "
                      "letter. About 11 characters.",
    "{0}h {1}m left": "What a game's clock still holds: {0} hours and {1} minutes, each unit as "
                      "one letter. About 11 characters.",
    "{0} left": "What a game's clock still holds: {0} is minutes and seconds, such as 12:05. "
                "About 11 characters.",
    "Rating {0}": "The opponent's rating: {0} is a number such as 1871. About 12 characters.",
    "vs {0}": "A game's title: {0} is the opponent's name, or \"Stockfish level 3\". "
              "\"vs\" is short for versus.",
    "No games in progress": "Large title where the games in progress would be. About 35 letters.",
    "Sign in to Lichess": "Large title of the Profile page while nobody is signed in. One line of "
                          "about 24 letters.",
    "Fetching your account from Lichess": "While the account is being checked. No full stop.",
    "Start a game and it waits for you here, on the console and on every other device you play "
    "on.": "Under \"No games in progress\". At most three lines of about 65 letters.",
    "Play rated puzzles and online games, and pick up your games in progress on this console.":
        "Under \"Sign in to Lichess\". At most three lines of about 55 letters.",
    "Rated puzzles": "Title of a tile: what an account brings. About 24 letters.",
    "Online games": "Title of a tile: what an account brings. About 24 letters.",
    "Solve the daily puzzle and train with puzzles that count for your rating.":
        "Under \"Rated puzzles\". At most three lines of about 36 letters.",
    "Play rated and casual games against players from all over the world.":
        "Under \"Online games\". At most three lines of about 36 letters.",
    "Pick up the games you have going, on the console and everywhere else.":
        "Under \"Games in progress\". At most three lines of about 36 letters.",

    # ---- signing in ----
    "Sign in\nto Lichess": "Large title on two lines; \\n is where the line breaks (without one the "
                           "text breaks where the column ends). Each line about 12 letters.",
    "step\x04Scan": "First step of signing in with a phone: scan the code. A short heading: about "
                    "16 letters.",
    "step\x04Authorize": "Second step of signing in with a phone. Lichess has a button with this "
                         "name: use the word lichess.org uses for it. About 16 letters.",
    "step\x04Done": "Last step of signing in: it is finished. About 16 letters.",
    "step\x04Create": "First step of signing in with a token: create one. About 16 letters.",
    "step\x04Type": "Second step of signing in with a token: type it. About 16 letters.",
    "With your phone": "Under the step Scan. About 30 letters.",
    "At lichess.org": "Under the step Authorize. About 30 letters.",
    "Back on the console": "Under the step Done. About 30 letters.",
    "A token at lichess.org": "Under the step Create. About 30 letters.",
    "The token, here": "Under the step Type. About 30 letters.",
    "Scan the code with your phone": "Heading beside the QR code. One line of about 32 letters is "
                                     "best; longer text takes two.",
    "Sign in to Lichess and press Authorize. Lichess will warn that the address is not secure: it "
    "is this console on your home network.": "\"Authorize\" is a button of lichess.org: use the "
                                             "word it has there.",
    "Your phone shows \"Signed in\" and ProsperoLichess continues by itself.":
        "\"Signed in\" is the start of what the phone shows (\"Signed in to ProsperoLichess\"): "
        "word the two alike.",
    "Signed in to ProsperoLichess": "Heading of the web page the phone shows when signing in "
                                    "worked.",
    "You can put your phone away and return to your PS5.": "On the web page the phone shows when "
                                                           "signing in worked.",
    "Waiting for your phone": "Over the time left to scan the code. In capitals on screen: about "
                              "30 letters.",
    "Console address": "Small heading over the console's network address. In capitals on screen.",
    "Use a token instead": "Button: sign in by typing a personal token. About 40 letters.",
    "Create a token": "Heading under a QR code that leads to lichess.org. About 20 letters.",
    "Personal token": "Label of the field the token is typed into. A token is a long password "
                      "Lichess makes for an app.",
    "Starts with {0}": "Under the token field: {0} is lip_, the letters every Lichess token begins "
                       "with.",
    "{0} character typed": "Under the token field, after a dot; {0} is 1.",
    "{0} characters typed": "Under the token field, after a dot; {0} is a number.",
    "A token has at least {0} characters": "Error under the token field; {0} is 8.",
    "Sign-in did not finish": "Large title after signing in failed. About 35 letters.",
    "Signed in as {0}": "Large title and short message; {0} is the player's name.",
    "Your ratings and your games in progress are on the Profile page, and signing out is there "
    "too.": "\"Profile\" is a page's name: word it as in its own entry.",
    "Lichess did not accept the sign-in ({0}).": "{0} is the number of the error Lichess answered "
                                                 "with, such as 401.",
    "Lichess did not issue a token ({0}).": "{0} is the number of the error Lichess answered with, "
                                            "such as 400.",
    "Lichess reported an error: {0}": "{0} is the error's name in English, as Lichess sent it.",
    "Your phone approved it. Fetching your account": "While waiting; no full stop at the end.",
    "Checking the token with Lichess": "While waiting; no full stop at the end.",
    "Rated puzzles, online games and your games in progress, on this console":
        "Under the title \"Sign in to Lichess\"; no full stop at the end.",
}
