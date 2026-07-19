#include "Engine.h"

#include <ea_data_structures/Pointers/OwningPointer.h>

namespace Doom
{
namespace
{
// The one Engine, heap-owned so resetEngine() can drop it and make a fresh one.
// Still a function-local static, so the *pointer itself* is constructed on first
// touch regardless of translation-unit init order - the same guarantee the plain
// `static auto instance = Engine{}` this replaced gave m_random.cpp's static-init
// reference (since retired; REFACTOR.md, Step 9 strand (a)), kept here for
// whatever future static-init-time caller wants it.
EA::OwningPointer<Engine>& enginePointer()
{
    static auto instance = EA::OwningPointer<Engine> {};
    return instance;
}
} // namespace

Engine& engine()
{
    return enginePointer().getOrCreate();
}

// Test/embedder facility - see the comment on Engine (Engine.h) for what this is
// for. new Engine{} runs before the old one is deleted, so the two briefly coexist;
// that costs one extra Engine's worth of memory for the duration of one call and
// nothing else, this being neither hot nor reentrant (the engine is
// single-threaded).
void resetEngine()
{
    enginePointer().create();
}

// The vanilla free functions, now views onto the one Engine's members rather than
// singletons of their own. This is the composition root: the three subsystems are
// owned in one place, and every caller - rewritten or not - reaches the same one.
Random& randomness()
{
    return engine().random;
}

WadFile& wad()
{
    return engine().wad;
}

Level& level()
{
    return engine().level;
}

Clip& clip()
{
    return engine().clip;
}

ActionScratch& actionScratch()
{
    return engine().actionScratch;
}

SightScratch& sightScratch()
{
    return engine().sightScratch;
}

WeaponScratch& weaponScratch()
{
    return engine().weaponScratch;
}

EnemyAI& enemyAI()
{
    return engine().enemyAI;
}

SwitchList& switchList()
{
    return engine().switchList;
}

PlayerScratch& playerScratch()
{
    return engine().playerScratch;
}

AnimatedSurfaces& animatedSurfaces()
{
    return engine().animatedSurfaces;
}

LevelPool& levelPool()
{
    return engine().levelPool;
}

ThinkerList& thinkerList()
{
    return engine().thinkerList;
}

EngineParams& engineParams()
{
    return engine().engineParams;
}

ViewPoint& viewPoint()
{
    return engine().viewPoint;
}

ViewProjection& viewProjection()
{
    return engine().viewProjection;
}

ViewWindow& viewWindow()
{
    return engine().viewWindow;
}

Lighting& lighting()
{
    return engine().lighting;
}

GraphicsData& graphicsData()
{
    return engine().graphicsData;
}

CompositeCache& compositeCache()
{
    return engine().compositeCache;
}

WallScratch& wallScratch()
{
    return engine().wallScratch;
}

SpriteScratch& spriteScratch()
{
    return engine().spriteScratch;
}

DrawTables& drawTables()
{
    return engine().drawTables;
}

SolidSegs& solidSegs()
{
    return engine().solidSegs;
}

PlaneScratch& planeScratch()
{
    return engine().planeScratch;
}

RenderMainState& renderMainState()
{
    return engine().renderMainState;
}

RenderScratch& renderScratch()
{
    return engine().renderScratch;
}

LevelStats& levelStats()
{
    return engine().levelStats;
}

LaunchOptions& launchOptions()
{
    return engine().launchOptions;
}

GameVersion& gameVersion()
{
    return engine().gameVersion;
}

GameSession& gameSession()
{
    return engine().gameSession;
}

StartupDefaults& startupDefaults()
{
    return engine().startupDefaults;
}

PlayerState& playerState()
{
    return engine().playerState;
}

GameFlow& gameFlow()
{
    return engine().gameFlow;
}

DemoState& demoState()
{
    return engine().demoState;
}

RefreshFlags& refreshFlags()
{
    return engine().refreshFlags;
}

SaveGameState& saveGameState()
{
    return engine().saveGameState;
}

OverlayState& overlayState()
{
    return engine().overlayState;
}

NetState& netState()
{
    return engine().netState;
}

MapSpawns& mapSpawns()
{
    return engine().mapSpawns;
}

GameClock& gameClock()
{
    return engine().gameClock;
}

AmmoLimits& ammoLimits()
{
    return engine().ammoLimits;
}

IntermissionInfo& intermissionInfo()
{
    return engine().intermissionInfo;
}

SkyState& skyState()
{
    return engine().skyState;
}

ItemRespawnQueue& itemRespawnQueue()
{
    return engine().itemRespawnQueue;
}

CorpseQueue& corpseQueue()
{
    return engine().corpseQueue;
}

EventQueue& eventQueue()
{
    return engine().eventQueue;
}

ActiveSpecials& activeSpecials()
{
    return engine().activeSpecials;
}

EndLevelTimer& endLevelTimer()
{
    return engine().endLevelTimer;
}

AttractMode& attractMode()
{
    return engine().attractMode;
}

ValidCount& validCount()
{
    return engine().validCount;
}

TiccmdInput& ticcmdInput()
{
    return engine().ticcmdInput;
}

DeferredNewGame& deferredNewGame()
{
    return engine().deferredNewGame;
}

ParTimes& parTimes()
{
    return engine().parTimes;
}

MovementSpeeds& movementSpeeds()
{
    return engine().movementSpeeds;
}

TimeDemo& timeDemo()
{
    return engine().timeDemo;
}

PendingCommands& pendingCommands()
{
    return engine().pendingCommands;
}

HudMessage& hudMessage()
{
    return engine().hudMessage;
}

HudChat& hudChat()
{
    return engine().hudChat;
}

HudState& hudState()
{
    return engine().hudState;
}

IntermissionState& intermissionState()
{
    return engine().intermissionState;
}

FinaleState& finaleState()
{
    return engine().finaleState;
}

MenuState& menuState()
{
    return engine().menuState;
}

AutomapView& automapView()
{
    return engine().automapView;
}

StatusBarFace& statusBarFace()
{
    return engine().statusBarFace;
}

StatusBarGraphics& statusBarGraphics()
{
    return engine().statusBarGraphics;
}

StatusBarState& statusBarState()
{
    return engine().statusBarState;
}

StatusBarWidgets& statusBarWidgets()
{
    return engine().statusBarWidgets;
}

WipeState& wipeState()
{
    return engine().wipeState;
}

SoundSettings& soundSettings()
{
    return engine().soundSettings;
}

SoundState& soundState()
{
    return engine().soundState;
}

MenuSettings& menuSettings()
{
    return engine().menuSettings;
}

ConfigPaths& configPaths()
{
    return engine().configPaths;
}

BSPScratch& bspScratch()
{
    return engine().bspScratch;
}

SegState& segState()
{
    return engine().segState;
}

SpriteState& spriteState()
{
    return engine().spriteState;
}

DrawState& drawState()
{
    return engine().drawState;
}

VideoState& videoState()
{
    return engine().videoState;
}

HudFlags& hudFlags()
{
    return engine().hudFlags;
}

InputConfig& inputConfig()
{
    return engine().inputConfig;
}

SoundTarget& soundTarget()
{
    return engine().soundTarget;
}

HudFont& hudFont()
{
    return engine().hudFont;
}

StatusWidgetGraphics& statusWidgetGraphics()
{
    return engine().statusWidgetGraphics;
}

DisplayState& displayState()
{
    return engine().displayState;
}
} // namespace Doom
