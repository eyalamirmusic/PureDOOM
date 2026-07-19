#pragma once

#include "../Game/AmmoLimits.h"
#include "../Game/AttractMode.h"
#include "../Game/ConfigPaths.h"
#include "../Game/CorpseQueue.h"
#include "../Game/DeferredNewGame.h"
#include "../Game/DemoState.h"
#include "../Game/DisplayState.h"
#include "../Game/EngineParams.h"
#include "../Game/EventQueue.h"
#include "../Game/GameClock.h"
#include "../Game/GameFlow.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/InputConfig.h"
#include "../Game/IntermissionInfo.h"
#include "../Game/LaunchOptions.h"
#include "../Game/LevelStats.h"
#include "../Game/MapSpawns.h"
#include "../Game/MovementSpeeds.h"
#include "../Game/NetState.h"
#include "../Game/OverlayState.h"
#include "../Game/ParTimes.h"
#include "../Game/PendingCommands.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "../Game/SaveGameState.h"
#include "../Game/SkyState.h"
#include "../Game/SoundSettings.h"
#include "../Game/SoundState.h"
#include "../Game/StartupDefaults.h"
#include "../Game/TiccmdInput.h"
#include "../Game/TimeDemo.h"
#include "../Render/BSPScratch.h"
#include "../Render/CompositeCache.h"
#include "../Render/DrawState.h"
#include "../Render/DrawTables.h"
#include "../Render/GraphicsData.h"
#include "../Render/Lighting.h"
#include "../Render/PlaneScratch.h"
#include "../Render/RenderMainState.h"
#include "../Render/RenderScratch.h"
#include "../Render/SegState.h"
#include "../Render/SolidSegs.h"
#include "../Render/SpriteScratch.h"
#include "../Render/SpriteState.h"
#include "../Render/ViewPoint.h"
#include "../Render/ViewProjection.h"
#include "../Render/ViewWindow.h"
#include "../Render/VideoState.h"
#include "../Render/WallScratch.h"
#include "../Sim/ActionScratch.h"
#include "../Sim/ActiveSpecials.h"
#include "../Sim/AnimatedSurfaces.h"
#include "../Sim/Clip.h"
#include "../Sim/EnemyAI.h"
#include "../Sim/EndLevelTimer.h"
#include "../Sim/ItemRespawnQueue.h"
#include "../Sim/Level.h"
#include "../Sim/LevelPool.h"
#include "../Sim/PlayerScratch.h"
#include "../Sim/Random.h"
#include "../Sim/SightScratch.h"
#include "../Sim/SoundTarget.h"
#include "../Sim/SwitchList.h"
#include "../Sim/ThinkerList.h"
#include "../Sim/ValidCount.h"
#include "../Sim/WeaponScratch.h"
#include "../UI/AutomapView.h"
#include "../UI/FinaleState.h"
#include "../UI/HudChat.h"
#include "../UI/HudFlags.h"
#include "../UI/HudFont.h"
#include "../UI/HudMessage.h"
#include "../UI/HudState.h"
#include "../UI/IntermissionState.h"
#include "../UI/MenuSettings.h"
#include "../UI/MenuState.h"
#include "../UI/StatusBarFace.h"
#include "../UI/StatusBarGraphics.h"
#include "../UI/StatusBarState.h"
#include "../UI/StatusBarWidgets.h"
#include "../UI/StatusWidgetGraphics.h"
#include "../UI/WipeState.h"
#include "../Wad/WadFile.h"

