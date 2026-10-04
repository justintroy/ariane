#pragma once
#include "vendor/json.hpp"
#include <string>
#include <vector>

namespace samp {
using Json = nlohmann::json;
struct ImportResult { Json objects = Json::array(), removals = Json::array(), diagnostics = Json::array(); };
ImportResult ParsePawn(const std::string &source, const std::string &file);
std::string ExportPawn(const Json &document, const std::string &group = "");
Json EmptyDocument();
void ValidateDocument(const Json &document);
class Document {
public:
    Json data = EmptyDocument();
    Json history = Json::array();
    unsigned revision = 0;
    Json request(const Json &request);
    void restore(const Json &snapshot);
    Json snapshot() const;
    void restoreSnapshot(const Json &snapshot);
private:
    std::vector<Json> undo_, redo_;
    void commit(const Json &next, const std::string &label);
};
}
