# ProsperoLichess - Notes for translators: the game screens.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# English text -> a note, for the game screen (online games and Pass & Play),
# its menu, its result and the screen that waits for a game. For a text in a
# context the key is "context\x04Text". See notes.py.

NOTES = {
    # ---- the two sides ----
    "side\x04White": "The side that has the white pieces: the name of a Pass & Play player, the "
                     "head of the column of White's moves, and the {0} of a player's note "
                     "(\"White · your move\").",
    "side\x04Black": "The side that has the black pieces: the name of a Pass & Play player, the "
                     "head of the column of Black's moves, and the {0} of a player's note "
                     "(\"Black · thinking\").",

    # ---- the band over the moves: what is happening (one short line) ----
    "Your move": "Game screen, state line: it is the player's turn. Short (about 20 letters).",
    "Their move": "Game screen, state line: it is the opponent's turn. Short (about 20 letters).",
    "You are in check": "Game screen, state line: the player's king is in check.",
    "White to move": "Pass & Play, state line: it is White's turn.",
    "Black to move": "Pass & Play, state line: it is Black's turn.",
    "White is in check": "Pass & Play, state line: White's king is in check.",
    "Black is in check": "Pass & Play, state line: Black's king is in check.",
    "Looking back": "Game screen, state line: an earlier position of the game is on the board.",
    "Not started": "The game has not started yet: the state line, and the plate where resigning "
                   "will be. About 18 letters.",
    "Reconnecting…": "Game screen, state line: the connection to the game dropped and is "
                          "being made again.",
    "Waiting for the first move": "In the empty list of moves of an online game.",
    "White moves first": "In the empty list of moves of a Pass & Play game.",

    # ---- the line under a player's name ----
    "player\x04Waiting": "Pass & Play, under a player's name: it is the other player's turn.",
    "To move": "Pass & Play, under a player's name: it is this player's turn.",
    "In check": "Pass & Play, under a player's name: this player's king is in check.",
    "Won": "Beside or under a player's name once the game is over: this player won. One short "
           "word (about 10 letters).",
    "Lost": "Beside or under a player's name once the game is over: this player lost. One short "
            "word (about 10 letters).",
    "player\x04Draw": "Beside or under a player's name once the game is over: this player drew. "
                      "One short word (about 10 letters), in the form of \"Won\" and \"Lost\".",
    "{0} · your move": "Under the player's own name. {0} is their side (\"White\", \"Black\"). "
                            "Keep the dot.",
    "{0} · in check": "Under a player's name: their king is in check. {0} is their side "
                           "(\"White\", \"Black\", or \"Black · level 3\" for the computer).",
    "{0} · thinking": "Under the opponent's name: it is their turn. {0} is their side "
                           "(\"White\", \"Black\", or \"Black · level 3\" for the computer).",
    "{0} · won": "Under a player's name once the game is over: this player won. {0} is "
                      "their side.",
    "{0} · draw": "Under a player's name once the game is over: the game was drawn. {0} is "
                       "their side.",
    "{0} · reconnecting…": "Under the player's own name while the connection to the "
                                     "game is being made again. {0} is their side.",
    "{0} · left, claim the win in {1} s": "Under the opponent's name: they left the game. "
                                               "{0} is their side, {1} the seconds until the "
                                               "player may claim the win. About 40 letters.",
    "{0} · left, claim the win in the menu": "Under the opponent's name: they left, and "
                                                  "the game menu now has \"Claim victory\". {0} "
                                                  "is their side. About 40 letters.",
    "{0} · level {1}": "Under the computer's name (Stockfish). {0} is its side (\"White\", "
                            "\"Black\"), {1} its strength, 1 to 8.",
    "level {0}": "Result card, under the computer's name (Stockfish). {0} is its strength, 1 to "
                 "8. Lower case.",

    # ---- the row of actions (plates beside each other: short labels) ----
    "Undo": "Pass & Play, action plate: take the last move back. About 14 letters.",
    "Flip board": "Action plate and menu row: turn the board to see it from the other side. "
                  "About 14 letters.",
    "Offer draw": "Action plate and menu row: propose a draw to the opponent. About 14 letters.",
    "Draw offered": "Action plate, in place of \"Offer draw\" while the player's draw offer "
                    "waits for an answer. About 14 letters.",
    "Takeback": "Action plate: ask the opponent to let the player take the last move back. "
                "About 14 letters.",
    "Takeback asked": "Action plate, in place of \"Takeback\" while the player's request waits "
                      "for an answer. About 14 letters.",
    "Result": "Action plate, once the game is over: show the result card again. About 14 "
              "letters.",
    "Hold to resign": "The plate beside the actions: hold the Square button to give the game "
                      "up. About 18 letters.",
    "Hold to abort": "The plate beside the actions, before both sides have moved: hold the "
                     "Square button to call the game off (it is not scored). About 18 letters.",
    "Resign": "The plate beside the actions when one press is enough: give the game up. A verb.",
    "Abort": "The plate beside the actions when one press is enough, before both sides have "
             "moved: call the game off (it is not scored). A verb.",
    "Keep holding": "On the resign plate, for a moment after the Square button was only "
                    "tapped. About 18 letters.",
    "Game over": "On the plate where resigning was, once the game has ended. About 18 letters.",

    # ---- stepping through the moves ----
    "Review": "Small label in capitals before the position being looked at (\"REVIEW  After "
              "12. Nxd4\"). A noun, about 12 letters.",
    "After {0}": "Which position is on the board while stepping through the moves. {0} is a "
                 "move with its number: \"12. Nxd4\", \"12… Nf6\".",
    "Starting position": "The position before the first move, while stepping through the moves.",

    # ---- an offer from the opponent ----
    "Your opponent offers a draw": "Title of the card that asks the player to answer.",
    "Your opponent asks for a takeback": "Title of the card that asks the player to answer: "
                                         "the opponent wants to take their last move back.",
    "Accept and the game ends in a draw.": "Under the title of the card with the opponent's "
                                           "draw offer. One line, about 55 letters.",
    "Accept and their last move is taken back.": "Under the title of the card with the "
                                                 "opponent's takeback request. One line, about "
                                                 "55 letters.",
    "Accept": "Answer to a draw offer or a takeback request: agree. About 16 letters.",
    "Decline": "Answer to a draw offer or a takeback request, and the button hint for it: "
               "refuse. About 16 letters.",
    "Draw offer sent": "Short message after the player offered a draw.",
    "Takeback proposal sent": "Short message after the player asked to take a move back.",
    "Move refused": "Short message: Lichess did not accept the player's move.",
    "Your opponent left. Claim victory from the menu.": "Short message. \"Claim victory\" is a "
                                                        "row of the game menu: use the same "
                                                        "words.",

    # ---- the result ----
    "Victory": "Result card, the large title: the player won. One or two words.",
    "Defeat": "Result card, the large title: the player lost. One or two words.",
    "result\x04Draw": "The game's result, as the large title of the result card and in the state "
                      "line: nobody won. One or two words.",
    "Game aborted": "Result card, the large title: the game was called off before it began.",
    "White wins": "Pass & Play, the large title of the result card and the state line.",
    "Black wins": "Pass & Play, the large title of the result card and the state line.",
    "Checkmate": "Result card, how the game ended.",
    "Stalemate": "Result card, how the game ended: the side to move has no legal move and is "
                 "not in check (a draw).",
    "Resignation": "Result card, how the game ended: a player gave up.",
    "White resigned": "Result card, how the game ended.",
    "Black resigned": "Result card, how the game ended.",
    "Time out": "Result card, how the game ended: a player's clock ran out.",
    "Insufficient material": "Result card, how the game ended: neither side has the pieces to "
                             "give mate (a draw).",
    "Fifty-move rule": "Result card, how the game ended: fifty moves without a capture or a "
                       "pawn move (a draw).",
    "Threefold repetition": "Result card, how the game ended: the same position three times "
                            "(a draw).",
    "Draw agreed or claimed": "Result card, how the game ended.",
    "The opponent left the game": "Result card, how the game ended.",
    "The game was aborted": "Result card, how the game ended: called off before it began.",
    "The game did not start in time": "Result card, how the game ended: a player did not make "
                                      "the first move.",
    "Cheat detected": "Result card, how the game ended: Lichess found that a player cheated.",
    "{0} move": "Result card, how long the game was, for exactly one move: \"Checkmate · 1 "
                "move\".",
    "{0} moves": "Result card, how long the game was: \"Checkmate · 34 moves\". {0} is the "
                 "number of moves.",
    "New game": "Button on the result card and row of the game menu (Pass & Play): start "
                "again. About 16 letters.",
    "Review game": "Button on the result card: step through the moves of the finished game. "
                   "About 16 letters.",
    "button\x04Menu": "Button on the result card: leave the game and go back to the app's "
                      "menu. About 16 letters.",

    # ---- the game menu ----
    "Game menu": "Small label in capitals over the menu that the Options button opens during "
                 "a game.",
    "vs {0}": "Title of the game menu. {0} is the opponent's name.",
    "Pass & Play": "The name of the mode in which two players share one controller: the title "
                   "of its menu and the label of its game.",
    "Two players, one controller": "Under the title \"Pass & Play\".",
    "The game is over": "Under the game menu's title.",
    "Your clock keeps running while you are here": "Under the game menu's title: the menu does "
                                                   "not pause an online game.",
    "Resume": "Row of the game menu and button hint: close the menu and go on playing.",
    "Abort game": "Row of the game menu, before both sides have moved: call the game off (it "
                  "is not scored).",
    "Propose takeback": "Row of the game menu: ask the opponent to let the player take the "
                        "last move back.",
    "Claim victory": "Row of the game menu: the opponent left, take the win.",
    "Return to menu": "Row of the game menu: leave the game screen for the app's menu.",
    "Take back move": "Row of the game menu (Pass & Play): undo the last move.",
    "Auto-flip": "Row of the game menu (Pass & Play), a switch: the board turns to the player "
                 "whose move it is.",
    "On": "The state of a switch in a menu.",
    "Off": "The state of a switch in a menu.",

    # ---- button hints (the row at the bottom: one short word each) ----
    "Choose": "Button hint for the directional buttons: go from one button or answer to the "
              "next. About 10 letters.",
    "Move": "Button hint for the directional buttons: move the cursor over the board. A verb, "
            "not a chess move. About 10 letters.",
    "Place": "Button hint: pick up the piece under the cursor, or put it down there. A verb. "
             "About 10 letters.",
    "Cancel": "Button hint: put the piece back, or stop waiting for a game. About 10 letters.",
    "Actions": "Button hint: go to the row of action plates under the moves. About 10 letters.",
    "Board": "Button hint: go back from the action plates to the board. About 10 letters.",
    "Step": "Button hint for L2 and R2: one move back or forward through the game. About 10 "
            "letters.",
    "History": "Button hint for L2 and R2: look at the earlier moves of the game. About 10 "
               "letters.",
    "Back to game": "Button hint, while an earlier position shows: return to the current one. "
                    "About 14 letters.",

    # ---- a game that cannot be shown ----
    "Connecting to the game": "On the empty board while an online game is being loaded.",
    "Lichess game": "Small label in capitals over \"Could not open the game\".",
    "Could not open the game": "Title of the panel that says why a game cannot be shown.",
    "The game is not available.": "Why a game cannot be shown, when nothing more is known.",
    "Could not follow the game's moves": "Short message: the moves Lichess sent could not be "
                                         "played on the board.",
    "Connection problem": "Why a move or a request did not reach Lichess.",
    "Lichess refused the request": "Why a game or a request failed, when Lichess gave no "
                                   "reason.",
    "Connection to the game lost. Reconnecting…": "Notice at the top of the screen during "
                                                       "an online game.",
    "Lichess is not answering. Still trying; press Circle to go back.": "Why a game cannot be "
                                                                        "shown. Circle is the "
                                                                        "controller's button.",

    # ---- waiting for a game ----
    "Lichess pairing": "Small label in capitals on the waiting screen: Lichess is looking for "
                       "an opponent. About 25 letters.",
    "Rated": "A game that counts for the players' ratings. Also a small tag in capitals: about "
             "12 letters.",
    "Casual": "A game that does not count for the players' ratings. Also a small tag in "
              "capitals: about 12 letters.",
    "Bullet": "The name of a speed of play, as lichess.org writes it in this language.",
    "Blitz": "The name of a speed of play, as lichess.org writes it in this language.",
    "Rapid": "The name of a speed of play, as lichess.org writes it in this language.",
    "Classical": "The name of a speed of play, as lichess.org writes it in this language.",
    "Correspondence": "The name of a speed of play (days for each move), as lichess.org "
                      "writes it in this language.",
    "UltraBullet": "The name of a speed of play, as lichess.org writes it in this language.",
    "{0} day": "The time for each move of a correspondence game, for exactly one day.",
    "{0} days": "The time for each move of a correspondence game: \"3 days\". {0} is the "
                "number of days.",
    "Unlimited": "The time control of a game without a clock. Large on the waiting screen: "
                 "about 12 letters.",
    "Level {0}": "Small label in capitals on the waiting screen: the strength of the computer "
                 "(Stockfish). {0} is 1 to 8.",
    "Finding an opponent": "Waiting screen, what is happening. About 22 letters.",
    "Setting up the board": "Waiting screen, what is happening before a game against the "
                            "computer. About 22 letters.",
    "Waiting in the lobby": "Waiting screen, what is happening: a correspondence game was "
                            "offered and waits for someone to take it. About 22 letters.",
    "timer\x04Waiting": "Small label in capitals over the time spent waiting for a game "
                        "(\"WAITING 0:42\").",
    "Could not start the game": "Title of the panel that says why no game began.",
    "No connection to Lichess": "Why no game began.",
    "Sign in to Lichess first: games are played on your account.": "Why no game began: the "
                                                                   "player is signed out.",
    "Lichess did not create the game": "Why no game began.",
    "Lichess asked us to slow down. Try again in a minute.": "Why no game began: too many "
                                                             "requests were sent.",
}
