// The snapshot interface between the DOOM engine's internals and the eacp
// renderer: an ordinary translation unit that includes the engine's headers.
// Nothing DOOM-typed leaks out through EngineAccess.h.

#include "EngineAccess.h"
#include <DOOM/Sim/Level.h>

#include <DOOM/Game/OverlayState.h>
#include <DOOM/Game/SkyState.h>
#include <DOOM/Render/BSP.h>
#include <DOOM/Render/GraphicsData.h>
#include <DOOM/Render/Lighting.h>
#include <DOOM/Render/Planes.h>
#include <DOOM/Render/ViewWindow.h>
#include <DOOM/Sim/ThinkerList.h>
#include <DOOM/Render/Things.h>
#include <DOOM/Render/Video.h>
#include <DOOM/UI/Automap.h>
#include <DOOM/UI/Hud.h>
#include <DOOM/DOOM.h>
#include <DOOM/Host/Platform.h>

#include <DOOM/UI/AutomapTypes.h>
#include <DOOM/Game/PlayerTypes.h>
#include <DOOM/Game/GameDefs.h>
#include <DOOM/Game/MapSpawns.h>
#include <DOOM/doomtype.h>
#include <DOOM/UI/Wipe.h>
#include <DOOM/UI/Hud.h>
#include <DOOM/Sim/Info.h>
#include <DOOM/Math/FixedPoint.h>
#include <DOOM/UI/Menu.h>
#include <DOOM/Game/ConfigTypes.h>
#include <DOOM/Sim/SimDefs.h>
#include <DOOM/Sim/WeaponTypes.h>
#include <DOOM/Sim/MapTypes.h>
#include <DOOM/Render/RenderTypes.h>
#include <DOOM/UI/StatusBarTypes.h>
#include <DOOM/Math/TrigTables.h>
#include <DOOM/Game/GameClock.h>
#include <DOOM/Game/GameFlow.h>
#include <DOOM/Game/GameSession.h>
#include <DOOM/Game/OverlayState.h>
#include <DOOM/Game/PlayerState.h>
#include <DOOM/Game/RefreshFlags.h>
#include <DOOM/UI/MenuSettings.h>
#include <DOOM/Wad/WadFile.h>

#include <cstring>

// Engine globals that no header declares. DOOM.c reaches them the same way, so
#include <DOOM/UI/Menu.h>
// this is the house style rather than a workaround - but they are the natural
#include <DOOM/Render/Main.h>
// candidates for a real interface as the engine gets refactored.
extern unsigned char screen_palette[256 * 3];

// Convex subsector cells rarely exceed a handful of corners; the cap only
// bounds the clipper's scratch space.
#define EACP_MAX_POLY_VERTICES 32

// Larger than any DOOM map, so the initial square the BSP clips down is
// effectively unbounded.
#define EACP_MAP_LIMIT 32768.0

#define EACP_CLIP_EPSILON 1e-6

#define EACP_FLAT_SIZE 64

#define EACP_SCREEN_PIXELS (EACP_DOOM_SCREEN_WIDTH * EACP_DOOM_SCREEN_HEIGHT)

// The sky is a cylinder around the camera, far enough out that every wall in a
// DOOM level stands in front of it, and tall enough to fill the view.
#define EACP_SKY_SEGMENTS 64
#define EACP_SKY_RADIUS 12000.0f
#define EACP_SKY_HEIGHT 9000.0f

// The view's focal length in rows: half of the 168-row view divided by the
// tangent of half its vertical field of view. It converts a height on the sky
// cylinder into the screen row DOOM would have drawn it at.
#define EACP_SKY_FOCAL 133.33f

typedef struct
{
    double x, y;
} EacpPoint;

typedef struct
{
    // Null while counting: the first pass sizes each texture's run, the second
    // writes into it.
    EacpDoomVertex* vertices;
    int* counts;
    int* cursors;
} EacpEmitter;

static float* eacpPolyVertices = 0;
static int* eacpPolyStart = 0;
static int* eacpPolyCount = 0;
static int eacpPolyTotal = 0;

// A line's length never changes, and the geometry pass needs it for every wall
// it lays a texture along, so it is measured once when the level loads rather
// than thousands of times a frame.
static float* eacpLineLengths = 0;

static int* eacpTextureCounts = 0;
static int* eacpTextureCursors = 0;
static int eacpScratchTextures = 0;

static void* eacpCachedNodes = 0;
static int eacpCachedSubsectors = -1;
static int eacpCachedEpisode = -1;
static int eacpCachedMap = -1;

static int eacpTexturesReady = 0;
static unsigned char* eacpWallMasked = 0;
static short* eacpSpriteHeights = 0;

// Where every sector's floor and ceiling stood at the last tic, so a door or a
// lift can be drawn part-way to where it is going rather than jumping there.
static float* eacpPreviousFloor = 0;
static float* eacpPreviousCeiling = 0;
static int eacpSnapshotSectors = 0;

// How far into the current tic the frame being built sits.
static float eacpAlpha = 0.0f;

static double eacpFixedToDouble(fixed_t value)
{
    return (double) value.raw / (double) FRACUNIT.raw;
}

static float eacpFixedToFloat(fixed_t value)
{
    return (float) value.raw / (float) FRACUNIT.raw;
}

// The engine deliberately links no libm, so the one square root the texture
// mapping needs (a wall's length, for its horizontal texture coordinate)
// comes from Newton's method rather than an include.
static double eacpSqrt(double value)
{
    double guess;
    int i;

    if (value <= 0.0)
        return 0.0;

    guess = value;

    for (i = 0; i < 24; ++i)
        guess = 0.5 * (guess + value / guess);

    return guess;
}

static float eacpMix(float from, float to, float amount)
{
    return from + (to - from) * amount;
}

// A sector's floor and ceiling where they are *now*, part-way through whatever
// move a door or a lift has them making, rather than where the last tic left
// them. The walls that meet them are drawn from the same numbers, so nothing
// tears.
static float eacpFloorHeight(Doom::Sector* sector)
{
    float now = eacpFixedToFloat(sector->floorheight);
    int index = (int) (sector - sectors);

    if (eacpPreviousFloor == 0 || index < 0 || index >= eacpSnapshotSectors)
        return now;

    return eacpMix(eacpPreviousFloor[index], now, eacpAlpha);
}

static float eacpCeilingHeight(Doom::Sector* sector)
{
    float now = eacpFixedToFloat(sector->ceilingheight);
    int index = (int) (sector - sectors);

    if (eacpPreviousCeiling == 0 || index < 0 || index >= eacpSnapshotSectors)
        return now;

    return eacpMix(eacpPreviousCeiling[index], now, eacpAlpha);
}

void eacpDoomSnapshotTic()
{
    int i;

    if (Doom::gameFlow().gamestate != Doom::GS_LEVEL || sectors == 0
        || numsectors <= 0)
        return;

    if (eacpSnapshotSectors != numsectors)
    {
        doom_free(eacpPreviousFloor);
        doom_free(eacpPreviousCeiling);

        eacpPreviousFloor = (float*) doom_malloc(numsectors * (int) sizeof(float));
        eacpPreviousCeiling = (float*) doom_malloc(numsectors * (int) sizeof(float));
        eacpSnapshotSectors = numsectors;

        if (eacpPreviousFloor == 0 || eacpPreviousCeiling == 0)
        {
            eacpSnapshotSectors = 0;
            return;
        }
    }

    for (i = 0; i < numsectors; ++i)
    {
        eacpPreviousFloor[i] = eacpFixedToFloat(sectors[i].floorheight);
        eacpPreviousCeiling[i] = eacpFixedToFloat(sectors[i].ceilingheight);
    }
}

int eacpDoomViewActive()
{
    return Doom::gameFlow().gamestate == Doom::GS_LEVEL && Doom::gameClock().gametic;
}

