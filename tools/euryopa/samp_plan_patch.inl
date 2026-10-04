// Atomic symbolic plans use the same document commit and history contract.
Json Document::applyPatch(const Json &r)
{
    const auto &operations=r.at("operations");
    if(!operations.is_array() || operations.size()>4096) throw std::runtime_error("patch operations must be an array of at most 4096 entries");
    Document staged; staged.data=data;
    Json references=Json::object();
    if(r.contains("groups")) {
        const auto &groups=r.at("groups");
        if(!groups.is_array() || groups.size()>4096) throw std::runtime_error("invalid patch groups");
        for(const auto &group:groups) {
            if(!group.is_string() || group.get<std::string>().empty()) throw std::runtime_error("invalid patch group");
            if(std::find(staged.data["groups"].begin(),staged.data["groups"].end(),group)==staged.data["groups"].end()) staged.data["groups"].push_back(group);
        }
        ValidateDocument(staged.data);
    }
    for(auto operation:operations) {
        std::string action=operation.at("op");
        if(action!="place" && action!="update" && action!="delete" && action!="material" && action!="duplicate" && action!="removal" && action!="import" && action!="preview") throw std::runtime_error("unsupported patch operation");
        std::string key;
        if(operation.contains("key")) {
            if(!operation.at("key").is_string()) throw std::runtime_error("patch key must be a string");
            key=operation.at("key").get<std::string>();
            if(key.empty() || key.size()>128 || references.contains(key)) throw std::runtime_error("invalid or duplicate patch key");
            if(action!="place" && action!="duplicate" && action!="removal") throw std::runtime_error("patch key requires one created record");
            operation.erase("key");
        }
        if(operation.contains("id") && operation.at("id").is_object()) {
            const auto &reference=operation.at("id");
            if(reference.size()!=1 || !reference.contains("ref") || !reference.at("ref").is_string()) throw std::runtime_error("invalid patch reference");
            const auto name=reference.at("ref").get<std::string>();
            if(!references.contains(name)) throw std::runtime_error("unknown or forward patch reference: "+name);
            operation["id"]=references.at(name);
        }
        const int createdId=staged.data.at("next_id").get<int>();
        staged.request(operation);
        if(!key.empty()) references[key]=createdId;
    }
    commit(staged.data,r.value("label",std::string("SA-MP patch")));
    return {{"revision",revision},{"document",data},{"references",references}};
}
