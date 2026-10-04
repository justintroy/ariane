#include "samp_document.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <functional>

using samp::Json;
static int checks = 0;
static void check(bool value)
{
    ++checks;
    if (!value) throw std::runtime_error("check failed: " + std::to_string(checks));
}
static void rejected(const std::function<void()> &action)
{
    bool failed = false;
    try { action(); } catch (const std::exception &) { failed = true; }
    check(failed);
}
static Json importRequest(const std::string &source, const std::string &name = "fixture.pwn")
{
    return {{"op", "import"}, {"files", {{{"name", name}, {"source", source}}}}};
}
#include "plan_patch_test.inl"
static int runCoreTests(int argc, char **argv)
{
    testPlanPatch();
    samp::Document document;
    const std::string source = R"(
#define BASE 19000
const INDEX = 1 + 2;
new objects[4];
objects[INDEX] = CreateDynamicObject(BASE + 379, 1e-2, -2.5, 3, 10, 20, 30,
    7, 2, -1, 400.5, 200, -1, 9);
new temporary = CreateObject(19379, 4, 5, 6, 0, 0, 0, 125);
SetDynamicObjectMaterial(objects[3], 0, 19379, "all_walls", "wall//name", 0xAA112233);
SetObjectMaterialText(temporary, "hello\n\"world\"\\path, () // text", 1);
new alias = temporary;
SetObjectMaterial(alias, 2, -1, "none", "none", -1);
temporary = CreateDynamicObject(19379, 7, 8, 9, 0, 0, 0);
SetDynamicObjectMaterialText(temporary, 0, "{FF0000}New", OBJECT_MATERIAL_SIZE_256x128);
RemoveBuildingForPlayer(playerid, -1, 0, 0, 0, 50);
)";
    auto parsed = samp::ParsePawn(source, "fixture.pwn");
    check(parsed.diagnostics.empty());
    check(parsed.objects.size() == 3 && parsed.removals.size() == 1);
    check(parsed.objects[0]["materials"]["0"]["type"] == "texture");
    check(parsed.objects[0]["world"] == 7 && parsed.objects[0]["priority"] == 9);
    check(parsed.objects[1]["materials"]["1"]["type"] == "text");
    check(parsed.objects[1]["materials"]["2"]["color"] == 4294967295u);
    check(parsed.objects[1]["draw"] == 125);
    check(parsed.objects[2]["materials"]["0"]["text"] == "{FF0000}New");
    check(parsed.objects[2]["stream"] == 300 && parsed.objects[2]["draw"] == 0);
    check(parsed.objects[1]["materials"]["1"]["font"] == "Arial");
    check(parsed.objects[1]["materials"]["1"]["foreground"] == 4294967295u);
    check(parsed.objects[2]["materials"]["0"]["size"] == 90);
    document.request(importRequest(source));
    check(document.history.back()["affected"].size() == 4);
    check(document.history.back()["affected"][0]["before"].is_null());
    const auto original = document.data;
    const auto exported = samp::ExportPawn(document.data);
    auto again = samp::ParsePawn(exported, "fixture.pwn");
    check(again.diagnostics.empty());
    check(again.objects == parsed.objects);
    check(again.removals == parsed.removals);
    check(samp::ExportPawn(document.data) == exported);
    document.request({{"op", "delete"}, {"id", 1}});
    check(document.history.back()["affected"].size() == 1);
    check(document.history.back()["affected"][0]["id"] == 1 && document.history.back()["affected"][0]["after"].is_null());
    check(document.data["objects"].size() == 2);
    document.request({{"op", "undo"}});
    check(document.data == original);
    document.request({{"op", "redo"}});
    check(document.data["objects"].size() == 2);
    document.request({{"op", "undo"}});
    const auto snapshot = document.snapshot();
    const auto inspectedSnapshot = document.request({{"op", "inspect"}, {"include_snapshot", true}});
    check(inspectedSnapshot["snapshot"] == snapshot);
    document.request({{"op", "duplicate"}, {"id", 1}});
    check(document.data["objects"].size() == 4);
    check(document.data["objects"].back()["materials"] == parsed.objects[0]["materials"]);
    document.restoreSnapshot(snapshot);
    check(document.data == original);
    document.request({{"op", "duplicate"}, {"id", 1}});
    document.request({{"op", "replace"}, {"snapshot", snapshot}});
    check(document.snapshot() == snapshot && document.data == original);
    auto badSnapshot = snapshot;
    badSnapshot["undo"] = Json::array({{{"version", 99}}});
    const auto preRestore = document.snapshot();
    const auto preRestoreRevision = document.revision;
    rejected([&] { document.restoreSnapshot(badSnapshot); });
    check(document.snapshot() == preRestore && document.revision == preRestoreRevision);
    badSnapshot = snapshot;
    badSnapshot["history"] = Json::array({"invalid"});
    rejected([&] { document.restoreSnapshot(badSnapshot); });
    check(document.snapshot() == preRestore && document.revision == preRestoreRevision);
    samp::Document removalOnly;
    removalOnly.request({{"op", "removal"}, {"removal", {{"model", -1}, {"position", {0, 0, 0}}, {"radius", 25}}}});
    check(removalOnly.data["groups"] == Json::array({"untitled"}));
    check(samp::ParsePawn(samp::ExportPawn(removalOnly.data), "untitled").removals.size() == 1);
    samp::Document exhausted;
    auto fullIds = samp::EmptyDocument();
    fullIds["next_id"] = 2147483647;
    exhausted.request({{"op", "replace"}, {"document", fullIds}});
    const auto fullSnapshot = exhausted.snapshot();
    const auto fullRevision = exhausted.revision;
    rejected([&] { exhausted.request({{"op", "place"}, {"object", {{"model", 19379}}}}); });
    rejected([&] { exhausted.request(importRequest("CreateObject(19379,0,0,0,0,0,0);")); });
    rejected([&] { exhausted.request({{"op", "removal"}, {"removal", {{"model", -1}, {"position", {0, 0, 0}}, {"radius", 1}}}}); });
    check(exhausted.snapshot() == fullSnapshot && exhausted.revision == fullRevision);
    samp::Document multiFile;
    Json files = Json::array({
        {{"name", "north.pwn"}, {"source", "new shared=CreateObject(19379,1,2,3,0,0,0); SetObjectMaterial(shared,0,19379,\"walls\",\"brick\");"}},
        {{"name", "south.pwn"}, {"source", "new shared=CreateDynamicObject(19379,4,5,6,0,0,0); SetDynamicObjectMaterialText(shared,1,\"South\");"}}
    });
    auto preview = multiFile.request({{"op", "preview_import"}, {"files", files}});
    check(multiFile.data["objects"].empty() && preview["diagnostics"].empty());
    multiFile.request({{"op", "import"}, {"files", files}});
    check(multiFile.data["objects"].size() == 2 && multiFile.data["objects"][0]["group"] == "north.pwn");
    check(multiFile.data["objects"][1]["materials"]["1"]["text"] == "South");
    check(samp::ExportPawn(multiFile.data).find("// Source: south.pwn") != std::string::npos);
    const auto beforeBadImport = multiFile.snapshot();
    const auto beforeBadRevision = multiFile.revision;
    Json badFiles = Json::array({{{"name", "bad.pwn"}, {"source", "SetObjectMaterial(shared,0,19379,\"walls\",\"brick\");"}}});
    preview = multiFile.request({{"op", "preview_import"}, {"files", badFiles}});
    check(preview["diagnostics"][0]["file"] == "bad.pwn" && preview["diagnostics"][0]["line"] == 1);
    rejected([&] { multiFile.request({{"op", "import"}, {"files", badFiles}}); });
    check(multiFile.snapshot() == beforeBadImport && multiFile.revision == beforeBadRevision);
    samp::Document bounded;
    bounded.request(importRequest("CreateObject(19379,0,0,0,0,0,0);"));
    for(int n=0;n<70;++n)
        bounded.request({{"op", "update"}, {"id", 1}, {"changes", {{"position", {n, 0, 0}}}}});
    check(bounded.history.size() == 64);
    samp::Document retried;
    const auto retryRevision = retried.revision;
    Json retryRequest = {{"op", "place"}, {"object", {{"model", 19379}}}, {"expected_revision", retryRevision}};
    retried.request(retryRequest);
    rejected([&] { retried.request(retryRequest); });
    check(retried.data["objects"].size() == 1);
    samp::Document atomicPatch;
    const auto patchRevision = atomicPatch.revision;
    atomicPatch.request({{"op", "patch"}, {"expected_revision", patchRevision}, {"operations", Json::array({
        {{"op", "place"}, {"object", {{"model", 19379}}}},
        {{"op", "removal"}, {"removal", {{"model", -1}, {"position", {1, 2, 3}}, {"radius", 10}}}}
    })}});
    check(atomicPatch.revision == patchRevision + 1 && atomicPatch.history.back()["affected"].size() == 2);
    check(atomicPatch.data["objects"].size() == 1 && atomicPatch.data["removals"].size() == 1);
    rejected([&] { atomicPatch.request({{"op", "patch"}, {"expected_revision", patchRevision}, {"operations", Json::array()}}); });
    atomicPatch.request({{"op", "undo"}});
    check(atomicPatch.data["objects"].empty() && atomicPatch.data["removals"].empty());
    for(int n=0;n<64;++n) bounded.request({{"op", "undo"}});
    rejected([&] { bounded.request({{"op", "undo"}}); });
    check(bounded.history.size() == 64);
    for(int n=0;n<64;++n) bounded.request({{"op", "redo"}});
    rejected([&] { bounded.request({{"op", "redo"}}); });
    check(bounded.history.size() == 64);
    const auto revision = document.revision;
    rejected([&] { document.request({{"op", "delete"}, {"id", 1}, {"expected_revision", revision - 1}}); });
    check(document.data == original);
    rejected([&] { document.request({{"op", "patch"}, {"operations", {
        {{"op", "delete"}, {"id", 1}}, {{"op", "delete"}, {"id", 99999}}
    }}}); });
    check(document.data == original && document.revision == revision);
    rejected([&] { document.request({{"op", "material"}, {"id", 1}, {"slot", 16},
        {"material", parsed.objects[0]["materials"]["0"]}}); });
    check(document.data == original);
    rejected([&] { document.request({{"op", "update"}, {"id", 1}, {"changes", {{"id", 99}}}}); });
    check(document.data == original);
    auto invalid = original;
    invalid["next_id"] = 1;
    rejected([&] { document.request({{"op", "replace"}, {"document", invalid}}); });
    invalid = original;
    invalid["objects"][0]["materials"]["0"] = {{"type", "text"}};
    rejected([&] { document.request({{"op", "replace"}, {"document", invalid}}); });
    invalid = original;
    invalid["asset_paths"] = Json::array({42});
    rejected([&] { document.request({{"op", "replace"}, {"document", invalid}}); });
    check(document.data == original);
    check(!samp::ParsePawn("SetDynamicObjectMaterialText(missing, 0, \"x\");", "bad.pwn").diagnostics.empty());
    check(!samp::ParsePawn("if(playerid) { CreateObject(1,0,0,0,0,0,0); }", "bad.pwn").diagnostics.empty());
    check(samp::ParsePawn("#if FLAG\nCreateObject(1,0,0,0,0,0,0);\n#endif", "bad.pwn").objects.empty());
    check(!samp::ParsePawn("CreateObject(1,0,0,0,0,0,0", "bad.pwn").diagnostics.empty());
    check(!samp::ParsePawn("/* unclosed", "bad.pwn").diagnostics.empty());
    check(!samp::ParsePawn("new a=CreateObject(1,0,0,0,0,0,0); a=55; SetObjectMaterialText(a,\"wrong\");", "bad.pwn").diagnostics.empty());
    check(samp::ParsePawn("new a=CreateObject(1,0,0,0,0,0,0); SetObjectMaterialText(a,\"default\");", "good.pwn").diagnostics.empty());
    document.request(importRequest("new temporary=CreateObject(19379,0,0,0,0,0,0); SetObjectMaterialText(temporary,\"Other file\");", "other.pwn"));
    check(document.data["objects"].back()["materials"]["0"]["text"] == "Other file");
    check(samp::ParsePawn(samp::ExportPawn(document.data, "other.pwn"), "other.pwn").objects.size() == 1);
    const auto firstGroup = samp::ExportPawn(document.data, "fixture.pwn");
    const auto secondGroup = samp::ExportPawn(document.data, "other.pwn");
    check(firstGroup.find("stock SAMP_CreateMap_G1()") != std::string::npos);
    check(secondGroup.find("stock SAMP_CreateMap_G2()") != std::string::npos);
    check(firstGroup.find("stock SAMP_RemoveBuildings_G1(playerid)") != std::string::npos);
    check(exported.find("// Source: fixture.pwn") != std::string::npos);
    check(samp::ExportPawn(document.data).find("// Source: other.pwn") != std::string::npos);
    rejected([&] { samp::ExportPawn(document.data, "missing.pwn"); });
    for (const std::string &bad : {
        "new a=CreateObject(1,0,0,0,0,0,0); SetObjectMaterial(a,0,1,\"x\",\"y\",4294967296);",
        "new a=CreateObject(1,0,0,0,0,0,0); SetObjectMaterial(a,1e99,1,\"x\",\"y\");",
        "new a=CreateObject(1,0,0,0,0,0,0); SetObjectMaterialText(a,\"x\",0,90,\"Arial\",24,1,1e99);",
        "#define BIG 4294967296\nCreateObject(BIG & 1,0,0,0,0,0,0);",
        "#define SHIFT 32\nCreateObject(1 << SHIFT,0,0,0,0,0,0);",
        "new a[2]; a[4294967296]=CreateObject(1,0,0,0,0,0,0);"
    }) check(!samp::ParsePawn(bad, "bad.pwn").diagnostics.empty());
    check(samp::ParsePawn("CreateObject(0xFFFFFFFF & 1,0,0,0,0,0,0);", "good.pwn").diagnostics.empty());
    auto malformed = samp::ParsePawn("\n\nCreateObject(1,0,0,0,0,0,0)", "bad.pwn");
    check(malformed.objects.empty() && malformed.diagnostics[0]["line"] == 3);
    malformed = samp::ParsePawn("\n\n/* unterminated", "bad.pwn");
    check(malformed.objects.empty() && malformed.diagnostics[0]["line"] == 3);
    malformed = samp::ParsePawn("stock Broken() { CreateObject(1,0,0,0,0,0,0);", "bad.pwn");
    check(!malformed.diagnostics.empty());
    malformed = samp::ParsePawn("CreateObject(1,0,0,0,0,0,0); }", "bad.pwn");
    check(!malformed.diagnostics.empty());
    const auto scoped = samp::ParsePawn(
        "stock First() { new object=CreateObject(1,0,0,0,0,0,0); }\n"
        "stock Second() { SetObjectMaterialText(object,\"stale\"); }", "scope.pwn");
    check(!scoped.diagnostics.empty());
    check(scoped.objects.size() == 1 && scoped.objects[0]["materials"].empty());
    const auto scopedConstant = samp::ParsePawn(
        "stock First() { const MODEL = 19379; CreateObject(MODEL,0,0,0,0,0,0); }\n"
        "stock Second() { CreateObject(MODEL,0,0,0,0,0,0); }", "scope.pwn");
    check(scopedConstant.objects.size() == 1 && !scopedConstant.diagnostics.empty());
    if (argc > 1)
    {
        const std::string path = argv[1];
        auto complete = document.data;
        complete["asset_paths"] = Json::array({"C:/local/models", "C:/local/txd"});
        complete["preview"] = {{"world", 7}, {"interior", 2}};
        document.request({{"op", "replace"}, {"document", complete}});
        document.request({{"op", "save"}, {"path", path + ".samp.json"}});
        samp::Document reopened;
        reopened.request({{"op", "open"}, {"path", path + ".samp.json"}});
        check(reopened.data == document.data);
        check(reopened.data["asset_paths"].size() == 2 && reopened.data["preview"]["world"] == 7);
        check(samp::ExportPawn(reopened.data, "fixture.pwn") == firstGroup);
        check(samp::ExportPawn(reopened.data, "other.pwn") == secondGroup);
        reopened.request({{"op", "export"}, {"path", path + ".pwn"}});
        rejected([&] { reopened.request({{"op", "save"}, {"path", path + ".pwn"}}); });
        rejected([&] { reopened.request({{"op", "export"}, {"path", path + ".ipl"}}); });
        const auto beforeInvalidOpen = reopened.snapshot();
        const auto beforeInvalidRevision = reopened.revision;
        auto invalidVersion = reopened.data;
        invalidVersion["version"] = 999;
        std::ofstream invalidFile(path + "-invalid.samp.json");
        invalidFile << invalidVersion.dump();
        invalidFile.close();
        rejected([&] { reopened.request({{"op", "open"}, {"path", path + "-invalid.samp.json"}}); });
        check(reopened.snapshot() == beforeInvalidOpen && reopened.revision == beforeInvalidRevision);
        std::ofstream compiler(path + "-compile.pwn");
        compiler << "#include <a_samp>\n#include <streamer>\n" << samp::ExportPawn(document.data);
        compiler << "\nmain() {}\npublic OnGameModeInit() { return SAMP_CreateMap(); }\npublic OnPlayerConnect(playerid) { return SAMP_RemoveBuildings(playerid); }\n";
        std::ofstream groupCompiler(path + "-groups-compile.pwn");
        groupCompiler << "#include <a_samp>\n#include <streamer>\n" << firstGroup << secondGroup;
        groupCompiler << "\nmain() {}\npublic OnGameModeInit() { SAMP_CreateMap_G1(); SAMP_CreateMap_G2(); return 1; }\n";
        groupCompiler << "public OnPlayerConnect(playerid) { SAMP_RemoveBuildings_G1(playerid); SAMP_RemoveBuildings_G2(playerid); return 1; }\n";
    }
    std::cout << checks << " SA-MP core checks passed\n";
    return 0;
}
int main(int argc, char **argv)
{
    try
    {
        return runCoreTests(argc, argv);
    }
    catch (const std::exception &error)
    {
        std::cerr << "SA-MP core test failed: " << error.what() << '\n';
        return 1;
    }
}