int eacpDoomBuildWipe(unsigned char* outStart, unsigned char* outOffsets)
{
    const unsigned char* start = (const unsigned char*) wipe_scr_start;
    int column;
    int row;

    if (!Doom::gameFlow().is_wiping_screen || start == 0)
        return 0;

    // wipe_melt_running is the melt's own "I have been set up" flag, and the only
    // to test: wipe_exitMelt frees the column table but leaves y pointing at it,
    // so between melts y is non-null and dangling. Until the melt is set up, the
    // outgoing screen is still row-major and nothing has slid.
    if (!wipe_melt_running)
    {
        doom_memcpy(outStart, start, EACP_SCREEN_PIXELS);
        doom_memset(outOffsets, 0, EACP_DOOM_WIPE_COLUMNS);

        return 1;
    }

    for (column = 0; column < EACP_DOOM_WIPE_COLUMNS; ++column)
    {
        int slid = wipe_melt_offsets[column];

        // A column that has not started moving sits at a negative offset; it has
        // slid nothing, which is what zero says.
        if (slid < 0)
            slid = 0;
        else if (slid > EACP_DOOM_SCREEN_HEIGHT)
            slid = EACP_DOOM_SCREEN_HEIGHT;

        outOffsets[column] = (unsigned char) slid;

        // wipe_initMelt leaves the outgoing screen column-major - a column of
        // two-pixel shorts - because that is how the melt walks it. A texture
        // wants it back the way round it was. Copying the two bytes rather than
        // the short keeps this independent of byte order.
        for (row = 0; row < EACP_DOOM_SCREEN_HEIGHT; ++row)
        {
            const unsigned char* source =
                start + (column * EACP_DOOM_SCREEN_HEIGHT + row) * 2;
            unsigned char* destination =
                outStart + row * EACP_DOOM_SCREEN_WIDTH + column * 2;

            destination[0] = source[0];
            destination[1] = source[1];
        }
    }

    return 1;
}

int eacpDoomAutomapActive()
{
    return Doom::overlayState().automapactive ? 1 : 0;
}

int eacpDoomStatusBarVisible()
{
    // Doom::drawStatusBar's own st_statusbaron, which is private to it: Doom::displayFrame asks for
    // a bar-less frame once the view fills all 200 rows, and the automap keeps
    // the bar whatever the screen size says.
    return Doom::viewWindow().viewheight != Doom::SCREENHEIGHT
           || Doom::overlayState().automapactive;
}

float eacpDoomViewRows()
{
    return eacpDoomStatusBarVisible() ? (float) Doom::ST_Y
                                      : (float) Doom::SCREENHEIGHT;
}

void eacpDoomRevealAutomap()
{
    auto& players_ = Doom::playerState();

    Doom::Player* player = &players_.players[players_.displayplayer];

    if (Doom::gameFlow().gamestate != Doom::GS_LEVEL || !Doom::gameClock().gametic
        || !Doom::overlayState().automapactive || player->mo == 0)
        return;

    Doom::setupFrame(*player);
    Doom::clearClipSegs();
    Doom::clearDrawSegs();
    Doom::clearPlanes();
    Doom::clearSprites();

    // Marking a line is Doom::storeWallRange's doing, and it does it as it draws the
    // wall - so the walls land in the frame the automap has just drawn itself
    // into (the column drawers write through ylookup, which was pointed at
    // screens[0] when the view size was set, and does not follow it anywhere).
    // Drawing the map again puts it back. The GPU path does not read that frame
    // for anything but the status bar, which the view never reaches; the
    // software one reads all of it.
    Doom::renderBSPNode(numnodes - 1);
    Doom::drawAutomap();
}

int eacpDoomDarkenRow()
{
    // Doom::drawMenu only reaches its darkening once it is actually showing a menu: a
    // confirmation prompt draws its text and returns before then.
    if (Doom::overlayState().menuactive && !messageToPrint
        && (doom_flags & Doom::DOOM_FLAG_MENU_DARKEN_BG))
        return EACP_DOOM_MENU_DARKEN_ROW;

    return 0;
}

static unsigned char eacpOverlayPass[2][EACP_SCREEN_PIXELS];
static unsigned char eacpUnderIndex[EACP_SCREEN_PIXELS];
static unsigned char eacpUnderMask[EACP_SCREEN_PIXELS];
static unsigned char eacpMenuIndex[EACP_SCREEN_PIXELS];
static unsigned char eacpMenuMask[EACP_SCREEN_PIXELS];

// What Doom::displayFrame draws over the view *before* the menu darkens the frame, in its
// order - and so what the menu darkens along with the world.
static void eacpDrawUnderLayers()
{
    auto& overlay = Doom::overlayState();
    auto& view = Doom::viewWindow();

    if (Doom::gameFlow().gamestate != Doom::GS_LEVEL || !Doom::gameClock().gametic)
        return;

    if (overlay.automapactive)
        Doom::drawAutomapMarks();

    Doom::drawHud();

    if (Doom::refreshFlags().paused)
    {
        int y = overlay.automapactive ? 4 : view.viewwindowy + 4;

        Doom::drawPatchDirect(view.viewwindowx + (view.scaledviewwidth - 68) / 2,
                              y,
                              0,
                              (Doom::Patch*) Doom::cacheLumpName("M_PAUSE"));
    }
}

// One layer's pixels and the coverage that goes with them. Primed differently,
// the two passes agree only where the layer drew, so where they agree is exactly
// what it covered - which no single pass can tell, a drawn pixel being free to
// hold whatever value the buffer was primed with.
static void eacpCaptureLayer(void (*layer)(),
                             unsigned char* indices,
                             unsigned char* coverage)
{
    byte* frame = screens[0];
    int pass;
    int i;

    for (pass = 0; pass < 2; ++pass)
    {
        doom_memset(
            eacpOverlayPass[pass], pass == 0 ? 0x00 : 0xff, EACP_SCREEN_PIXELS);

        screens[0] = eacpOverlayPass[pass];
        layer();
    }

    screens[0] = frame;

    for (i = 0; i < EACP_SCREEN_PIXELS; ++i)
    {
        indices[i] = eacpOverlayPass[0][i];
        coverage[i] = eacpOverlayPass[0][i] == eacpOverlayPass[1][i] ? 255 : 0;
    }
}

int eacpDoomBuildOverlay(unsigned char* outRgba)
{
    int flags = doom_flags;
    int covered = 0;
    int i;

    // Left to the GPU view, which can darken at full resolution and exactly (see
    // eacpDoomDarkenRow). Were it left on, it would write to all 64000 pixels and
    // the whole screen would come back as covered.
    doom_flags &= ~Doom::DOOM_FLAG_MENU_DARKEN_BG;

    eacpCaptureLayer(eacpDrawUnderLayers, eacpUnderIndex, eacpUnderMask);
    eacpCaptureLayer(Doom::drawMenu, eacpMenuIndex, eacpMenuMask);

    doom_flags = flags;

    for (i = 0; i < EACP_SCREEN_PIXELS; ++i)
    {
        int menu = eacpMenuMask[i] != 0;
        int under = eacpUnderMask[i] != 0;

        outRgba[i * 4 + 0] = menu ? eacpMenuIndex[i] : eacpUnderIndex[i];

        // The menu darkens what was already on the screen and then draws itself
        // over it, so a message or the PAUSE graphic dims with the world it sits
        // on while the menu itself stays bright.
        outRgba[i * 4 + 1] = (under && !menu) ? 255 : 0;
        outRgba[i * 4 + 2] = 0;
        outRgba[i * 4 + 3] = (menu || under) ? 255 : 0;

        covered |= menu || under;
    }

    return covered;
}

void eacpDoomBindKeys()
{
    int count = numdefaults;
    int i;

    for (i = 0; i < count; i++)
    {
        Doom::ConfigDefault* entry = &defaults[i];

        if (entry->defaultvalue != Doom::STRING_VALUE
            && std::strncmp(entry->name, "key_", 4) == 0)
        {
            *entry->location = entry->defaultvalue;
        }
    }
}

double eacpDoomTicTime()
{
    int sec, usec;

    doom_gettime(&sec, &usec);

    // Doom::currentTic's own expression, kept fractional instead of truncated, so this
    // steps from one tic to the next at exactly the moment the engine does. It
    // omits the engine's private start-of-run offset, which only shifts the
    // count by a whole number of tics and so changes neither the steps nor the
    // fraction.
    return (double) sec * Doom::TICRATE + (double) usec * Doom::TICRATE / 1000000.0;
}

int eacpDoomIsWiping()
{
    return Doom::gameFlow().is_wiping_screen ? 1 : 0;
}

int eacpDoomMouseSensitivity()
{
    return Doom::menuSettings().mouseSensitivity;
}

