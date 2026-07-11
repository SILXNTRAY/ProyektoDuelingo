#pragma once
#include <array>
#include <memory>

// #include "GameMode/Impl/Regular.hpp"
#include "GameMode/Impl/1v1.hpp"
#include "GameMode/Impl/3v3.hpp"
#include "GameMode/Impl/4v4.hpp"

#include "GameMode/Impl/HardCore.hpp"

#include "GameMode/Impl/Boss.hpp"
#include "GameMode/Impl/Clone.hpp"

#include "GameMode/Impl/Deathmatch.hpp"
#include "GameMode/Impl/RandomDeathmatch.hpp"

extern GameMode s_GameMode;
extern std::array<std::unique_ptr<IGameModeHandler>, GameMode::__Internal_Max_Length> s_ModeHandlers;

inline GameMode getGameMode()
{
	return s_GameMode;
}

// Boss (1v1 Duel Arena) and Deathmatch (endless arcade) both play out as
// open-ended, non-standard-length runs rather than a normal timed team
// match, so per-match checks like "fastest win" bonuses or per-hero
// "best time" records don't mean the same thing there. Checks gating on
// "is this one of those modes" should use this rather than hardcoding
// GameMode::Boss on its own -- that was the bug that left Deathmatch out
// of a couple of these checks in the first place.
inline bool isDuelMode()
{
	return s_GameMode == GameMode::Boss || s_GameMode == GameMode::Deathmatch;
}

inline IGameModeHandler *getGameModeHandler()
{
	return s_ModeHandlers[static_cast<size_t>(s_GameMode)].get();
}
