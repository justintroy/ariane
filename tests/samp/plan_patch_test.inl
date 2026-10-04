// Symbolic plan operations share the document transaction and its undo boundary.
static void testPlanPatch()
{
    samp::Document document;
    const auto before = document.snapshot();
    Json plan = {{"op", "patch"}, {"expected_revision", 0}, {"groups", {"interior"}},
        {"operations", {
            {{"op", "place"}, {"key", "wall"}, {"object", {{"model", 19379}, {"group", "interior"}, {"world", 17}, {"interior", 3}}}},
            {{"op", "material"}, {"id", {{"ref", "wall"}}}, {"slot", 0},
                {"material", {{"type", "text"}, {"text", "Entrance"}, {"size", 90}, {"font", "Arial"}, {"font_size", 24},
                    {"bold", true}, {"foreground", 4294967295u}, {"background", 0}, {"align", 1}}}},
            {{"op", "duplicate"}, {"id", {{"ref", "wall"}}}, {"key", "copy"}},
            {{"op", "update"}, {"id", {{"ref", "copy"}}}, {"changes", {{"position", {5, 0, 0}}}}},
            {{"op", "removal"}, {"key", "building"}, {"removal", {{"model", -1}, {"position", {0, 0, 0}}, {"radius", 5}, {"group", "interior"}}}}
        }}};
    const auto result = document.request(plan);
    check(result.at("references") == Json({{"wall", 1}, {"copy", 2}, {"building", 3}}));
    check(document.revision == 1 && document.history.size() == 1);
    check(document.data["objects"][1]["world"] == 17 && document.data["objects"][1]["interior"] == 3);
    check(document.data["objects"][0]["materials"] == document.data["objects"][1]["materials"]);
    const auto installed = document.data;
    rejected([&] { document.request(plan); });
    check(document.data == installed && document.revision == 1);
    document.request({{"op", "undo"}});
    check(document.data == before.at("data"));
    document.request({{"op", "redo"}});
    check(document.data == installed);
    document.request({{"op", "material"}, {"id", 2}, {"slot", 0}, {"material", nullptr}});
    check(!document.data["objects"][0]["materials"].empty() && document.data["objects"][1]["materials"].empty());

    for (const auto &operations : std::vector<Json>{
        Json::array({{{"op", "place"}, {"key", "same"}, {"object", {{"model", 19379}}}},
                     {{"op", "place"}, {"key", "same"}, {"object", {{"model", 19379}}}}}),
        Json::array({{{"op", "delete"}, {"id", {{"ref", "later"}}}},
                     {{"op", "place"}, {"key", "later"}, {"object", {{"model", 19379}}}}}),
        Json::array({{{"op", "place"}, {"key", "new"}, {"object", {{"model", 19379}}}},
                     {{"op", "material"}, {"id", {{"ref", "new"}}}, {"slot", 16}, {"material", {{"type", "text"}}}}}),
        Json::array({{{"op", "place"}, {"key", ""}, {"object", {{"model", 19379}}}}}),
        Json::array({{{"op", "delete"}, {"key", "bad"}, {"id", 1}}}),
        Json::array({{{"op", "delete"}, {"id", {{"ref", "wall"}, {"extra", 1}}}}}),
        Json::array({{{"op", "material"}, {"kind", "removals"}, {"id", 3}, {"slot", 0},
                     {"material", {{"type", "texture"}, {"model", -1}, {"txd", "none"}, {"texture", "none"}, {"color", 4294967295u}}}}})
    })
    {
        const auto snapshot = document.snapshot();
        const auto revision = document.revision;
        rejected([&] { document.request({{"op", "patch"}, {"groups", {"new-group"}}, {"operations", operations}}); });
        check(document.snapshot() == snapshot && document.revision == revision);
    }
    const auto snapshot = document.snapshot();
    rejected([&] { document.request({{"op", "patch"}, {"operations", Json::object()}}); });
    rejected([&] { document.request({{"op", "patch"}, {"operations", Json::array()}, {"groups", {42}}}); });
    Json excessive = Json::array();
    for (int i = 0; i < 4097; ++i) excessive.push_back({{"op", "preview"}, {"world", -1}});
    rejected([&] { document.request({{"op", "patch"}, {"operations", excessive}}); });
    check(document.snapshot() == snapshot);
}
