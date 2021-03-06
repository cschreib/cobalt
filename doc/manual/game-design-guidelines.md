# Game design guidelines

These guidelines draft how to implement general concepts, and pitfalls to avoid. These should represent a core reference that drives every decision about the gameplay, for this specific game. These are not meant to be absolute truths valid for any game, as different games can appeal to different people. Whenever possible, examples of good and bad implementations observed in published games should be given, so as to make the point clear.

## Table of content

<!-- MarkdownTOC autolink=true bracket=round -->

- [Never force the user to do repetitive tasks.](#never-force-the-user-to-do-repetitive-tasks)
- [Never put the user into a fight against the random number generator.](#never-put-the-user-into-a-fight-against-the-random-number-generator)
- [Do not require the user to have a PhD in order to play correctly.](#do-not-require-the-user-to-have-a-phd-in-order-to-play-correctly)
- [Do not impose long gaming sessions to the user, and allow them to get away from their computer at any time.](#do-not-impose-long-gaming-sessions-to-the-user-and-allow-them-to-get-away-from-their-computer-at-any-time)
- [Provide all the needed information to the user in-place, and if a game manual is necessary it should always be available in-game.](#provide-all-the-needed-information-to-the-user-in-place-and-if-a-game-manual-is-necessary-it-should-always-be-available-in-game)
- [In a turn-based gameplay, allow maximal interaction with the game while the user is waiting for their turn.](#in-a-turn-based-gameplay-allow-maximal-interaction-with-the-game-while-the-user-is-waiting-for-their-turn)
- [What is forbidden for gameplay reasons must be clear \(and, ideally, justified\) and known to the user as soon as possible.](#what-is-forbidden-for-gameplay-reasons-must-be-clear-and-ideally-justified-and-known-to-the-user-as-soon-as-possible)
- [Do not give advantages to the AI that the user cannot have, and vice versa.](#do-not-give-advantages-to-the-ai-that-the-user-cannot-have-and-vice-versa)
- [Randomly generated content must not be fully random, but retain a large amount of cohesion](#randomly-generated-content-must-not-be-fully-random-but-retain-a-large-amount-of-cohesion)

<!-- /MarkdownTOC -->


## Never force the user to do repetitive tasks.

If the game requires some task to be performed, and that task is repetitive (i.e., the user is expected to perform this same task a number of times during a single gaming session), there should always be a way for the user to automate it. Either with some interface option, or better, with a gameplay element that takes care of it in his place. For example, although managing a single planet "by hand" can be an interesting activity in itself, it becomes a heavy task when the number of planets starts to grow. Bad examples can be seen in most real time strategy games, such as Starcraft, where you have to take care of every single resource harvester during the whole game. A good example is Sins of a Solar Empire, where resource harvesters are controlled automatically (what is important is to pick good resource spots, which is a one time decision).


## Never put the user into a fight against the random number generator.

There is no point in having the user try again and again the same action that depends on random parameters until the end result is favorable to them. Randomization is a good tool to generate diversity with little effort, but it should not impact the gameplay too much. For example, in a battle between two ships, there can be a part of randomness, but the winner of the fight should not depend entirely (or in a too direct way) on this randomness. The typical bad example is Dungeon & Dragon PC games like Baldur's Gate, where you could win most fights with a sufficient amount of saving and reloading. Another alternative would be to allow randomization to have important consequences, but to prevent the user from repeatedly try until they are satisfied with the result, so that they have no choice but to accept the outcome. Example of this are games where every action is permanently saved and there is no way to go back in time, such as most MMORPGs. This system could however generate frustration, and should be avoided when possible.


## Do not require the user to have a PhD in order to play correctly.

It should be possible to play the game without ever using a calculator or running a one month computer simulation. This means the user should have enough data to be able to make decisions without "calculating" anything (i.e., not having to do actual maths beyond mere strategical planning). It does not mean that any decision can be made without thinking, on the contrary. But this thinking must only involve simple cause-effect relationships (which, ideally, are taught to the user by the game using tooltips and other help mechanisms), intuition (to some extent), and experience. That being said, there can be elements of the gameplay that, to be used optimally, may require complex calculations (see for example factory yield calculations in X3 - The Threat). As long as this optimization is not essential to the gameplay, it is a good thing as it may create a whole different way to play the game for those of us who like playing with numbers.


## Do not impose long gaming sessions to the user, and allow them to get away from their computer at any time.

There are many games in which there are only fixed places and times where one can save the game to start again later (see, for example, the whole Final Fantasy series). This means the user cannot shut down their computer at any time they wish without loosing game progression. In these cases, a common workaround is to provide a "pause" function that halts the game until the user "un-pauses" it. The user can then leave the game paused for any period of time, and find it in the exact same state when they return. This is very close to a "saved game" mechanism, the only differences being that the computer must remain turned on during the whole process (which is not kind to Mother Earth), and that this "saved game" can only be used once (i.e., one cannot fail and reload indefinitely). To summarize, it should be possible to save the users progression at any time, so they can quit the game whenever they want to. Mechanisms can be implemented to avoid infinite reloading of a single saved state, if needed.


## Provide all the needed information to the user in-place, and if a game manual is necessary it should always be available in-game.

When a user needs more information on a particular game element, there are several ways to provide it. The best way is to display it within a "tooltip" that appears when the mouse hovers the region of the screen where this element is located: it is a standard and well known concept (a particularly good example is World of Warcraft), it is fast, and requires very little actions from the user. If there is too much information to display, or if it is not essential, it should be available in the game manual. This manual can exist independently from the game (like it used to be with all games before 2010), but it should also be easily accessible in-game at any time. Nowadays, most manuals are provided as PDF files and are never printed. Thus, if the user wants to access it, they must "Alt-Tab" out from the game, and run their favorite resource-hungry PDF viewer in the background. This is not practical, and can prevent the user from obtaining the information they seek, generating frustration (even more when the game crashes because of the Alt-Tab, which is not an uncommon phenomenon). Having an in-game manual (also called "encyclopedia" in some games) is an uncommon practice (see for example the Civilization series), perhaps because of the programming effort it requires, but it is a good practice.


## In a turn-based gameplay, allow maximal interaction with the game while the user is waiting for their turn.

In too many turn-based games, the user cannot do anything but watch while the turn of each other player is computed/played. If this is not a problem at the beginning of the game where turns are quite shorts, it becomes a real pain after a few hours of playing, where one can wait up to several minutes. It is alright to forbid users from making certain actions while their turn has not began, but any action that does not modify the game situation should be allowed. In particular, selecting units, viewing planets or ships status, browsing the in-game manual, etc. All of these actions require that the user interface remain responsive, i.e., that sufficient CPU power is left to the user during the turn computation (in Civilization V for example, such interaction is possible, but the turn computation is so intensive that most hardware configurations make it way too unresponsive to be used enjoyably). In the event that the action of another player requires the attention of the user (for example a combat between two ships) a notification should be sent to the user, asking they if they want to witness the action or not (possibly allowing they to tick a "remember my answer" box). In a multiplayer context, with other human players, a timer can also be introduced, forcing the user to respond within several seconds (so that other players do not wait forever).


## What is forbidden for gameplay reasons must be clear (and, ideally, justified) and known to the user as soon as possible.

In an ideal world, it would be possible to create a game in which there are no restriction on what the user can do. In the real world however, there has to be rules, things that the user cannot do because the program is not built for it. Such rules can also generate fun, if used wisely, but to maximize enjoyment the rules must be clear, as simple as possible (so they are easy to remember) and always recalled when relevant. One way to introduce them to the user is through a short tutorial which presents typical situations and the available options. The situation one wants to avoid is to let the user play and make decisions then have them discover, at a crucial moment, that some rule prevents them from doing something, so that their only option is to loose badly. Furthermore, it may be a good thing to try to justify the game's limitation and rules so that the user understands it is something that has been carefully chosen, and not only dictated by laziness or overdue shipping deadlines. For example, one can justify the quasi-absence of civilian interstellar traffic in the game by the high price of the interstellar travel technology (and such non-essential information can be written in the game manual). Bad examples include numerous games where the player bumps into "invisible walls" to prevent them from reaching certain areas that are either not available yet (but will be unlocked later) or simply not implemented in the game (like house doors in the cities of most RPGs).


## Do not give advantages to the AI that the user cannot have, and vice versa.

It is always frustrating to play a game in which all players do not start as equals. Typical examples include games in which the AI always knows where the other players are located, even when they are supposed to be hidden by "fog-of-war" (see Starcraft for example). Rules should be the same for everyone, so that the winner is the most cunning, and not the most fortunate one. There are many games in which difficulty levels are implemented by giving bonuses to the player's opponents (for example in the Civilization series), rather than increasing how smart the opponents are. This is acceptable if chosen and known by the user (so as to create an extra challenge), but there are other ways to increase difficulty, either by algorithmic changes (for hard-coded AIs) or different/more extensive training (for machine-learned AIs).


## Randomly generated content must not be fully random, but retain a large amount of cohesion