EacpDoomCamera eacpDoomGetCamera()
{
    auto& players_ = Doom::playerState();

    EacpDoomCamera camera = {0, 0, 0, 0};
    Doom::Player* player = &players_.players[players_.displayplayer];

    if (player->mo == 0)
        return camera;

    camera.x = eacpFixedToFloat(player->mo->x);
    camera.y = eacpFixedToFloat(player->mo->y);
    camera.z = eacpFixedToFloat(player->viewz);

    // angle_t maps the full circle onto 32 bits; 2^31 is half a turn.
    camera.angle =
        (float) ((double) player->mo->angle.raw * (3.14159265358979 / 2147483648.0));
    return camera;
}

static int eacpSpriteBase()
{
    auto& gfx = Doom::graphicsData();

    return gfx.numtextures + gfx.numflats;
}

// Draws a patch's posts into an index image and its coverage into an alpha
// image. This is how DOOM stores every graphic: runs of pixels down a column,
// with the gaps between the runs left transparent. Composing from the patches
// (as Doom::generateComposite does) rather than reading the engine's cached columns
// is what makes a masked texture's holes come out as holes.
static void eacpBlitPatch(Doom::Patch* patch,
                          int originX,
                          int originY,
                          unsigned char* indices,
                          unsigned char* alpha,
                          int width,
                          int height)
{
    int x;

    for (x = 0; x < patch->width; ++x)
    {
        int destX = originX + x;
        Doom::Column* column;

        if (destX < 0 || destX >= width)
            continue;

        column = (Doom::Column*) ((byte*) patch + patch->columnofs[x]);

        while (column->topdelta != 0xff)
        {
            byte* source = (byte*) column + 3;
            int count = column->length;
            int i;

            for (i = 0; i < count; ++i)
            {
                int destY = originY + column->topdelta + i;

                if (destY < 0 || destY >= height)
                    continue;

                indices[destY * width + destX] = source[i];
                alpha[destY * width + destX] = 255;
            }

            column = (Doom::Column*) ((byte*) column + column->length + 4);
        }
    }
}

static void eacpDecodeWall(
    int id, unsigned char* indices, unsigned char* alpha, int width, int height)
{
    Doom::Texture* texture = textures[id];
    int i;

    doom_memset(indices, 0, width * height);
    doom_memset(alpha, 0, width * height);

    for (i = 0; i < texture->patchcount; ++i)
    {
        Doom::TexPatch* piece = &texture->patches[i];
        Doom::Patch* patch = (Doom::Patch*) Doom::cacheLumpNum(piece->patch);

        eacpBlitPatch(
            patch, piece->originx, piece->originy, indices, alpha, width, height);
    }
}

static int eacpWallIsMasked(int id)
{
    int width = textures[id]->width;
    int height = textures[id]->height;
    int count = width * height;
    int masked = 0;
    int i;

    unsigned char* indices = (unsigned char*) doom_malloc(count);
    unsigned char* alpha = (unsigned char*) doom_malloc(count);

    if (indices != 0 && alpha != 0)
    {
        eacpDecodeWall(id, indices, alpha, width, height);

        for (i = 0; i < count; ++i)
            if (alpha[i] == 0)
            {
                masked = 1;
                break;
            }
    }

    doom_free(indices);
    doom_free(alpha);

    return masked;
}

// Which wall textures have holes, and how tall each sprite is: both are known
// only after decoding, and both are wanted before the renderer asks for a
// single pixel.
static void eacpEnsureTextureData()
{
    auto& gfx = Doom::graphicsData();

    int i;

    if (eacpTexturesReady || gfx.numtextures <= 0 || textures == 0)
        return;

    eacpWallMasked = (unsigned char*) doom_malloc(gfx.numtextures);
    eacpSpriteHeights =
        (short*) doom_malloc(gfx.numspritelumps * (int) sizeof(short));

    if (eacpWallMasked == 0 || eacpSpriteHeights == 0)
        return;

    for (i = 0; i < gfx.numtextures; ++i)
        eacpWallMasked[i] = (unsigned char) eacpWallIsMasked(i);

    for (i = 0; i < gfx.numspritelumps; ++i)
    {
        Doom::Patch* patch =
            (Doom::Patch*) Doom::cacheLumpNum(gfx.firstspritelump + i);
        eacpSpriteHeights[i] = patch->height;
    }

    eacpTexturesReady = 1;
}

int eacpDoomGetTextureCount()
{
    auto& gfx = Doom::graphicsData();

    if (gfx.numtextures <= 0 || textures == 0)
        return 0;

    return gfx.numtextures + gfx.numflats + gfx.numspritelumps;
}

EacpDoomTextureInfo eacpDoomGetTextureInfo(int id)
{
    EacpDoomTextureInfo info;
    info.width = 0;
    info.height = 0;
    info.masked = 0;

    eacpEnsureTextureData();

    if (!eacpTexturesReady || id < 0 || id >= eacpDoomGetTextureCount())
        return info;

    if (id < Doom::graphicsData().numtextures)
    {
        info.width = textures[id]->width;
        info.height = textures[id]->height;
        info.masked = eacpWallMasked[id];
    }
    else if (id < eacpSpriteBase())
    {
        info.width = EACP_FLAT_SIZE;
        info.height = EACP_FLAT_SIZE;
    }
    else
    {
        int lump = id - eacpSpriteBase();

        info.width = spritewidth[lump].toInt();
        info.height = eacpSpriteHeights[lump];
        info.masked = 1;
    }

    return info;
}

void eacpDoomGetTexturePixels(int id, unsigned char* out)
{
    auto& gfx = Doom::graphicsData();

    EacpDoomTextureInfo info = eacpDoomGetTextureInfo(id);
    int count = info.width * info.height;
    unsigned char* indices;
    unsigned char* alpha;
    int i;

    if (out == 0 || count <= 0)
        return;

    if (id >= gfx.numtextures && id < eacpSpriteBase())
    {
        byte* flat =
            (byte*) Doom::cacheLumpNum(gfx.firstflat + (id - gfx.numtextures));
        doom_memcpy(out, flat, EACP_FLAT_SIZE * EACP_FLAT_SIZE);
        return;
    }

    indices = (unsigned char*) doom_malloc(count);
    alpha = (unsigned char*) doom_malloc(count);

    if (indices == 0 || alpha == 0)
    {
        doom_free(indices);
        doom_free(alpha);
        return;
    }

    if (id < gfx.numtextures)
    {
        eacpDecodeWall(id, indices, alpha, info.width, info.height);
    }
    else
    {
        Doom::Patch* patch = (Doom::Patch*) Doom::cacheLumpNum(
            gfx.firstspritelump + (id - eacpSpriteBase()));

        doom_memset(indices, 0, count);
        doom_memset(alpha, 0, count);
        eacpBlitPatch(patch, 0, 0, indices, alpha, info.width, info.height);
    }

    // A masked texture carries its coverage in alpha, so the shader can throw
    // the empty pixels away; everything else is a bare index.
    if (info.masked)
    {
        for (i = 0; i < count; ++i)
        {
            out[i * 4 + 0] = indices[i];
            out[i * 4 + 1] = 0;
            out[i * 4 + 2] = 0;
            out[i * 4 + 3] = alpha[i];
        }
    }
    else
    {
        for (i = 0; i < count; ++i)
            out[i] = indices[i];
    }

    doom_free(indices);
    doom_free(alpha);
}

void eacpDoomGetColormaps(unsigned char* out)
{
    if (out == 0 || colormaps == 0)
        return;

    doom_memcpy(out, colormaps, 256 * EACP_DOOM_COLORMAP_ROWS);
}

// The COLORMAP row a surface resolves through, and whether distance darkens it
// further.
typedef struct
{
    float row;
    float falloff;
} EacpLight;

// The row the whole view is locked to, or zero for none: the invulnerability
// sphere picks the inverse map and the light-amp visor the brightest row
// (P_PlayerThink), and R_SetupFrame then puts every wall, flat, sprite and the
// weapon through it, with the sector's brightness and the distance falloff both
// ignored.
static int eacpFixedRow()
{
    auto& players_ = Doom::playerState();

    return players_.players[players_.displayplayer].fixedcolormap;
}

// One row, whatever the distance - what the engine does with a bare colormap
// pointer rather than a light table.
static EacpLight eacpFixedLight(int row)
{
    EacpLight light;

    light.row = (float) row;
    light.falloff = 0.0f;

    return light;
}

