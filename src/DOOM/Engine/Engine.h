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
// Today it holds the three subsystems already rewritten out of the global cloud:
// the random sequence, the WAD, the level geometry. It grows as each further
// subsystem is rewritten to be a member rather than a scatter of globals
// (REFACTOR.md, Step 5) - doomstat's 73 externs, r_state's 44, p_local's 27.
//
// Moving state in is necessary but not sufficient for constructing the engine rather
// than booting it. The blocker is not the state - it is that the vanilla names are
// *references* onto these members (extern T& viewx = engine().viewPoint.viewx), bound
// at static-init and never re-pointable, which pins this object to a fixed address.
// So a fresh world cannot come from making a new Engine elsewhere (every reference would
// dangle); it would need an in-place reset (placement-new), which is a half-measure not
// worth its risk while level reload already resets the world per scenario (Step 4). The
// engine becomes cheaply *constructible* only once the references are eliminated and every
// caller takes an Engine& - the terminal rewrite (REFACTOR.md, the Step-8 tail). Until
// then there is one instance (engine()), and the vanilla free functions randomness(),
// wad() and level() are views onto its members - so a caller that takes an Engine& and one that
// still reaches for the global see the same state.
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
} // namespace Doom
