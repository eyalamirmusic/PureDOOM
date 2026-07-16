#include "Engine.h"

namespace Doom
{
// A function-local static, so it is constructed on the first call whoever makes
// it, regardless of translation-unit init order. That matters: m_random.cpp binds
// `int& rndindex = randomness().menuIndex` at static-init time, which reaches
// through here before main() runs.
Engine& engine()
{
    static auto instance = Engine {};
    return instance;
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
} // namespace Doom