// The row a surface starts at before distance darkens it further: the engine's
// light level scaled into the 32 maps, offset by the sector's brightness and the
// fake contrast walls get for their orientation.
static EacpLight eacpSectorLight(int lightlevel, int contrast)
{
    EacpLight light;
    int lightnum;

    if (eacpFixedRow())
        return eacpFixedLight(eacpFixedRow());

    lightnum =
        (lightlevel >> Doom::LIGHTSEGSHIFT) + Doom::lighting().extralight + contrast;

    if (lightnum < 0)
        lightnum = 0;

    if (lightnum >= Doom::LIGHTLEVELS)
        lightnum = Doom::LIGHTLEVELS - 1;

    light.row = (float) ((Doom::LIGHTLEVELS - 1 - lightnum) * 2 * Doom::NUMCOLORMAPS
                         / Doom::LIGHTLEVELS);
    light.falloff = 1.0f;

    return light;
}

// A frame the engine marks as lit - a muzzle flash, a rocket - is drawn through
// row 0 at any distance. A powerup outranks it (R_ProjectSprite tests
// fixedcolormap first), and this says both.
static EacpLight eacpFullbrightLight()
{
    return eacpFixedLight(eacpFixedRow());
}

// Sutherland-Hodgman against one half-plane. The side test matches the
// engine's own Doom::pointOnSide: a point is in front of a directed line when
// dy * (px - x) - dx * (py - y) is positive.
static void eacpClipToLine(const EacpPoint* in,
                           int inCount,
                           double x,
                           double y,
                           double dx,
                           double dy,
                           int keepFront,
                           EacpPoint* out,
                           int* outCount)
{
    int i;
    int count = 0;

    for (i = 0; i < inCount; ++i)
    {
        const EacpPoint* a = &in[i];
        const EacpPoint* b = &in[(i + 1) % inCount];

        double sideA = dy * (a->x - x) - dx * (a->y - y);
        double sideB = dy * (b->x - x) - dx * (b->y - y);

        if (!keepFront)
        {
            sideA = -sideA;
            sideB = -sideB;
        }

        if (sideA >= -EACP_CLIP_EPSILON && count < EACP_MAX_POLY_VERTICES)
            out[count++] = *a;

        if (((sideA > EACP_CLIP_EPSILON) && (sideB < -EACP_CLIP_EPSILON))
            || ((sideA < -EACP_CLIP_EPSILON) && (sideB > EACP_CLIP_EPSILON)))
        {
            double t = sideA / (sideA - sideB);

            if (count < EACP_MAX_POLY_VERTICES)
            {
                out[count].x = a->x + t * (b->x - a->x);
                out[count].y = a->y + t * (b->y - a->y);
                ++count;
            }
        }
    }

    *outCount = count;
}

// A subsector's cell, trimmed to the sector boundary: the segs bound it on the
// sides that came from real linedefs, and the interior always lies in front of
// them.
static void eacpStoreSubsector(int index, const EacpPoint* poly, int count)
{
    EacpPoint current[EACP_MAX_POLY_VERTICES];
    EacpPoint clipped[EACP_MAX_POLY_VERTICES];
    int currentCount = count;
    int clippedCount;
    int i;

    for (i = 0; i < count; ++i)
        current[i] = poly[i];

    for (i = 0; i < subsectors[index].numlines && currentCount >= 3; ++i)
    {
        Doom::Seg* seg = &segs[subsectors[index].firstline + i];

        double x = eacpFixedToDouble(seg->v1->x);
        double y = eacpFixedToDouble(seg->v1->y);
        double dx = eacpFixedToDouble(seg->v2->x) - x;
        double dy = eacpFixedToDouble(seg->v2->y) - y;

        eacpClipToLine(
            current, currentCount, x, y, dx, dy, 1, clipped, &clippedCount);

        for (currentCount = 0; currentCount < clippedCount; ++currentCount)
            current[currentCount] = clipped[currentCount];
    }

    if (currentCount < 3)
        return;

    eacpPolyStart[index] = eacpPolyTotal;
    eacpPolyCount[index] = currentCount;

    for (i = 0; i < currentCount; ++i)
    {
        eacpPolyVertices[(eacpPolyTotal + i) * 2 + 0] = (float) current[i].x;
        eacpPolyVertices[(eacpPolyTotal + i) * 2 + 1] = (float) current[i].y;
    }

    eacpPolyTotal += currentCount;
}

// Vanilla nodes carry no polygons - only the split planes - so each
// subsector's shape is recovered by carrying a huge square down the BSP and
// clipping it by every partition on the way to the leaf.
static void eacpDescend(int nodenum, const EacpPoint* poly, int count)
{
    EacpPoint clipped[EACP_MAX_POLY_VERTICES];
    int clippedCount;
    Doom::Node* node;
    double x, y, dx, dy;

    if (count < 3)
        return;

    if (nodenum & Doom::NF_SUBSECTOR)
    {
        eacpStoreSubsector(nodenum & ~Doom::NF_SUBSECTOR, poly, count);
        return;
    }

    node = &nodes[nodenum];
    x = eacpFixedToDouble(node->x);
    y = eacpFixedToDouble(node->y);
    dx = eacpFixedToDouble(node->dx);
    dy = eacpFixedToDouble(node->dy);

    eacpClipToLine(poly, count, x, y, dx, dy, 1, clipped, &clippedCount);
    eacpDescend(node->children[0], clipped, clippedCount);

    eacpClipToLine(poly, count, x, y, dx, dy, 0, clipped, &clippedCount);
    eacpDescend(node->children[1], clipped, clippedCount);
}

static int eacpEnsurePolyStorage()
{
    doom_free(eacpPolyVertices);
    doom_free(eacpPolyStart);
    doom_free(eacpPolyCount);

    eacpPolyVertices = (float*) doom_malloc(numsubsectors * EACP_MAX_POLY_VERTICES
                                            * 2 * (int) sizeof(float));
    eacpPolyStart = (int*) doom_malloc(numsubsectors * (int) sizeof(int));
    eacpPolyCount = (int*) doom_malloc(numsubsectors * (int) sizeof(int));

    return eacpPolyVertices != 0 && eacpPolyStart != 0 && eacpPolyCount != 0;
}

static void eacpMeasureLines()
{
    int i;

    doom_free(eacpLineLengths);
    eacpLineLengths = (float*) doom_malloc(numlines * (int) sizeof(float));

    if (eacpLineLengths == 0)
        return;

    for (i = 0; i < numlines; ++i)
    {
        double dx = eacpFixedToDouble(lines[i].dx);
        double dy = eacpFixedToDouble(lines[i].dy);

        eacpLineLengths[i] = (float) eacpSqrt(dx * dx + dy * dy);
    }
}

// The BSP is static for a level, so the cells are rebuilt only when a new one
// loads; per-frame the geometry pass just re-reads the (moving) heights.
static void eacpEnsureLevel()
{
    auto& session = Doom::gameSession();

    EacpPoint square[4];
    int i;

    if (nodes == eacpCachedNodes && numsubsectors == eacpCachedSubsectors
        && session.gameepisode == eacpCachedEpisode
        && session.gamemap == eacpCachedMap)
        return;

    eacpCachedNodes = nodes;
    eacpCachedSubsectors = numsubsectors;
    eacpCachedEpisode = session.gameepisode;
    eacpCachedMap = session.gamemap;

    eacpPolyTotal = 0;

    if (numsubsectors <= 0 || numnodes <= 0 || nodes == 0)
        return;

    if (!eacpEnsurePolyStorage())
        return;

    eacpMeasureLines();

    for (i = 0; i < numsubsectors; ++i)
    {
        eacpPolyStart[i] = 0;
        eacpPolyCount[i] = 0;
    }

    square[0].x = -EACP_MAP_LIMIT;
    square[0].y = -EACP_MAP_LIMIT;
    square[1].x = EACP_MAP_LIMIT;
    square[1].y = -EACP_MAP_LIMIT;
    square[2].x = EACP_MAP_LIMIT;
    square[2].y = EACP_MAP_LIMIT;
    square[3].x = -EACP_MAP_LIMIT;
    square[3].y = EACP_MAP_LIMIT;

    eacpDescend(numnodes - 1, square, 4);
}

