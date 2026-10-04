// ==============================================================================
// Ariane SA-MP Editor Sample: Streamer Dynamic Objects & Materials
// Exercising: CreateDynamicObject, SetDynamicObjectMaterial,
//             SetDynamicObjectMaterialText, RemoveBuildingForPlayer,
//             constants, expressions, arrays, comments, and string escapes.
// ==============================================================================

#include <a_samp>
#include <streamer>

// Static constants and expressions
#define BASE_SIGN_MODEL     19379
#define STREAM_DIST_DEFAULT (300.0 + 50.0)
#define DRAW_DIST_DEFAULT   (150.0)

const MATERIAL_INDEX_TEXTURE = 0;
const MATERIAL_INDEX_TEXT    = 1;
const COLOR_WHITE            = 0xFFFFFFFF;
const COLOR_BLUE_TINT        = 0xCC2244AA;
const COLOR_CARDINAL_RED     = 0xFFCC1122;

/*
 * Dynamic object array demonstrating constant-indexed and variable assignment
 */
new gDynamicSigns[4];

stock LoadStreamerSampleMap()
{
    // --------------------------------------------------------------------------
    // Object 0: Basic dynamic sign with full streamer distance & priority fields
    // --------------------------------------------------------------------------
    gDynamicSigns[0] = CreateDynamicObject(
        BASE_SIGN_MODEL,
        1540.0, -1670.0, 14.5,
        0.0, 0.0, 90.0,
        0,                  // worldid
        0,                  // interiorid
        -1,                 // playerid (all players)
        STREAM_DIST_DEFAULT,
        DRAW_DIST_DEFAULT,
        -1,                 // areaid
        1                   // priority
    );

    // Apply texture override to slot 0 with color tint
    SetDynamicObjectMaterial(
        gDynamicSigns[0],
        MATERIAL_INDEX_TEXTURE,
        BASE_SIGN_MODEL,
        "all_walls",
        "wall_stone",
        COLOR_BLUE_TINT
    );

    // --------------------------------------------------------------------------
    // Object 1: Sign with formatted material text, bold style, center alignment
    // --------------------------------------------------------------------------
    gDynamicSigns[1] = CreateDynamicObject(
        BASE_SIGN_MODEL,
        1540.0, -1665.0, 14.5,
        0.0, 0.0, 90.0,
        0, 0, -1,
        300.0, 150.0, -1, 0
    );

    SetDynamicObjectMaterialText(
        gDynamicSigns[1],
        MATERIAL_INDEX_TEXT,
        "{FFCC00}WELCOME TO ARIANE\n{FFFFFF}SA-MP Map Editor\n\"Streamer Preview\"",
        OBJECT_MATERIAL_SIZE_256x128,
        "Arial",
        22,
        1,                  // bold: true
        COLOR_WHITE,
        0xFF112233,         // dark background color
        OBJECT_MATERIAL_TEXT_ALIGN_CENTER
    );

    // --------------------------------------------------------------------------
    // Object 2: Sign with left-aligned warning text and multiline formatting
    // --------------------------------------------------------------------------
    new temporarySign = CreateDynamicObject(
        BASE_SIGN_MODEL,
        1540.0, -1660.0, 14.5,
        0.0, 0.0, 90.0
    );

    SetDynamicObjectMaterialText(
        temporarySign,
        0,
        "{FF1111}[ALERT]\n{FFFFFF}Authorized Personnel Only!\nPath: C:\\Ariane\\Maps",
        OBJECT_MATERIAL_SIZE_256x128,
        "Arial",
        20,
        1,
        COLOR_CARDINAL_RED,
        0xAA000000,
        OBJECT_MATERIAL_TEXT_ALIGN_LEFT
    );
    gDynamicSigns[2] = temporarySign;

    return 1;
}

stock RemoveStreamerSampleBuildings(playerid)
{
    // Specific model removal (e.g. model 6153 near test area)
    RemoveBuildingForPlayer(playerid, 6153, 1540.0, -1670.0, 14.5, 25.0);

    // Wildcard removal (model -1 removes all buildings within radius)
    RemoveBuildingForPlayer(playerid, -1, 1550.0, -1680.0, 15.0, 10.0);

    return 1;
}

main()
{
    print("Ariane SA-MP sample map (streamer) loaded.");
}

public OnGameModeInit()
{
    LoadStreamerSampleMap();
    return 1;
}

public OnPlayerConnect(playerid)
{
    RemoveStreamerSampleBuildings(playerid);
    return 1;
}