namespace Doom
{
// The engine's mutable state, gathered into one owner.
//
// Every subsystem this refactor has rewritten lives here: the random sequence, the
// WAD, the level geometry, and the render/UI/game/sim clusters that doomstat.h's 73
// externs, r_state.h's 44 and p_local.h's 27 emptied into as each was rewritten
// (REFACTOR.md, Step 5). Every reader reaches its cluster through an owner or an
// accessor (`viewPoint().viewx`, not a bare `viewx`) - the reference-alias layer
// that used to bind ~290 vanilla names onto these members at static-init time,
// pinning this object to a fixed address, is retired (Step 9 strand (a)). Nothing
// outside Engine.cpp holds this object's address, which is what lets engine() hand
// out a different one.
//
// It does exactly that: engine() is a heap-owned instance
// (EA::OwningPointer<Engine>, Engine.cpp), and resetEngine() drops it and makes a
// fresh one in its place. A test can therefore own a genuinely clean world
// mid-process - dirty some state, reset, and find the new instance has none of it -
// rather than relying on the per-level reset Doom::Level already does, which resets
// the *level*, not every cluster (Tests/Sim/EngineTests.cpp proves the difference).
//
// resetEngine() is a test/embedder facility, not part of normal engine operation:
// nothing in the engine calls it, and it must not be wired into doom_init or a
// level load - the per-level reset already works and is what the demos rely on. A
// reset also repopulates nothing: the WAD- and level-derived pointer views
// (vertexes/textures/screens[]/...) are left stale until something loads a WAD and
// a level into the new instance, exactly as they are before the first doom_init.
struct Engine
{
    Random random;
    WadFile wad;
    Level level;
    Clip clip;
    ActionScratch actionScratch;
    SightScratch sightScratch;
    WeaponScratch weaponScratch;
    EnemyAI enemyAI;
    SwitchList switchList;
    PlayerScratch playerScratch;
    AnimatedSurfaces animatedSurfaces;
    LevelPool levelPool;
    ThinkerList thinkerList;
    EngineParams engineParams;
    ViewPoint viewPoint;
    ViewProjection viewProjection;
    ViewWindow viewWindow;
    Lighting lighting;
    GraphicsData graphicsData;
    CompositeCache compositeCache;
    RenderScratch renderScratch;
    WallScratch wallScratch;
    SpriteScratch spriteScratch;
    DrawTables drawTables;
    SolidSegs solidSegs;
    PlaneScratch planeScratch;
    RenderMainState renderMainState;
    LevelStats levelStats;
    LaunchOptions launchOptions;
    GameVersion gameVersion;
    GameSession gameSession;
    StartupDefaults startupDefaults;
    PlayerState playerState;
    GameFlow gameFlow;
    DemoState demoState;
    RefreshFlags refreshFlags;
    SaveGameState saveGameState;
    OverlayState overlayState;
    NetState netState;
    MapSpawns mapSpawns;
    GameClock gameClock;
    AmmoLimits ammoLimits;
    IntermissionInfo intermissionInfo;
    SkyState skyState;
    ItemRespawnQueue itemRespawnQueue;
    CorpseQueue corpseQueue;
    EventQueue eventQueue;
    ActiveSpecials activeSpecials;
    EndLevelTimer endLevelTimer;
    AttractMode attractMode;
    ValidCount validCount;
    TiccmdInput ticcmdInput;
    DeferredNewGame deferredNewGame;
    ParTimes parTimes;
    MovementSpeeds movementSpeeds;
    TimeDemo timeDemo;
    PendingCommands pendingCommands;
    HudMessage hudMessage;
    HudChat hudChat;
    HudState hudState;
    IntermissionState intermissionState;
    FinaleState finaleState;
    MenuState menuState;
    AutomapView automapView;
    StatusBarFace statusBarFace;
    StatusBarGraphics statusBarGraphics;
    StatusBarState statusBarState;
    StatusBarWidgets statusBarWidgets;
    WipeState wipeState;
    SoundSettings soundSettings;
    SoundState soundState;
    MenuSettings menuSettings;
    ConfigPaths configPaths;
    BSPScratch bspScratch;
    SegState segState;
    SpriteState spriteState;
    DrawState drawState;
    VideoState videoState;
    HudFlags hudFlags;
    InputConfig inputConfig;
    SoundTarget soundTarget;
    HudFont hudFont;
    StatusWidgetGraphics statusWidgetGraphics;
    DisplayState displayState;
};

// The one instance, for as long as the vanilla globals still reach state by free
// function rather than through an Engine& they were handed.
Engine& engine();

// Drops the live Engine and makes a fresh one in its place - see the comment on
// Engine above for what this is for and what it deliberately leaves stale.
void resetEngine();
} // namespace Doom