static int eacpEnsureScratch(int textureCount)
{
    if (eacpScratchTextures == textureCount && eacpTextureCounts != 0)
        return 1;

    doom_free(eacpTextureCounts);
    doom_free(eacpTextureCursors);

    eacpTextureCounts = (int*) doom_malloc(textureCount * (int) sizeof(int));
    eacpTextureCursors = (int*) doom_malloc(textureCount * (int) sizeof(int));
    eacpScratchTextures = textureCount;

    return eacpTextureCounts != 0 && eacpTextureCursors != 0;
}

static void eacpEmitVertex(EacpEmitter* em,
                           int textureId,
                           float x,
                           float y,
                           float z,
                           float u,
                           float v,
                           EacpLight light)
{
    EacpDoomVertex* out;

    if (em->vertices == 0)
    {
        em->counts[textureId]++;
        return;
    }

    if (em->cursors[textureId] < 0)
        return;

    out = &em->vertices[em->cursors[textureId]++];
    out->position[0] = x;
    out->position[1] = y;
    out->position[2] = z;
    out->uv[0] = u;
    out->uv[1] = v;
    out->light = light.row;
    out->falloff = light.falloff;
}

// DOOM's map plane is (x, y) with z up; the renderer's is (x, up, -y).
static void eacpEmitWallQuad(EacpEmitter* em,
                             int textureId,
                             double x1,
                             double y1,
                             double x2,
                             double y2,
                             float bottom,
                             float top,
                             float textureTop,
                             float uStart,
                             float uEnd,
                             float textureHeight,
                             EacpLight light)
{
    float u1 = uStart;
    float u2 = uEnd;
    float vTop = (textureTop - top) / textureHeight;
    float vBottom = (textureTop - bottom) / textureHeight;

    float ax = (float) x1;
    float az = (float) -y1;
    float bx = (float) x2;
    float bz = (float) -y2;

    if (top <= bottom)
        return;

    eacpEmitVertex(em, textureId, ax, bottom, az, u1, vBottom, light);
    eacpEmitVertex(em, textureId, bx, bottom, bz, u2, vBottom, light);
    eacpEmitVertex(em, textureId, bx, top, bz, u2, vTop, light);

    eacpEmitVertex(em, textureId, ax, bottom, az, u1, vBottom, light);
    eacpEmitVertex(em, textureId, bx, top, bz, u2, vTop, light);
    eacpEmitVertex(em, textureId, ax, top, az, u1, vTop, light);
}

