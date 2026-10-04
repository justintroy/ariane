// ==============================================================================
// Ariane SA-MP Editor Sample: Native SA-MP Objects & Materials
// Exercising: CreateObject, SetObjectMaterial, SetObjectMaterialText (native order),
//             RemoveBuildingForPlayer, variable reuse, and alias references.
// ==============================================================================

#include <a_samp>

#define NATIVE_SIGN_MODEL   19379
#define DEFAULT_DRAW_DIST   120.0

const SLOT_TEXTURE = 0;
const SLOT_TEXT    = 1;

stock LoadNativeSampleMap()
{
    // --------------------------------------------------------------------------
    // Object 1: Native object creation with custom draw distance
    // --------------------------------------------------------------------------
    new temporary = CreateObject(
        NATIVE_SIGN_MODEL,
        2480.0, -1660.0, 14.0,
        0.0, 0.0, 180.0,
        DEFAULT_DRAW_DIST
    );

    // Apply native material override
    SetObjectMaterial(
        temporary,
        SLOT_TEXTURE,
        NATIVE_SIGN_MODEL,
        "mat_walls",
        "brick_wall",
        0xFFEEEEEE
    );

    // --------------------------------------------------------------------------
    // Object 1 (Slot 1 Text via alias variable reference and native SetObjectMaterialText order)
    // Native order: (objectid, text[], materialindex, materialsize, fontface[], fontsize, bold, fontcolor, backcolor, textalignment)
    // --------------------------------------------------------------------------
    new aliasObj = temporary;
    SetObjectMaterialText(
        aliasObj,
        "Native SA-MP Text\n\"Direct GDI Rasterization\"\nDefault Size",
        SLOT_TEXT,
        OBJECT_MATERIAL_SIZE_256x128,
        "Arial",
        24,
        1,
        0xFFFFFFFF,
        0xCC002244,
        OBJECT_MATERIAL_TEXT_ALIGN_CENTER
    );

    // --------------------------------------------------------------------------
    // Object 2: Variable reuse pattern (assigning a new object to 'temporary')
    // --------------------------------------------------------------------------
    temporary = CreateObject(
        NATIVE_SIGN_MODEL,
        2485.0, -1660.0, 14.0,
        0.0, 0.0, 180.0
    );

    SetObjectMaterialText(
        temporary,
        "Reused Variable Sign\nSlot 0",
        0,
        OBJECT_MATERIAL_SIZE_128x64,
        "Courier New",
        18,
        0,
        0xFF00FF00,
        0xFF000000,
        OBJECT_MATERIAL_TEXT_ALIGN_LEFT
    );

    return 1;
}

stock RemoveNativeSampleBuildings(playerid)
{
    // Specific building removal
    RemoveBuildingForPlayer(playerid, 1308, 2480.0, -1660.0, 14.0, 50.0);
    return 1;
}

main()
{
    print("Ariane SA-MP native sample map loaded.");
}

public OnGameModeInit()
{
    LoadNativeSampleMap();
    return 1;
}

public OnPlayerConnect(playerid)
{
    RemoveNativeSampleBuildings(playerid);
    return 1;
}
