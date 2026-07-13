// Included at the end of DoomImpl.c, inside the engine translation unit, so
// the functions below can use the engine's types and globals directly. Never
// compiled standalone.

#include "EngineAccess.h"

static float eacpFixedToFloat(fixed_t value)
{
    return (float) value / (float) FRACUNIT;
}

int eacpDoomWorldVisible(void)
{
    return gamestate == GS_LEVEL && !menuactive && !automapactive
           && !is_wiping_screen;
}

EacpDoomCamera eacpDoomGetCamera(void)
{
    EacpDoomCamera camera = {0, 0, 0, 0};
    player_t* player = &players[displayplayer];

    if (player->mo == 0)
        return camera;

    camera.x = eacpFixedToFloat(player->mo->x);
    camera.y = eacpFixedToFloat(player->mo->y);
    camera.z = eacpFixedToFloat(player->viewz);

    // angle_t maps the full circle onto 32 bits; 2^31 is half a turn.
    camera.angle =
        (float) ((double) player->mo->angle * (3.14159265358979 / 2147483648.0));
    return camera;
}

static int eacpEmitWall(EacpDoomWall* out,
                        int count,
                        int maxWalls,
                        line_t* line,
                        float bottom,
                        float top,
                        float light)
{
    if (count >= maxWalls || top <= bottom)
        return count;

    EacpDoomWall* wall = &out[count];
    wall->x1 = eacpFixedToFloat(line->v1->x);
    wall->y1 = eacpFixedToFloat(line->v1->y);
    wall->x2 = eacpFixedToFloat(line->v2->x);
    wall->y2 = eacpFixedToFloat(line->v2->y);
    wall->bottom = bottom;
    wall->top = top;
    wall->light = light;
    return count + 1;
}

int eacpDoomGetWalls(EacpDoomWall* out, int maxWalls)
{
    int i;
    int count = 0;

    if (gamestate != GS_LEVEL || lines == 0)
        return 0;

    for (i = 0; i < numlines; ++i)
    {
        line_t* line = &lines[i];
        sector_t* front;
        float light, frontFloor, frontCeiling;

        if (line->sidenum[0] < 0)
            continue;

        front = sides[line->sidenum[0]].sector;
        light = (float) front->lightlevel / 255.0f;
        frontFloor = eacpFixedToFloat(front->floorheight);
        frontCeiling = eacpFixedToFloat(front->ceilingheight);

        if (line->sidenum[1] < 0 || line->backsector == 0)
        {
            count = eacpEmitWall(
                out, count, maxWalls, line, frontFloor, frontCeiling, light);
        }
        else
        {
            sector_t* back = sides[line->sidenum[1]].sector;
            float backFloor = eacpFixedToFloat(back->floorheight);
            float backCeiling = eacpFixedToFloat(back->ceilingheight);

            if (backFloor != frontFloor)
                count = eacpEmitWall(out,
                                     count,
                                     maxWalls,
                                     line,
                                     backFloor < frontFloor ? backFloor : frontFloor,
                                     backFloor < frontFloor ? frontFloor : backFloor,
                                     light);

            // Between two sky ceilings the step is invisible sky (the classic
            // sky hack); everywhere else differing ceilings make an upper wall.
            if (backCeiling != frontCeiling
                && !(front->ceilingpic == skyflatnum
                     && back->ceilingpic == skyflatnum))
                count = eacpEmitWall(
                    out,
                    count,
                    maxWalls,
                    line,
                    backCeiling < frontCeiling ? backCeiling : frontCeiling,
                    backCeiling < frontCeiling ? frontCeiling : backCeiling,
                    light);
        }
    }

    return count;
}