static void eacpEmitLineSide(EacpEmitter* em, Doom::Line* line, int index, int s)
{
    auto& sky = Doom::skyState();

    Doom::Side* side;
    Doom::Sector* front;
    Doom::Sector* back;
    Doom::Vertex* v1;
    Doom::Vertex* v2;
    double x1, y1, x2, y2;
    float length;
    float frontFloor, frontCeiling;
    float uStart, uEnd, rowOffset;
    EacpLight light;
    int contrast;

    if (line->sidenum[s] < 0)
        return;

    side = &sides[line->sidenum[s]];
    front = side->sector;
    back = line->sidenum[s ^ 1] >= 0 ? sides[line->sidenum[s ^ 1]].sector : 0;

    v1 = s == 0 ? line->v1 : line->v2;
    v2 = s == 0 ? line->v2 : line->v1;

    x1 = eacpFixedToDouble(v1->x);
    y1 = eacpFixedToDouble(v1->y);
    x2 = eacpFixedToDouble(v2->x);
    y2 = eacpFixedToDouble(v2->y);
    length = eacpLineLengths[index];

    frontFloor = eacpFloorHeight(front);
    frontCeiling = eacpCeilingHeight(front);
    rowOffset = eacpFixedToFloat(side->rowoffset);

    // The software renderer's fake contrast: walls running east-west are a
    // step darker, north-south a step brighter, so corners stay readable.
    contrast = 0;

    if (line->v1->y == line->v2->y)
        contrast = -1;
    else if (line->v1->x == line->v2->x)
        contrast = 1;

    light = eacpSectorLight(front->lightlevel, contrast);

    if (back == 0)
    {
        int texture = texturetranslation[side->midtexture];
        float textureWidth, textureHeight, textureTop;

        if (side->midtexture <= 0)
            return;

        textureWidth = (float) textures[texture]->width;
        textureHeight = (float) textures[texture]->height;

        textureTop = (line->flags & Doom::ML_DONTPEGBOTTOM)
                         ? frontFloor + textureHeight
                         : frontCeiling;
        textureTop += rowOffset;

        uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
        uEnd = uStart + length / textureWidth;

        eacpEmitWallQuad(em,
                         texture,
                         x1,
                         y1,
                         x2,
                         y2,
                         frontFloor,
                         frontCeiling,
                         textureTop,
                         uStart,
                         uEnd,
                         textureHeight,
                         light);
        return;
    }

    {
        float backFloor = eacpFloorHeight(back);
        float backCeiling = eacpCeilingHeight(back);

        if (side->bottomtexture > 0 && backFloor > frontFloor)
        {
            int texture = texturetranslation[side->bottomtexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float textureTop =
                (line->flags & Doom::ML_DONTPEGBOTTOM) ? frontCeiling : backFloor;
            textureTop += rowOffset;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + length / textureWidth;

            eacpEmitWallQuad(em,
                             texture,
                             x1,
                             y1,
                             x2,
                             y2,
                             frontFloor,
                             backFloor,
                             textureTop,
                             uStart,
                             uEnd,
                             textureHeight,
                             light);
        }

        // Between two sky ceilings the step is invisible sky (the classic sky
        // hack), not an upper wall.
        if (side->toptexture > 0 && backCeiling < frontCeiling
            && !(front->ceilingpic == sky.skyflatnum
                 && back->ceilingpic == sky.skyflatnum))
        {
            int texture = texturetranslation[side->toptexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float textureTop = (line->flags & Doom::ML_DONTPEGTOP)
                                   ? frontCeiling
                                   : backCeiling + textureHeight;
            textureTop += rowOffset;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + length / textureWidth;

            eacpEmitWallQuad(em,
                             texture,
                             x1,
                             y1,
                             x2,
                             y2,
                             backCeiling,
                             frontCeiling,
                             textureTop,
                             uStart,
                             uEnd,
                             textureHeight,
                             light);
        }

        // The middle texture of a two-sided line - a grate, a window, a hanging
        // vine. It is masked, and unlike the walls above it never tiles: DOOM
        // draws it once, clipped to the opening, which is why a too-short
        // midtexture leaves a gap rather than repeating.
        if (side->midtexture > 0)
        {
            int texture = texturetranslation[side->midtexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float openingBottom = backFloor > frontFloor ? backFloor : frontFloor;
            float openingTop =
                backCeiling < frontCeiling ? backCeiling : frontCeiling;

            float textureTop = (line->flags & Doom::ML_DONTPEGBOTTOM)
                                   ? openingBottom + textureHeight
                                   : openingTop;
            float top;
            float bottom;

            textureTop += rowOffset;

            top = textureTop < openingTop ? textureTop : openingTop;
            bottom = textureTop - textureHeight;

            if (bottom < openingBottom)
                bottom = openingBottom;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + length / textureWidth;

            eacpEmitWallQuad(em,
                             texture,
                             x1,
                             y1,
                             x2,
                             y2,
                             bottom,
                             top,
                             textureTop,
                             uStart,
                             uEnd,
                             textureHeight,
                             light);
        }
    }
}

// Every thing in the level - monsters, items, decorations, the player's
// corpse - as a quad facing the camera, exactly where DOOM's own sprite
// projection would put it: the sprite's left edge sits its own offset to the
// left of the thing's position along the view plane, and its top sits the
// sprite's top offset above the thing's feet.
static void eacpEmitSprite(EacpEmitter* em,
                           Doom::Mobj* thing,
                           Doom::Mobj* viewer,
                           double rightX,
                           double rightY)
{
    auto& gfx = Doom::graphicsData();

    Doom::SpriteDef* definition;
    Doom::SpriteFrame* frame;
    int rotation = 0;
    int lump;
    int flip;
    EacpLight light;
    double leftX, leftY;
    float width, height, top, bottom;
    float u0, u1;

    if (thing == viewer || thing->sprite < 0 || thing->sprite >= gfx.numsprites)
        return;

    definition = &sprites[thing->sprite];

    if ((int) (thing->frame & Doom::FF_FRAMEMASK) >= definition->numframes)
        return;

    frame = &definition->spriteframes[thing->frame & Doom::FF_FRAMEMASK];

    // Eight drawings per frame, one per facing: which one shows depends on the
    // angle the thing is seen from.
    if (frame->rotate)
    {
        angle_t seen = Doom::pointToAngle2(viewer->x, viewer->y, thing->x, thing->y);
        rotation = ((seen - thing->angle + (Doom::ang45 / 2u) * 9u) >> 29).raw;
    }

    lump = frame->lump[rotation];
    flip = frame->flip[rotation];

    if (lump < 0 || lump >= gfx.numspritelumps)
        return;

    width = (float) (spritewidth[lump].toInt());
    height = (float) eacpSpriteHeights[lump];

    // A thing moves once a tic, so drawing it where the tic left it makes it
    // step while the world glides past. Its momentum is how far it travelled to
    // get there, so winding that back by the part of the tic still to come puts
    // it where it would be at the moment being drawn.
    {
        double back = 1.0 - (double) eacpAlpha;

        double thingX =
            eacpFixedToDouble(thing->x) - eacpFixedToDouble(thing->momx) * back;
        double thingY =
            eacpFixedToDouble(thing->y) - eacpFixedToDouble(thing->momy) * back;
        float thingZ = eacpFixedToFloat(thing->z)
                       - (float) (eacpFixedToDouble(thing->momz) * back);

        leftX = thingX - rightX * eacpFixedToDouble(spriteoffset[lump]);
        leftY = thingY - rightY * eacpFixedToDouble(spriteoffset[lump]);

        top = thingZ + eacpFixedToFloat(spritetopoffset[lump]);
        bottom = top - height;
    }

    light = (thing->frame & Doom::FF_FULLBRIGHT)
                ? eacpFullbrightLight()
                : eacpSectorLight(thing->subsector->sector->lightlevel, 0);

    u0 = flip ? 1.0f : 0.0f;
    u1 = flip ? 0.0f : 1.0f;

    {
        int texture = eacpSpriteBase() + lump;

        float ax = (float) leftX;
        float az = (float) -leftY;
        float bx = (float) (leftX + rightX * width);
        float bz = (float) -(leftY + rightY * width);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, 1.0f, light);
        eacpEmitVertex(em, texture, bx, bottom, bz, u1, 1.0f, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, 0.0f, light);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, 1.0f, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, 0.0f, light);
        eacpEmitVertex(em, texture, ax, top, az, u0, 0.0f, light);
    }
}

// The engine's 32-bit angle for a heading in radians, so the view being drawn
// can index the same sine tables the engine uses.
static angle_t eacpAngleFromRadians(float radians)
{
    return (angle_t) (unsigned) (long long) ((double) radians
                                             * (2147483648.0 / 3.14159265358979));
}

static void eacpEmitSprites(EacpEmitter* em,
                            Doom::Mobj* viewer,
                            const EacpDoomCamera* camera)
{
    auto& thinkers = Doom::thinkerList();

    Doom::Thinker* thinker;

    // The view plane's right axis, the one DOOM measures a sprite's width
    // along, so the billboards stay square-on to the camera being drawn.
    angle_t facing = eacpAngleFromRadians(camera->angle);

    double rightX = eacpFixedToDouble(finesine[facing.fineIndex()]);
    double rightY = -eacpFixedToDouble(finecosine[facing.fineIndex()]);

    for (thinker = thinkers.cap.next; thinker != &thinkers.cap;
         thinker = thinker->next)
    {
        // A mobj is a Thinker whose virtual kind() is Doom::Mobj (was the function.acp1 ==
        // Doom::mobjThinker identity, before Doom::Thinker became a real Doom::Thinker);
        // skip a removed-but-not-yet-freed one, as the engine's own scans do.
        if (thinker->kind() != Doom::ThinkerKind::Mobj || thinker->removed)
            continue;

        eacpEmitSprite(em, (Doom::Mobj*) thinker, viewer, rightX, rightY);
    }
}

// The sky is not geometry in DOOM: it is painted wherever a ceiling is missing,
// at a column picked by the direction the player faces. A cylinder around the
// camera reproduces that - it never moves relative to the viewer, so it has no
// parallax, and its texture repeats four times around, as the engine's does.
static void eacpEmitSky(EacpEmitter* em, const EacpDoomCamera* camera)
{
    auto& sky = Doom::skyState();

    int texture;
    int i;
    double camX, camY;
    float camZ;
    float vTop, vBottom;

    // "Sky is allways drawn full bright, i.e. colormaps[0] is used. Because of
    // this hack, sky is not affected by INVUL inverse mapping" - Doom::drawPlanes,
    // whose words those are. Row 0 at any distance, and through any powerup.
    EacpLight light = eacpFixedLight(0);

    if (sky.skytexture <= 0 || sky.skytexture >= Doom::graphicsData().numtextures)
        return;

    texture = texturetranslation[sky.skytexture];

    camX = camera->x;
    camY = camera->y;
    camZ = camera->z;

    // DOOM pins the sky to screen rows, with row 100 on the horizon. A screen
    // row is linear in height on the cylinder, so two rings are exact.
    vTop = (100.0f - EACP_SKY_FOCAL * EACP_SKY_HEIGHT / EACP_SKY_RADIUS) / 128.0f;
    vBottom = (100.0f + EACP_SKY_FOCAL * EACP_SKY_HEIGHT / EACP_SKY_RADIUS) / 128.0f;

    for (i = 0; i < EACP_SKY_SEGMENTS; ++i)
    {
        angle_t a0 = (angle_t) i << 26;
        angle_t a1 = (angle_t) (i + 1) << 26;

        double x0 =
            camX + EACP_SKY_RADIUS * eacpFixedToDouble(finecosine[a0.fineIndex()]);
        double y0 =
            camY + EACP_SKY_RADIUS * eacpFixedToDouble(finesine[a0.fineIndex()]);
        double x1 =
            camX + EACP_SKY_RADIUS * eacpFixedToDouble(finecosine[a1.fineIndex()]);
        double y1 =
            camY + EACP_SKY_RADIUS * eacpFixedToDouble(finesine[a1.fineIndex()]);

        float u0 = 4.0f * (float) i / (float) EACP_SKY_SEGMENTS;
        float u1 = 4.0f * (float) (i + 1) / (float) EACP_SKY_SEGMENTS;

        float top = camZ + EACP_SKY_HEIGHT;
        float bottom = camZ - EACP_SKY_HEIGHT;

        float ax = (float) x0;
        float az = (float) -y0;
        float bx = (float) x1;
        float bz = (float) -y1;

        eacpEmitVertex(em, texture, ax, bottom, az, u0, vBottom, light);
        eacpEmitVertex(em, texture, bx, bottom, bz, u1, vBottom, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, vTop, light);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, vBottom, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, vTop, light);
        eacpEmitVertex(em, texture, ax, top, az, u0, vTop, light);
    }
}

// R_DrawPSprite lights the weapon at the *nearest* entry of the scale table -
// it is right up against the camera - and that entry is this many rows brighter
// than the sector's start map. Reading the start map alone lights the weapon as
// if it stood infinitely far away, which is nearly black in a dim room and
// visibly dark in almost any room: DOOM's weapon is fullbright in every sector
// above light level 240 and close to it well below that.
static float eacpWeaponBrightening()
{
    auto& view = Doom::viewWindow();

    int width = view.viewwidth << view.detailshift;

    if (width <= 0)
        return 0.0f;

    return (float) ((Doom::MAXLIGHTSCALE - 1) * Doom::SCREENWIDTH / width
                    / Doom::DISTMAP);
}

// R_DrawPSprite's choice of colormap, in its own order: a powerup first, then a
// lit frame, then the sector.
static float eacpWeaponLight(int fullbright)
{
    auto& players_ = Doom::playerState();

    float row;

    if (eacpFixedRow())
        return (float) eacpFixedRow();

    if (fullbright)
        return 0.0f;

    row = eacpSectorLight(players_.players[players_.displayplayer]
                              .mo->subsector->sector->lightlevel,
                          0)
              .row
          - eacpWeaponBrightening();

    return row < 0.0f ? 0.0f : row;
}

// The engine places the weapon in a 320x200 space centred on row 100
// (BASEYCENTER), and R_DrawPSprite lands that centre on the middle row of
// whatever the view is: row 84 with the status bar up, row 100 without it.
static float eacpWeaponRowShift()
{
    return 100.0f - eacpDoomViewRows() * 0.5f;
}

