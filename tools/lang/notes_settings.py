# ProsperoLichess - What a translator cannot tell from a text alone: settings, Watch, Lichess TV.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# English text -> a note, for the settings page, the Watch page, the Lichess TV screen and the
# words the interface kit draws by itself. tools/strings.py writes each note above its text in
# the template and in every catalog. A text in a context has the key "context\x04Text".

NOTES = {
    # ---- button hints ----
    "Back": "Button hint: go back one screen. Also the word under the button that goes back, in "
            "the controller settings: about 12 letters.",
    "Open": "Button hint: open the highlighted item.",
    "Choose": "Button hint: move between the items.",
    "Change": "Button hint (a verb): change the highlighted setting's value.",
    "Scroll": "Button hint (a verb): scroll the About text.",
    "Channel": "Button hint: move to another Lichess TV channel.",
    "Flip board": "Button hint: turn the board to see it from the other side.",
    "Watch": "The Watch page's name (Lichess TV), and the button hint that opens the game on the "
             "full screen. One word for both.",

    # ---- settings: the six groups ----
    "Settings": "The settings page's name.",
    "Board": "Settings: the group about how the board looks, and the row that chooses the "
             "board. About 14 letters.",
    "settings\x04Play": "Settings: the group about how moves are made (a noun, as in \"how you "
                        "play\"), not the page that starts a game. About 14 letters.",
    "Sound": "Settings: the group with the volumes. About 14 letters.",
    "Controller": "Settings: the group about the controller's buttons and its vibration. About "
                  "14 letters.",
    "Display": "Settings: the group about the picture (a noun: the screen). About 14 letters.",
    "About": "Settings: the group that says what the app is and whose work it uses. About 14 "
             "letters.",
    "Changes apply at once and are saved for this console.": "Settings: under the list of "
                                                             "groups. Three short lines at most.",

    # ---- settings: rows. A row's name is one line of about 24 letters, the line under it one
    # line of about 55 letters; longer ones are set smaller.
    "Appearance": "Settings, Board: the heading over the board and the piece set.",
    "Pieces": "Settings: the row that chooses the piece set. Also a row of the credits.",
    "On the board": "Settings, Board: the heading over what is drawn on the board.",
    "Coordinates": "Settings: the letters and numbers along the board's edge (a to h, 1 to 8).",
    "Legal moves": "Settings: dots on the squares the selected piece may move to.",
    "Moves": "Settings, Play: the heading over how moves are made.",
    "Always promote to queen": "Settings: a pawn that reaches the last rank becomes a queen "
                               "without asking.",
    "Off shows a picker for every promotion.": "\"Off\" is the switch's state, as the text Off.",
    "Premoves": "Settings: Lichess's word for a move entered during the opponent's turn. Use "
                "the word lichess.org uses.",
    "Online games": "Settings, Play: the heading over what concerns games on Lichess.",
    "Volume": "Settings, Sound: the heading over the three volumes.",
    "Game sounds": "Settings: the volume of moves, captures and results. Also a meter's name in "
                   "the picture beside: about 20 letters.",
    "Interface sounds": "Settings: the volume of the menus' sounds.",
    "Interface": "Settings, Sound: the name of the meter of the menus' sounds, in small "
                 "capitals. About 20 letters.",
    "Music": "Settings: the music's volume, and its meter's name in the picture beside.",
    "Levels": "Settings, Sound: the title of the picture with the three volume meters.",
    "Buttons": "Settings, Controller: the heading over the buttons' roles, and the title of the "
               "picture beside.",
    "Swap Cross and Circle": "Cross and Circle are the PS5 controller's buttons: name them as "
                             "PlayStation does in this language.",
    "Circle confirms and Cross goes back.": "Cross and Circle are the PS5 controller's buttons.",
    "Confirm": "Settings, Controller: the word under the button that confirms, in small "
               "capitals on a tag. About 12 letters.",
    "Feedback": "Settings, Controller: the heading over what the controller lets the player "
                "feel (vibration). Not an opinion sent to someone.",
    "Vibration on": "Settings: under the picture of the controller. About 30 letters.",
    "Vibration off": "Settings: under the picture of the controller. About 30 letters.",
    "Picture": "Settings, Display: the heading over the picture's size and the frame counter, "
               "and the title of the drawing beside.",
    "Resolution": "Settings: the picture's size (1080p, 1440p, 4K).",
    "Show FPS": "Settings: FPS is frames per second; keep the letters FPS.",
    "Motion": "Settings, Display: the heading over how much the interface moves, and a label "
              "in small capitals in the drawing beside (about 10 letters).",
    "Reduced motion": "Settings: fewer animations.",
    "Quick fades": "Settings, Display: what the interface does with reduced motion on. About "
                   "20 letters.",
    "Slides and springs": "Settings, Display: what the interface does with reduced motion off. "
                          "About 20 letters.",
    "Preview": "Settings: the title of the board that shows the chosen look, in small capitals. "
               "About 12 letters.",
    "On": "A switch's state. Short: 3 or 4 letters.",
    "Off": "A switch's state. Short: 3 or 4 letters.",

    # ---- settings: what boards and piece sets are called (about 12 letters) ----
    "Walnut": "A board's name: a wooden board, dark brown.",
    "Maple": "A board's name: a wooden board, light.",
    "Marble": "A board's name: a board of stone.",
    "Classic": "A board's name: flat brown squares.",
    "Ocean": "A board's name: flat blue squares.",
    "Meadow": "A board's name: flat green squares.",
    "Twilight": "A board's name: violet stone.",
    "piece set\x04Classic": "A piece set's name: the usual pieces of lichess.org. The other sets "
                            "(Merida, Chessnut) keep their names.",

    # ---- settings: About ----
    "Brought to you by": "About: before the author's name.",
    "Version": "About: before the app's version number.",
    "Unknown": "About: shown in place of the version when it is not known.",
    "Licence": "About: before the name of the app's licence.",
    "Source": "About: before the web address of the app's source code.",
    "Powered by Lichess": "About: a heading. Lichess stays as it is.",
    "Also thanks to": "About: the heading over the credits.",
    "Puzzles": "The Puzzles page's name. Also a row of the credits (the puzzles' source).",
    "Lichess puzzle database (CC0)": "Credits. CC0 is a licence's name: keep it.",
    "The interface kit this app is drawn with": "Credits: what the project ps5-homebrew-ui is.",
    "QR codes": "Credits: the row about the code a phone scans.",
    "Sound and music": "Credits: the row about the app's sounds.",
    "{0} and {1} (GPLv2+), {2} (Apache-2.0)": "Credits: {0}, {1} and {2} are piece sets' names. "
                                              "Keep the licences' names in brackets.",
    "{0} for PS5, built on {1}": "Credits: {0} is \"OpenGL 4.6\", {1} is the project \"Mesa\".",
    "Typeface by {0} (SIL OFL 1.1)": "Credits: {0} is a person or a group. Keep the licence's "
                                     "name in brackets.",
    "Typeface by {0} and {1} (Bitstream Vera)": "Credits: {0} and {1} are names. Keep the "
                                                "licence's name in brackets.",
    "Music decoding by {0}": "Credits: {0} is a person's name.",
    "JSON parsing by {0} (MIT)": "Credits: {0} is a person's name. JSON and MIT stay.",
    "HTTPS by {0} and contributors (curl)": "Credits: {0} is a person's name. HTTPS and curl "
                                            "stay.",
    "TLS by the {0} (Apache-2.0)": "Credits: {0} is \"OpenSSL Project\". TLS and the licence's "
                                   "name stay.",
    "Compression and domain rules for {0} (zlib, BSD, MIT)": "Credits: {0} is \"libcurl\". Keep "
                                                             "the licences' names in brackets.",
    "QR Code generator by {0} (MIT)": "Credits: {0} is \"Project Nayuki\". \"QR Code generator\" "
                                      "is what the library does; MIT stays.",
    "Created with {0}": "Credits: {0} is \"ElevenLabs\", the service the sounds were made with.",
    "The full notices and licence texts ship with the source, in {0}.": "{0} is a file's name.",

    # ---- Watch and Lichess TV ----
    "Top rated": "A Lichess TV channel: the best game being played. About 14 letters; use "
                 "lichess.org's word.",
    "Blitz": "A Lichess TV channel and a speed of play: use lichess.org's word. About 14 "
             "letters.",
    "Rapid": "A Lichess TV channel and a speed of play: use lichess.org's word. About 14 "
             "letters.",
    "Classical": "A Lichess TV channel and a speed of play: use lichess.org's word. About 14 "
                 "letters.",
    "Bullet": "A Lichess TV channel and a speed of play: use lichess.org's word. About 14 "
              "letters.",
    "Best game right now": "Under the channel \"Top rated\". About 25 letters.",
    "3 to 8 minutes": "Under the channel Blitz: how long a player has. About 25 letters.",
    "8 to 25 minutes": "Under the channel Rapid: how long a player has. About 25 letters.",
    "25 minutes and more": "Under the channel Classical: how long a player has. About 25 "
                           "letters.",
    "Under 3 minutes": "Under the channel Bullet: how long a player has. About 25 letters.",
    "Shuffled back rank": "Under the channel Chess960: the pieces of the first rank start in "
                          "another order. About 25 letters.",
    "On air": "Watch: the game shown is being played right now. Small capitals; about 16 "
              "letters.",
    "Live": "Lichess TV: a tag beside the title, the game is being played right now. About 12 "
            "letters.",
    "Tuning in": "Watch and Lichess TV: the channel's game is being fetched. Small capitals; "
                 "about 16 letters.",
    "Reconnecting": "Watch and Lichess TV: the connection dropped and is being made again. "
                    "Small capitals; about 16 letters.",
    "Last move": "A label over the last move played (\"Nxd4\"). Small capitals; about 14 "
                 "letters.",
    "Average rating": "Watch: a label over the average of the two players' ratings. Small "
                      "capitals; about 18 letters.",
    "Material": "A label: what each side's pieces are worth. Small capitals; about 14 letters.",
    "Even": "Beside Material: both sides' pieces are worth the same. About 10 letters.",
    "White {0}": "Under the material bar: {0} is what White's pieces are worth, in pawns "
                 "(\"White 38\").",
    "Black {0}": "Under the material bar: {0} is what Black's pieces are worth, in pawns "
                 "(\"Black 38\").",
    "White": "The side with the white pieces: under a player's name, and beside \"Last move\" "
             "for who played it.",
    "Black": "The side with the black pieces: under a player's name, and beside \"Last move\" "
             "for who played it.",
    "White · to move": "Lichess TV, under a player's name: this player has the white pieces "
                       "and it is their turn.",
    "Black · to move": "Lichess TV, under a player's name: this player has the black pieces "
                       "and it is their turn.",
    "Captured": "Watch: a label over the pieces each side has taken. Small capitals.",
    "Nothing yet": "Watch: this side has not captured a piece yet.",
    "None yet": "Lichess TV, under \"Last move\": no move has been played yet.",
    "Watch full screen": "Watch: what the Cross button does, on a plate. About 24 letters.",
    "Since you tuned in": "Lichess TV: a label over the moves played while the player has been "
                          "watching. Small capitals.",
    "{0} move": "Lichess TV: how many moves were played while watching, when it is one.",
    "{0} moves": "Lichess TV: how many moves were played while watching.",
    "Moves appear here as they are played.": "Lichess TV: where the moves will be listed. One "
                                             "line, about 60 letters.",
    "No connection": "Lichess TV: a label over the text that says the TV cannot be reached. "
                     "Small capitals.",
    "Lichess TV is out of reach": "A headline: there is no internet connection. Lichess TV "
                                  "stays as it is. About 34 letters.",

    # ---- the interface kit's own words ----
    "Done": "On-screen keyboard: the key that ends typing. About 8 letters.",
    "Space": "On-screen keyboard: the key that types a space. About 8 letters.",
    "+{0} more": "Under a notification: {0} more of them are waiting (\"+2 more\").",
}
