// Development-only live acceptance probe, enabled by ARIANE_SAMP_VALIDATION=1.
// Included inside samp_editor.cpp's private namespace to inspect actual ownership.
Json validateRenderer()
{
    auto counts=[]() {
        return Json{{"geometry",rw::Geometry::numAllocated},{"materials",rw::Material::numAllocated},
            {"textures",rw::Texture::numAllocated},{"rasters",rw::Raster::numAllocated},
            {"atomics",rw::Atomic::numAllocated},{"frames",rw::Frame::numAllocated}};
    };
    auto signature=[](rw::Geometry *geometry) {
        Json result=Json::array();
        for(int n=0;n<geometry->matList.numMaterials;++n) {
            auto m=geometry->matList.materials[n];
            result.push_back({{"texture",(uintptr_t)m->texture},
                {"color",{m->color.red,m->color.green,m->color.blue,m->color.alpha}}});
        }
        return result;
    };
    Json results=Json::array();
    for(auto &entry:objects) {
        auto inst=entry.second;
        if(!row(entry.first) || inst->m_isDeleted) continue;
        auto def=GetObjectDef(inst->m_objectId);
        if(!def || def->m_type!=ObjectDef::ATOMIC)
            throw std::runtime_error("renderer fixture requires available atomic models");
        def->Load();
        if(!inst->m_rwObject) inst->CreateRwObject();
        if(!inst->m_rwObject) throw std::runtime_error("renderer fixture model did not load");
        SampApplyMaterials(inst);
        auto source=def->m_atomics[0]->geometry;
        auto original=signature(source);
        auto expected=signature(((rw::Atomic*)inst->m_rwObject)->geometry);
        auto before=counts();
        bool isolated=true, restored=true;
        for(int cycle=0;cycle<50;++cycle) {
            inst->DestroyRwObject();
            inst->CreateRwObject(); SampApplyMaterials(inst);
            auto atomic=(rw::Atomic*)inst->m_rwObject;
            isolated=isolated && atomic->geometry!=source && signature(source)==original;
            restored=restored && signature(atomic->geometry)==expected;
            for(int slot=0;slot<atomic->geometry->matList.numMaterials;++slot)
                isolated=isolated && atomic->geometry->matList.materials[slot]!=source->matList.materials[slot];
        }
        auto after=counts();
        results.push_back({{"id",entry.first},{"cycles",50},{"isolated",isolated},
            {"restored",restored},{"balanced",before==after},{"before",before},{"after",after}});
    }
    if(results.empty()) throw std::runtime_error("renderer validation needs a nonempty scratch document");
    // Exercise document-driven replacement/deletion without accumulating runtime
    // instances. Preload both models before taking the resource baseline.
    auto entry=objects.begin();
    while(entry!=objects.end() && (!row(entry->first) || entry->second->m_isDeleted)) ++entry;
    int id=entry->first;
    auto inst=entry->second;
    int originalModel=(*row(id))["model"];
    int replacement=originalModel==19379?19479:19379;
    auto replacementDef=GetObjectDef(replacement);
    if(!replacementDef || replacementDef->m_type!=ObjectDef::ATOMIC)
        throw std::runtime_error("lifecycle fixture replacement model is unavailable");
    replacementDef->Load();
    auto snapshot=document.snapshot();
    auto before=counts();
    size_t instanceCount=identities.size();
    bool stable=true, released=true;
    try {
        for(int cycle=0;cycle<50;++cycle) {
            document.request({{"op","update"},{"id",id},{"changes",{{"model",replacement}}}}); sync();
            stable=stable && objects[id]==inst;
            document.request({{"op","undo"}}); sync();
            document.request({{"op","delete"},{"id",id}}); sync();
            released=released && inst->m_rwObject==nullptr && inst->m_animState==nullptr && inst->m_isDeleted;
            document.request({{"op","undo"}}); sync();
            stable=stable && objects[id]==inst && inst->m_rwObject!=nullptr && !inst->m_isDeleted;
        }
    } catch(...) {
        document.restoreSnapshot(snapshot); sync(); throw;
    }
    document.restoreSnapshot(snapshot); sync();
    auto after=counts();
    return {{"objects",results},{"lifecycle",{{"cycles",50},{"stable_instances",stable && identities.size()==instanceCount},
        {"deleted_resources_released",released},{"balanced",before==after},{"before",before},{"after",after}}}};
}