void eacpDoomGetHudSprites(EacpDoomHudSprite* out)
{
    auto& players_ = Doom::playerState();

    auto& gfx = Doom::graphicsData();

    Doom::Player* player = &players_.players[players_.displayplayer];
    int i;

    if (out == 0)
        return;

    for (i = 0; i < EACP_DOOM_HUD_SPRITES; ++i)
        out[i].textureId = -1;

    eacpEnsureTextureData();

    if (!eacpTexturesReady || Doom::gameFlow().gamestate != Doom::GS_LEVEL
        || player->mo == 0)
        return;

    for (i = 0; i < Doom::NUMPSPRITES && i < EACP_DOOM_HUD_SPRITES; ++i)
    {
        Doom::PspDef* weapon = &player->psprites[i];
        Doom::State* state = weapon->state;
        Doom::SpriteDef* definition;
        Doom::SpriteFrame* frame;
        int lump;

        if (state == 0 || state->sprite < 0 || state->sprite >= gfx.numsprites)
            continue;

        definition = &sprites[state->sprite];

        if ((int) (state->frame & Doom::FF_FRAMEMASK) >= definition->numframes)
            continue;

        frame = &definition->spriteframes[state->frame & Doom::FF_FRAMEMASK];
        lump = frame->lump[0];

        if (lump < 0 || lump >= gfx.numspritelumps)
            continue;

        out[i].textureId = eacpSpriteBase() + lump;
        out[i].width = (float) (spritewidth[lump].toInt());
        out[i].height = (float) eacpSpriteHeights[lump];

        out[i].x =
            eacpFixedToFloat(weapon->sx) - eacpFixedToFloat(spriteoffset[lump]);
        out[i].y = eacpFixedToFloat(weapon->sy)
                   - eacpFixedToFloat(spritetopoffset[lump]) - eacpWeaponRowShift();

        out[i].light = eacpWeaponLight(state->frame & Doom::FF_FULLBRIGHT);
        out[i].flip = frame->flip[0];
    }
}

static void
    eacpEmitFlat(EacpEmitter* em, int index, int flat, float height, EacpLight light)
{
    int textureId = Doom::graphicsData().numtextures + flattranslation[flat];
    const float* poly = &eacpPolyVertices[eacpPolyStart[index] * 2];
    int count = eacpPolyCount[index];
    int i;

    for (i = 1; i + 1 < count; ++i)
    {
        int corners[3];
        int c;

        corners[0] = 0;
        corners[1] = i;
        corners[2] = i + 1;

        for (c = 0; c < 3; ++c)
        {
            float x = poly[corners[c] * 2 + 0];
            float y = poly[corners[c] * 2 + 1];

            eacpEmitVertex(em,
                           textureId,
                           x,
                           height,
                           -y,
                           x / (float) EACP_FLAT_SIZE,
                           -y / (float) EACP_FLAT_SIZE,
                           light);
        }
    }
}

static void eacpEmitSubsector(EacpEmitter* em, int index)
{
    auto& sky = Doom::skyState();

    Doom::Sector* sector = subsectors[index].sector;
    EacpLight light;

    if (eacpPolyCount[index] < 3 || sector == 0)
        return;

    light = eacpSectorLight(sector->lightlevel, 0);

    // Either way round, a sky flat is a hole onto the sky rather than a surface:
    // Doom::drawPlanes paints the sky wherever it finds one, and a floor is as free
    // to carry it as a ceiling.
    if (sector->floorpic != sky.skyflatnum)
        eacpEmitFlat(em, index, sector->floorpic, eacpFloorHeight(sector), light);

    if (sector->ceilingpic != sky.skyflatnum)
        eacpEmitFlat(
            em, index, sector->ceilingpic, eacpCeilingHeight(sector), light);
}

static void eacpEmitWorld(EacpEmitter* em, const EacpDoomCamera* camera)
{
    auto& players_ = Doom::playerState();

    Doom::Player* player = &players_.players[players_.displayplayer];
    int i;

    for (i = 0; i < numlines; ++i)
    {
        eacpEmitLineSide(em, &lines[i], i, 0);
        eacpEmitLineSide(em, &lines[i], i, 1);
    }

    for (i = 0; i < numsubsectors; ++i)
        eacpEmitSubsector(em, i);

    if (player->mo == 0)
        return;

    eacpEmitSky(em, camera);
    eacpEmitSprites(em, player->mo, camera);
}

int eacpDoomBuildGeometry(const EacpDoomCamera* camera,
                          float alpha,
                          EacpDoomVertex* vertices,
                          int maxVertices,
                          EacpDoomDraw* draws,
                          int maxDraws,
                          int* outVertexCount)
{
    EacpEmitter em;
    int textureCount = eacpDoomGetTextureCount();
    int total = 0;
    int drawCount = 0;
    int i;

    if (outVertexCount != 0)
        *outVertexCount = 0;

    eacpAlpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    eacpEnsureTextureData();

    if (Doom::gameFlow().gamestate != Doom::GS_LEVEL || vertices == 0 || draws == 0
        || camera == 0 || textureCount <= 0 || lines == 0 || !eacpTexturesReady)
        return 0;

    eacpEnsureLevel();

    if (eacpPolyTotal <= 0 || !eacpEnsureScratch(textureCount))
        return 0;

    for (i = 0; i < textureCount; ++i)
        eacpTextureCounts[i] = 0;

    em.counts = eacpTextureCounts;
    em.cursors = eacpTextureCursors;
    em.vertices = 0;
    eacpEmitWorld(&em, camera);

    // Each texture's vertices become one contiguous run, so the frame draws
    // once per texture with no state changes in between.
    for (i = 0; i < textureCount; ++i)
    {
        int count = eacpTextureCounts[i];

        if (count <= 0 || drawCount >= maxDraws || total + count > maxVertices)
        {
            eacpTextureCursors[i] = -1;
            continue;
        }

        eacpTextureCursors[i] = total;

        draws[drawCount].textureId = i;
        draws[drawCount].firstVertex = total;
        draws[drawCount].vertexCount = count;
        ++drawCount;

        total += count;
    }

    em.vertices = vertices;
    eacpEmitWorld(&em, camera);

    if (outVertexCount != 0)
        *outVertexCount = total;

    return drawCount;
}

// The automap, as geometry rather than as a rasterized frame.
//
// What is drawn, and in what colour, is Doom::drawAutomap's own choice, mirrored below:
// only its rasterizer (AM_drawFline, a Bresenham walk straight into the 320 x
// 168 frame) is replaced. The shapes it draws the player and the things with -
// player_arrow, cheat_player_arrow, thintriangle_guy - and the rotation it puts
// them through are the engine's, used here as they stand.

typedef struct
{
    EacpDoomAutomapVertex* vertices;
    int count;
    int max;

    // The map point the frame's lower-left corner sits on, in fixed-point map
    // units, and how many frame pixels one of those units spans.
    double originX;
    double originY;
    double scale;
} EacpAutomapEmitter;

static void eacpAutomapCorner(EacpAutomapEmitter* em,
                              double x,
                              double y,
                              double dx,
                              double dy,
                              float side,
                              float color)
{
    EacpDoomAutomapVertex* vertex = &em->vertices[em->count++];

    vertex->position[0] = (float) x;
    vertex->position[1] = (float) y;
    vertex->direction[0] = (float) dx;
    vertex->direction[1] = (float) dy;
    vertex->side = side;
    vertex->color = color;
}

// One line of the map, in frame coordinates, as the two triangles of a quad the
// vertex shader widens.
static void eacpAutomapFrameLine(
    EacpAutomapEmitter* em, double ax, double ay, double bx, double by, int color)
{
    double dx = bx - ax;
    double dy = by - ay;
    float shade = (float) (color & 0xff);

    if (em->count + 6 > em->max || (dx == 0.0 && dy == 0.0))
        return;

    eacpAutomapCorner(em, ax, ay, dx, dy, 1.0f, shade);
    eacpAutomapCorner(em, bx, by, dx, dy, 1.0f, shade);
    eacpAutomapCorner(em, bx, by, dx, dy, -1.0f, shade);

    eacpAutomapCorner(em, ax, ay, dx, dy, 1.0f, shade);
    eacpAutomapCorner(em, bx, by, dx, dy, -1.0f, shade);
    eacpAutomapCorner(em, ax, ay, dx, dy, -1.0f, shade);
}

