// ==============================================================================
// Multi-group sample: Amenities (File 2 of 2)
// Group name in Ariane: amenities.pwn
// ==============================================================================

#include <a_samp>
#include <streamer>

stock LoadAmenities()
{
    // Information sign representing amenities / fixtures
    new amenityObj = CreateDynamicObject(
        19379,
        1505.0, -1650.0, 15.0,
        0.0, 0.0, 0.0,
        0, 0, -1,
        300.0, 100.0, -1, 0
    );

    SetDynamicObjectMaterialText(
        amenityObj,
        0,
        "{00AAFF}Info Center\nOpen 24/7",
        90,
        "Arial",
        20,
        1,
        0xFFFFFFFF,
        0xFF003366,
        1
    );

    return 1;
}

stock RemoveAmenityBuildings(playerid)
{
    #pragma unused playerid
    return 1;
}

main() {}
