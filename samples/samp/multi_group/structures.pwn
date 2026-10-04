// ==============================================================================
// Multi-group sample: Structures (File 1 of 2)
// Group name in Ariane: structures.pwn
// ==============================================================================

#include <a_samp>
#include <streamer>

stock LoadStructures()
{
    // Sign board representing primary structural element
    new structureObj = CreateDynamicObject(
        19379,
        1500.0, -1650.0, 15.0,
        0.0, 0.0, 0.0,
        0, 0, -1,
        350.0, 150.0, -1, 0
    );

    SetDynamicObjectMaterial(
        structureObj,
        0,
        19379,
        "all_walls",
        "wall_concrete",
        0xFF888888
    );

    return 1;
}

stock RemoveStructuralBuildings(playerid)
{
    RemoveBuildingForPlayer(playerid, 6153, 1500.0, -1650.0, 15.0, 30.0);
    return 1;
}

main() {}