// The same line, in map coordinates. CXMTOF and CYMTOF's transform, in floating
// point and without their rounding to whole pixels: the map's y runs up and the
// frame's runs down.
static void eacpAutomapLine(EacpAutomapEmitter* em,
                            fixed_t x1,
                            fixed_t y1,
                            fixed_t x2,
                            fixed_t y2,
                            int color)
{
    double ax = (double) f_x + (eacpFixedToDouble(x1) - em->originX) * em->scale;
    double ay = (double) f_y + (double) f_h
                - (eacpFixedToDouble(y1) - em->originY) * em->scale;
    double bx = (double) f_x + (eacpFixedToDouble(x2) - em->originX) * em->scale;
    double by = (double) f_y + (double) f_h
                - (eacpFixedToDouble(y2) - em->originY) * em->scale;

    eacpAutomapFrameLine(em, ax, ay, bx, by, color);
}

// AM_drawLineCharacter, emitting instead of rasterizing.
static void eacpAutomapLineCharacter(EacpAutomapEmitter* em,
                                     Doom::MapLine* lineguy,
                                     int lineguylines,
                                     fixed_t scale,
                                     angle_t angle,
                                     int color,
                                     fixed_t x,
                                     fixed_t y)
{
    int i;
    Doom::MapLine l;

    for (i = 0; i < lineguylines; i++)
    {
        l.a.x = lineguy[i].a.x;
        l.a.y = lineguy[i].a.y;
        l.b.x = lineguy[i].b.x;
        l.b.y = lineguy[i].b.y;

        if (scale)
        {
            l.a.x = FixedMul(scale, l.a.x);
            l.a.y = FixedMul(scale, l.a.y);
            l.b.x = FixedMul(scale, l.b.x);
            l.b.y = FixedMul(scale, l.b.y);
        }

        if (angle)
        {
            Doom::rotateAutomapPoint(&l.a.x, &l.a.y, angle);
            Doom::rotateAutomapPoint(&l.b.x, &l.b.y, angle);
        }

        eacpAutomapLine(em, l.a.x + x, l.a.y + y, l.b.x + x, l.b.y + y, color);
    }
}

static void eacpAutomapWalls(EacpAutomapEmitter* em)
{
    int i;

    for (i = 0; i < numlines; i++)
    {
        Doom::Line* line = &lines[i];
        fixed_t ax = line->v1->x;
        fixed_t ay = line->v1->y;
        fixed_t bx = line->v2->x;
        fixed_t by = line->v2->y;

        if (cheating || (line->flags & Doom::ML_MAPPED))
        {
            if ((line->flags & LINE_NEVERSEE) && !cheating)
                continue;

            if (!line->backsector)
                eacpAutomapLine(em, ax, ay, bx, by, WALLCOLORS + lightlev);
            else if (line->special == 39)
                eacpAutomapLine(em, ax, ay, bx, by, WALLCOLORS + WALLRANGE / 2);
            else if (line->flags & Doom::ML_SECRET)
                eacpAutomapLine(em,
                                ax,
                                ay,
                                bx,
                                by,
                                cheating ? SECRETWALLCOLORS + lightlev
                                         : WALLCOLORS + lightlev);
            else if (line->backsector->floorheight != line->frontsector->floorheight)
                eacpAutomapLine(em, ax, ay, bx, by, FDWALLCOLORS + lightlev);
            else if (line->backsector->ceilingheight
                     != line->frontsector->ceilingheight)
                eacpAutomapLine(em, ax, ay, bx, by, CDWALLCOLORS + lightlev);
            else if (cheating)
                eacpAutomapLine(em, ax, ay, bx, by, TSWALLCOLORS + lightlev);
        }
        else if (am_plr != 0 && am_plr->powers[Doom::pw_allmap])
        {
            if (!(line->flags & LINE_NEVERSEE))
                eacpAutomapLine(em, ax, ay, bx, by, GRAYS + 3);
        }
    }
}

static void eacpAutomapGrid(EacpAutomapEmitter* em, int color)
{
    fixed_t block = Doom::Fixed::fromInt(Doom::MAPBLOCKUNITS);
    fixed_t originX = (fixed_t) em->originX;
    fixed_t originY = (fixed_t) em->originY;
    fixed_t x, y, start, end;

    start = originX;
    if (fixed_t {(start - bmaporgx).raw % block.raw})
        start += block - (fixed_t {(start - bmaporgx).raw % block.raw});
    end = originX + m_w;

    for (x = start; x < end; x += block)
        eacpAutomapLine(em, x, originY, x, originY + m_h, color);

    start = originY;
    if (fixed_t {(start - bmaporgy).raw % block.raw})
        start += block - (fixed_t {(start - bmaporgy).raw % block.raw});
    end = originY + m_h;

    for (y = start; y < end; y += block)
        eacpAutomapLine(em, originX, y, originX + m_w, y, color);
}

// Drawn from the view rather than from the player: the arrow is the one thing on
// the map that turns, and turning it once a tic against a map that glides is
// what would be seen.
static void eacpAutomapPlayer(EacpAutomapEmitter* em, const EacpDoomCamera* camera)
{
    fixed_t x, y;
    angle_t angle;

    if (am_plr == 0 || am_plr->mo == 0)
        return;

    x = (fixed_t) ((double) camera->x * 65536.0);
    y = (fixed_t) ((double) camera->y * 65536.0);
    angle = eacpAngleFromRadians(camera->angle);

    if (cheating)
        eacpAutomapLineCharacter(em,
                                 cheat_player_arrow,
                                 NUMCHEATPLYRLINES,
                                 fixed_t {},
                                 angle,
                                 WHITE,
                                 x,
                                 y);
    else
        eacpAutomapLineCharacter(
            em, player_arrow, NUMPLYRLINES, fixed_t {}, angle, WHITE, x, y);
}

static void eacpAutomapThings(EacpAutomapEmitter* em, int color)
{
    int i;

    for (i = 0; i < numsectors; i++)
    {
        Doom::Mobj* thing = sectors[i].thinglist;

        while (thing != 0)
        {
            eacpAutomapLineCharacter(em,
                                     thintriangle_guy,
                                     NUMTHINTRIANGLEGUYLINES,
                                     Doom::Fixed::fromInt(16),
                                     thing->angle,
                                     color + lightlev,
                                     thing->x,
                                     thing->y);
            thing = thing->snext;
        }
    }
}

// AM_drawCrosshair pokes the frame's middle pixel; a line a pixel long over it
// is the same dot, and widens with everything else.
static void eacpAutomapCrosshair(EacpAutomapEmitter* em, int color)
{
    double x = (double) f_w * 0.5;
    double y = (double) f_h * 0.5;

    eacpAutomapFrameLine(em, x - 0.5, y, x + 0.5, y, color);
}

int eacpDoomBuildAutomap(const EacpDoomCamera* camera,
                         EacpDoomAutomapVertex* vertices,
                         int maxVertices)
{
    EacpAutomapEmitter em;

    if (!Doom::overlayState().automapactive
        || Doom::gameFlow().gamestate != Doom::GS_LEVEL || camera == 0
        || vertices == 0 || lines == 0)
        return 0;

    em.vertices = vertices;
    em.count = 0;
    em.max = maxVertices;

    // MTOF in fixed point: a map unit spans scale_mtof / 2^32 frame pixels.
    em.scale = (double) scale_mtof.raw / 4294967296.0;

    // Vanilla recentres on the player once a tic and snaps the map to whole
    // frame pixels as it does it (AM_doFollowPlayer's FTOM(MTOF(x))). Following
    // the interpolated view instead, and not rounding, is what makes the map
    // glide rather than crawl. Panned by hand, it is the engine's window that
    // moves, and that still steps.
    if (followplayer && am_plr != 0 && am_plr->mo != 0)
    {
        em.originX = (double) camera->x * 65536.0 - (double) m_w.raw / 2.0;
        em.originY = (double) camera->y * 65536.0 - (double) m_h.raw / 2.0;
    }
    else
    {
        em.originX = (double) m_x.raw;
        em.originY = (double) m_y.raw;
    }

    if (grid)
        eacpAutomapGrid(&em, GRIDCOLORS);

    eacpAutomapWalls(&em);
    eacpAutomapPlayer(&em, camera);

    if (cheating == 2)
        eacpAutomapThings(&em, THINGCOLORS);

    eacpAutomapCrosshair(&em, XHAIRCOLORS);

    return em.count;
}
