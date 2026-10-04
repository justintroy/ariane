// Development-only live acceptance probe, enabled by ARIANE_SAMP_VALIDATION=1.
// Included inside samp_editor.cpp's private namespace to inspect actual ownership.
Json validateRemovals()
{
    if(!active || !AgentBridgeSessionActive())
        throw std::runtime_error("removal validation requires an active SA-MP agent session");
    Json original=document.snapshot();
    Json checks=Json::object();
    auto expect=[&](const char *name,bool passed) {
        checks[name]=passed;
        if(!passed) throw std::runtime_error(std::string("removal validation failed: ")+name);
    };
    auto instance=[](int model,float x,float y,float z,bool lod=false) {
        ObjectInst result={};
        result.m_objectId=model;
        result.m_translation={x,y,z};
        result.m_isAdded=false;
        result.m_isBigBuilding=lod;
        return result;
    };
    auto clearRemovals=[&]() {
        Json next=document.data;
        next["removals"]=Json::array();
        document.request({{"op","replace"},{"document",next},{"label","Renderer removal validation"}});
        syncSampDocument();
    };
    auto addRemoval=[&](int model,float x,float y,float z,float radius) {
        document.request({{"op","removal"},{"removal",{{"model",model},{"group","renderer-validation"},
            {"position",{x,y,z}},{"radius",radius}}}});
        syncSampDocument();
    };
    auto removalCallCount=[](const std::string &code) {
        const std::string marker="RemoveBuildingForPlayer(playerid,";
        size_t count=0, at=0;
        while((at=code.find(marker,at))!=std::string::npos) { ++count; at+=marker.size(); }
        return count;
    };
    try {
        clearRemovals();
        addRemoval(19379,0,0,0,5);
        auto boundary=instance(19379,3,4,0);
        auto verticalInside=instance(19379,0,0,4.99f);
        auto verticalOutside=instance(19379,0,0,5.01f);
        auto wrongModel=instance(19380,0,0,0);
        expect("spherical_boundary_inclusive",SampHidden(&boundary));
        expect("vertical_inside_radius",SampHidden(&verticalInside));
        expect("vertical_separation_excludes",!SampHidden(&verticalOutside));
        expect("specific_model_filter",!SampHidden(&wrongModel));

        auto normal=instance(19400,0,0,0,false);
        auto lod=instance(19401,0,0,0,true);
        clearRemovals();
        addRemoval(19400,0,0,0,10);
        expect("specific_removal_targets_normal_only",SampHidden(&normal) && !SampHidden(&lod));
        expect("no_implicit_lod_removal_export",document.data["removals"].size()==1 &&
            removalCallCount(samp::ExportPawn(document.data))==1);
        addRemoval(-1,0,0,0,10);
        expect("wildcard_targets_lod_instance",SampHidden(&lod));

        clearRemovals();
        addRemoval(19400,0,0,0,3);
        addRemoval(19400,4,0,0,3);
        auto overlap=instance(19400,2,0,0);
        expect("overlapping_records_hide",SampHidden(&overlap));
        document.request({{"op","undo"}}); syncSampDocument();
        expect("undo_one_overlap_keeps_other_active",document.data["removals"].size()==1 && SampHidden(&overlap));
        document.request({{"op","undo"}}); syncSampDocument();
        expect("undo_last_overlap_restores_instance",document.data["removals"].empty() && !SampHidden(&overlap));
        document.request({{"op","redo"}}); syncSampDocument();
        expect("redo_restores_removal",document.data["removals"].size()==1 && SampHidden(&overlap));
        document.request({{"op","redo"}}); syncSampDocument();
        expect("redo_restores_overlap",document.data["removals"].size()==2 && SampHidden(&overlap));
    } catch(...) {
        document.restoreSnapshot(original); syncSampDocument();
        throw;
    }
    document.restoreSnapshot(original); syncSampDocument();
    return {{"checks",checks},{"passed",checks.size()}};
}

Json validateMetadataVisibility()
{
    if(!active || !AgentBridgeSessionActive())
        throw std::runtime_error("metadata visibility validation requires an active SA-MP agent session");
    if(document.data["objects"].size()<3)
        throw std::runtime_error("metadata visibility validation requires three owned object fixtures");

    Json original=document.snapshot();
    bool originalCaptureFiltersActive=captureFiltersActive;
    int originalCaptureWorld=captureWorldFilter;
    int originalCaptureInterior=captureInteriorFilter;
    std::vector<int> ids;
    for(const auto &object:document.data["objects"])
        ids.push_back(object["id"].get<int>());

    Json checks=Json::object();
    auto expect=[&](const char *name,bool passed) {
        checks[name]=passed;
        if(!passed) throw std::runtime_error(std::string("metadata visibility validation failed: ")+name);
    };
    auto restore=[&]() {
        SampClearCaptureFilters();
        document.restoreSnapshot(original);
        syncSampDocument();
        if(originalCaptureFiltersActive)
            SampSetCaptureFilters(originalCaptureWorld,originalCaptureInterior);
        else
            SampClearCaptureFilters();
    };

    try {
        Json updated=document.data;
        const int worlds[]={2,7,-1};
        const int interiors[]={3,4,-1};
        const int areas[]={51,52,53};
        const double draws[]={750.0,425.0,0.0};
        for(size_t n=0;n<3;++n) {
            for(auto &object:updated["objects"]) {
                if(object["id"]!=ids[n]) continue;
                object["world"]=worlds[n];
                object["interior"]=interiors[n];
                object["area"]=areas[n];
                object["draw"]=draws[n];
            }
        }
        document.request({{"op","replace"},{"document",updated},{"label","Renderer metadata visibility validation"}});
        syncSampDocument();

        ObjectInst *worldInteriorMatch=objects.at(ids[0]);
        ObjectInst *worldInteriorMismatch=objects.at(ids[1]);
        ObjectInst *unfiltered=objects.at(ids[2]);
        bool runtimeAreasUniversal=worldInteriorMatch->m_area==13 &&
            worldInteriorMismatch->m_area==13 && unfiltered->m_area==13;
        bool persistedMetadataIntact=true;
        for(size_t n=0;n<3;++n) {
            auto record=row(ids[n]);
            persistedMetadataIntact=persistedMetadataIntact && record &&
                (*record)["world"]==worlds[n] && (*record)["interior"]==interiors[n] &&
                (*record)["area"]==areas[n] && (*record)["draw"]==draws[n];
        }
        expect("owned_preview_area_is_13",runtimeAreasUniversal);
        expect("persisted_world_interior_and_streamer_area_are_preserved",persistedMetadataIntact);

        const float fallback=1234.5f;
        ObjectInst nonOwned={};
        nonOwned.m_objectId=worldInteriorMatch->m_objectId;
        expect("owned_draw_distance_750",SampDrawDistance(worldInteriorMatch,fallback)==750.0f);
        expect("owned_draw_distance_is_per_instance",SampDrawDistance(worldInteriorMismatch,fallback)==425.0f);
        expect("default_draw_distance_uses_native_fallback",SampDrawDistance(unfiltered,fallback)==fallback);
        expect("nonowned_draw_distance_uses_native_fallback",SampDrawDistance(&nonOwned,fallback)==fallback);

        document.request({{"op","preview"},{"world",2},{"interior",3}});
        syncSampDocument();
        Json persistedBeforeCapture=document.data;
        unsigned revisionBeforeCapture=document.revision;
        auto captureStateUnchanged=[&]() {
            return document.data==persistedBeforeCapture && document.revision==revisionBeforeCapture &&
                document.data["preview"]==Json({{"world",2},{"interior",3}});
        };

        expect("persisted_world_interior_preview_filters",!SampHidden(worldInteriorMatch) &&
            SampHidden(worldInteriorMismatch) && !SampHidden(unfiltered));

        SampSetCaptureFilters(7,4);
        expect("capture_filter_override_selects_matching_record",SampHidden(worldInteriorMatch) &&
            !SampHidden(worldInteriorMismatch) && !SampHidden(unfiltered) && captureStateUnchanged());

        SampSetCaptureFilters(7,3);
        expect("capture_filter_world_and_interior_axes",SampHidden(worldInteriorMatch) &&
            SampHidden(worldInteriorMismatch) && !SampHidden(unfiltered) && captureStateUnchanged());

        SampSetCaptureFilters(2,4);
        expect("capture_filter_interior_with_matching_world",SampHidden(worldInteriorMatch) &&
            SampHidden(worldInteriorMismatch) && !SampHidden(unfiltered) && captureStateUnchanged());

        SampClearCaptureFilters();
        expect("clearing_capture_filters_restores_persisted_preview",!SampHidden(worldInteriorMatch) &&
            SampHidden(worldInteriorMismatch) && !SampHidden(unfiltered) && captureStateUnchanged());
    } catch(...) {
        restore();
        throw;
    }
    restore();
    return {{"checks",checks},{"passed",checks.size()}};
}

Json validateClipboard(ObjectInst *fixture)
{
    if(!active || !AgentBridgeSessionActive() || !fixture || !SampOwns(fixture))
        throw std::runtime_error("clipboard validation requires an owned object in an active agent session");
    int fixtureId=identities.at(fixture);
    Json *fixtureRow=row(fixtureId);
    if(!fixtureRow) throw std::runtime_error("clipboard validation fixture row is unavailable");

    Json original=document.snapshot();
    auto originalCutRows=cutRows;
    auto originalCopiedRows=copiedRows;
    auto originalCheckedObjectIds=checkedObjectIds;
    std::vector<ObjectInst*> originalSelection;
    for(CPtrNode *node=selection.first;node;node=node->next)
        originalSelection.push_back((ObjectInst*)node->item);
    int originalSelected=selected;
    bool originalActive=active;
    std::string originalMessage=message;
    Json checks=Json::object();
    auto expect=[&](const char *name,bool passed) {
        checks[name]=passed;
        if(!passed) throw std::runtime_error(std::string("clipboard validation failed: ")+name);
    };
    auto restore=[&]() {
        document.restoreSnapshot(original);
        active=originalActive;
        syncSampDocument();
        cutRows=originalCutRows;
        copiedRows=originalCopiedRows;
        checkedObjectIds=originalCheckedObjectIds;
        selected=originalSelected;
        ClearSelection();
        for(auto instance:originalSelection) if(instance) instance->Select();
        message=originalMessage;
    };

    try {
        Json material={{"type","texture"},{"model",-1},{"txd","renderer-validation"},
            {"texture","clipboard-probe"},{"color",0xFF336699u}};
        document.request({{"op","material"},{"id",fixtureId},{"slot",0},{"material",material}});
        syncSampDocument();
        Json source=*row(fixtureId);
        ObjectInst vanilla={};
        std::vector<ObjectInst*> mixed={fixture,&vanilla};

        int copied=SampCopy(mixed);
        expect("copy_ignores_vanilla_pointer",copied==1 && copiedRows.size()==1 && copiedRows[0]==source);
        Json unchanged=document.snapshot();
        int ignoredCut=SampCut({&vanilla});
        expect("cut_ignores_vanilla_pointer",ignoredCut==0 && document.snapshot()==unchanged);

        copied=SampCopy(mixed);
        int duplicateId=document.data["next_id"].get<int>();
        int pasted=SampPaste({&vanilla},false,false);
        Json *duplicate=row(duplicateId);
        Json *currentFixture=row(fixtureId);
        expect("paste_copies_material_override",copied==1 && pasted==1 && duplicate &&
            duplicateId!=fixtureId && (*duplicate)["materials"]==source["materials"] &&
            currentFixture && (*currentFixture)["materials"]==source["materials"]);
        SampUndo(false);
        expect("undo_removes_pasted_copy",row(duplicateId)==nullptr && row(fixtureId) &&
            *row(fixtureId)==source);

        int cut=SampCut(mixed);
        expect("cut_ignores_vanilla_and_removes_document_record",cut==1 && !row(fixtureId) &&
            objects[fixtureId]==fixture && fixture->m_isDeleted && fixture->m_rwObject==nullptr);
        int restored=SampPaste({},false,true);
        expect("cut_paste_restores_stable_record",restored==1 && row(fixtureId) &&
            *row(fixtureId)==source && objects[fixtureId]==fixture && !fixture->m_isDeleted &&
            fixture->m_rwObject!=nullptr);
        SampUndo(false);
        expect("undo_cut_paste_removes_record",!row(fixtureId) && objects[fixtureId]==fixture &&
            fixture->m_isDeleted);
        SampUndo(true);
        expect("redo_cut_paste_restores_record",row(fixtureId) && *row(fixtureId)==source &&
            objects[fixtureId]==fixture && !fixture->m_isDeleted && fixture->m_rwObject!=nullptr);
    } catch(...) {
        restore();
        throw;
    }
    restore();
    return {{"scope","direct function paths; no UI shortcut delivery"},
        {"checks",checks},{"passed",checks.size()}};
}

#ifdef _WIN32
class RendererFailureScope
{
    bool hadValue;
    std::string previousValue;
public:
    RendererFailureScope() : hadValue(false)
    {
        DWORD needed=GetEnvironmentVariableA("ARIANE_SAMP_FAIL_RENDERER_ALLOC",nullptr,0);
        if(needed) {
            std::vector<char> value(needed);
            DWORD copied=GetEnvironmentVariableA("ARIANE_SAMP_FAIL_RENDERER_ALLOC",value.data(),(DWORD)value.size());
            if(copied) { hadValue=true; previousValue.assign(value.data(),copied); }
        }
    }
    void set(const char *point)
    {
        if(!SetEnvironmentVariableA("ARIANE_SAMP_FAIL_RENDERER_ALLOC",point))
            throw std::runtime_error("could not set renderer allocation failure probe");
    }
    ~RendererFailureScope()
    {
        SetEnvironmentVariableA("ARIANE_SAMP_FAIL_RENDERER_ALLOC",hadValue?previousValue.c_str():nullptr);
    }
};
#endif

Json validateRenderer()
{
    Json metadataVisibility=validateMetadataVisibility();
    Json removalValidation=validateRemovals();
    auto counts=[]() {
        return Json{{"clumps",rw::Clump::numAllocated},{"geometry",rw::Geometry::numAllocated},
            {"materials",rw::Material::numAllocated},
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
    ObjectInst *atomicFailureInstance=nullptr, *clumpFailureInstance=nullptr;
    for(auto &entry:objects) {
        auto inst=entry.second;
        if(!row(entry.first) || inst->m_isDeleted) continue;
        auto def=GetObjectDef(inst->m_objectId);
        if(!def)
            throw std::runtime_error("renderer fixture model definition is unavailable");
        def->Load();
        if(def->m_type==ObjectDef::CLUMP) {
            if(!clumpFailureInstance) clumpFailureInstance=inst;
            if(!def->m_clump)
                throw std::runtime_error("renderer fixture clump model did not load");
            if(!inst->m_rwObject) inst->CreateRwObject();
            if(!inst->m_rwObject || ((rw::Object*)inst->m_rwObject)->type!=rw::Clump::ID)
                throw std::runtime_error("renderer fixture instance is not a clump");
            auto source=def->m_clump;
            auto countAtomics=[](rw::Clump *clump) {
                int count=0;
                FORLIST(lnk, clump->atomics) count++;
                return count;
            };
            int sourceAtomics=countAtomics(source);
            std::vector<Json> sourceSignatures;
            FORLIST(lnk, source->atomics) {
                auto atomic=rw::Atomic::fromClump(lnk);
                if(!atomic || !atomic->geometry)
                    throw std::runtime_error("renderer fixture clump has an invalid atomic");
                sourceSignatures.push_back(signature(atomic->geometry));
            }
            auto before=counts();
            bool recreated=true, released=true, topology=true, isolated=true, restored=true;
            bool ownerStable=true, cacheInvalidated=true, cacheRestored=true;
            int pointerReuseCycles=0;
            Json resourceCheckpoints=Json::array();
            for(int cycle=0;cycle<50;++cycle) {
                void *previousObject=inst->m_rwObject;
                inst->DestroyRwObject();
                released=released && inst->m_rwObject==nullptr && inst->m_animState==nullptr;
                cacheInvalidated=cacheInvalidated && rendered.find(inst)==rendered.end();
                inst->CreateRwObject();
                if(!inst->m_rwObject || ((rw::Object*)inst->m_rwObject)->type!=rw::Clump::ID)
                    throw std::runtime_error("renderer fixture clump recreation failed");
                if(inst->m_rwObject==previousObject) pointerReuseCycles++;
                SampApplyMaterials(inst);
                auto currentRow=row(entry.first);
                std::string expectedCacheKey=currentRow ? (*currentRow)["materials"].dump()+
                    std::to_string((uintptr_t)inst->m_rwObject) : std::string();
                auto cache=rendered.find(inst);
                cacheRestored=cacheRestored && cache!=rendered.end() && cache->second==expectedCacheKey;
                auto clone=(rw::Clump*)inst->m_rwObject;
                recreated=recreated && clone!=source;
                topology=topology && countAtomics(clone)==sourceAtomics;
                auto sourceLink=source->atomics.link.next;
                size_t atomicIndex=0;
                FORLIST(lnk, clone->atomics) {
                    if(sourceLink==&source->atomics.link) {
                        topology=false; isolated=false; restored=false;
                        break;
                    }
                    auto sourceAtomic=rw::Atomic::fromClump(sourceLink);
                    auto cloneAtomic=rw::Atomic::fromClump(lnk);
                    if(sourceAtomic && cloneAtomic && sourceAtomic->geometry && cloneAtomic->geometry) {
                        isolated=isolated && sourceAtomic->geometry!=cloneAtomic->geometry;
                        restored=restored && atomicIndex<sourceSignatures.size() &&
                            signature(cloneAtomic->geometry)==sourceSignatures[atomicIndex];
                    } else {
                        isolated=false; restored=false;
                    }
                    if(sourceAtomic && cloneAtomic && sourceAtomic->geometry && cloneAtomic->geometry &&
                       sourceAtomic->geometry->matList.numMaterials==cloneAtomic->geometry->matList.numMaterials)
                        for(int slot=0;slot<sourceAtomic->geometry->matList.numMaterials;++slot)
                            isolated=isolated && sourceAtomic->geometry->matList.materials[slot]!=
                                cloneAtomic->geometry->matList.materials[slot];
                    else
                        isolated=false;
                    sourceLink=sourceLink->next;
                    atomicIndex++;
                }
                topology=topology && atomicIndex==sourceSignatures.size() && sourceLink==&source->atomics.link;
                ownerStable=ownerStable && objects[entry.first]==inst && !inst->m_isDeleted;
                if(cycle<3 || cycle==49)
                    resourceCheckpoints.push_back({{"cycle",cycle+1},{"counts",counts()}});
            }
            auto after=counts();
            // The pinned librw Frame::destroyHierarchy frees its tree without
            // decrementing Frame::numAllocated; report it, but balance the other counters.
            Json beforeNoFrames=before, afterNoFrames=after;
            beforeNoFrames.erase("frames");
            afterNoFrames.erase("frames");
            results.push_back({{"id",entry.first},{"model",inst->m_objectId},{"kind","clump"},
                {"cycles",50},{"cloned",recreated},{"released",released},{"topology",topology},
                {"isolated",isolated},{"restored",restored},{"owner_stable",ownerStable},
                {"cache_invalidated",cacheInvalidated},{"cache_restored",cacheRestored},
                {"pointer_reuse_cycles",pointerReuseCycles},
                {"balanced_except_frames",beforeNoFrames==afterNoFrames},
                {"before",before},{"after",after},{"resource_checkpoints",resourceCheckpoints}});
            continue;
        }
        if(def->m_type!=ObjectDef::ATOMIC)
            throw std::runtime_error("renderer fixture has an unsupported model type");
        if(!atomicFailureInstance) atomicFailureInstance=inst;
        if(!inst->m_rwObject) inst->CreateRwObject();
        if(!inst->m_rwObject) throw std::runtime_error("renderer fixture model did not load");
        SampApplyMaterials(inst);
        auto source=def->m_atomics[0]->geometry;
        auto original=signature(source);
        auto expected=signature(((rw::Atomic*)inst->m_rwObject)->geometry);
        auto before=counts();
        bool isolated=true, restored=true, cacheInvalidated=true, cacheRestored=true;
        int pointerReuseCycles=0;
        for(int cycle=0;cycle<50;++cycle) {
            void *previousObject=inst->m_rwObject;
            inst->DestroyRwObject();
            cacheInvalidated=cacheInvalidated && rendered.find(inst)==rendered.end();
            inst->CreateRwObject(); SampApplyMaterials(inst);
            if(inst->m_rwObject==previousObject) pointerReuseCycles++;
            auto currentRow=row(entry.first);
            std::string expectedCacheKey=currentRow ? (*currentRow)["materials"].dump()+
                std::to_string((uintptr_t)inst->m_rwObject) : std::string();
            auto cache=rendered.find(inst);
            cacheRestored=cacheRestored && cache!=rendered.end() && cache->second==expectedCacheKey;
            auto atomic=(rw::Atomic*)inst->m_rwObject;
            isolated=isolated && atomic->geometry!=source && signature(source)==original;
            restored=restored && signature(atomic->geometry)==expected;
            for(int slot=0;slot<atomic->geometry->matList.numMaterials;++slot)
                isolated=isolated && atomic->geometry->matList.materials[slot]!=source->matList.materials[slot];
        }
        auto after=counts();
        results.push_back({{"id",entry.first},{"model",inst->m_objectId},{"kind","atomic"},
            {"cycles",50},{"isolated",isolated},
            {"restored",restored},{"cache_invalidated",cacheInvalidated},{"cache_restored",cacheRestored},
            {"pointer_reuse_cycles",pointerReuseCycles},{"balanced",before==after},{"before",before},{"after",after}});
    }
    if(results.empty()) throw std::runtime_error("renderer validation needs a nonempty scratch document");
    Json clipboardValidation=validateClipboard(atomicFailureInstance);
    Json allocationFailures=Json::array();
#ifdef _WIN32
    auto countersBalanced=[](Json left,Json right,bool excludeFrameCounter) {
        if(excludeFrameCounter) { left.erase("frames"); right.erase("frames"); }
        return left==right;
    };
    auto injectFailure=[&](const char *stage,ObjectInst *inst,bool clump) {
        if(!inst || !inst->m_rwObject)
            throw std::runtime_error(std::string("renderer failure fixture is unavailable: ")+stage);
        Json before=counts();
        inst->DestroyRwObject();
        Json destroyed=counts();
        void *failed=nullptr;
        {
            RendererFailureScope failure;
            failure.set(stage);
            failed=inst->CreateRwObject();
        }
        Json afterFailure=counts();
        bool nullResult=failed==nullptr && inst->m_rwObject==nullptr;
        bool cleaned=countersBalanced(destroyed,afterFailure,clump);
        if(inst->m_rwObject) inst->DestroyRwObject();
        void *recovered=inst->CreateRwObject();
        if(recovered) SampApplyMaterials(inst);
        Json afterRecovery=counts();
        bool recovery= recovered!=nullptr && countersBalanced(before,afterRecovery,clump);
        if(!nullResult || !cleaned || !recovery)
            throw std::runtime_error(std::string("renderer failure validation failed: ")+stage);
        allocationFailures.push_back({{"stage",stage},{"injection","caller_null_return"},{"null_result",nullResult},
            {"cleanup_balanced_except_frames",cleaned},{"recovered",recovery},
            {"frame_counter_excluded",clump},{"before",before},
            {"destroyed",destroyed},{"after_failure",afterFailure},{"after_recovery",afterRecovery}});
    };
    if(!atomicFailureInstance || !clumpFailureInstance)
        throw std::runtime_error("allocation failure validation requires atomic and clump fixtures");
    injectFailure("atomic_clone",atomicFailureInstance,false);
    injectFailure("atomic_frame",atomicFailureInstance,false);
    injectFailure("clump_clone",clumpFailureInstance,true);
    injectFailure("clump_root",clumpFailureInstance,true);
#else
    allocationFailures.push_back({{"status","unavailable"},{"reason","the failure hooks are Windows-only"}});
#endif
    // Exercise document-driven replacement/deletion without accumulating runtime
    // instances. Preload both models before taking the resource baseline.
    auto entry=objects.begin();
    while(entry!=objects.end()) {
        if(row(entry->first) && !entry->second->m_isDeleted) {
            auto def=GetObjectDef(entry->second->m_objectId);
            if(def && def->m_type==ObjectDef::ATOMIC) break;
        }
        ++entry;
    }
    if(entry==objects.end())
        throw std::runtime_error("renderer lifecycle fixture requires an atomic model");
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
            document.request({{"op","update"},{"id",id},{"changes",{{"model",replacement}}}}); syncSampDocument();
            stable=stable && objects[id]==inst;
            document.request({{"op","undo"}}); syncSampDocument();
            document.request({{"op","delete"},{"id",id}}); syncSampDocument();
            released=released && inst->m_rwObject==nullptr && inst->m_animState==nullptr && inst->m_isDeleted;
            document.request({{"op","undo"}}); syncSampDocument();
            stable=stable && objects[id]==inst && inst->m_rwObject!=nullptr && !inst->m_isDeleted;
        }
    } catch(...) {
        document.restoreSnapshot(snapshot); syncSampDocument(); throw;
    }
    document.restoreSnapshot(snapshot); syncSampDocument();
    auto after=counts();
    return {{"objects",results},{"lifecycle",{{"cycles",50},{"stable_instances",stable && identities.size()==instanceCount},
        {"deleted_resources_released",released},{"balanced",before==after},{"before",before},{"after",after}}},
        {"metadata_visibility",metadataVisibility},{"removals",removalValidation},{"clipboard",clipboardValidation},
        {"allocation_failures",allocationFailures},
        {"device_reset",{{"status","not_exercised"},
            {"reason","requires a real client-area resize or D3D9 device-loss recovery; no safe validation API triggers either"}}},
        {"librw_internal_clone_failure",{{"status","not_exercised"},
            {"reason","pinned librw Clump::clone dereferences internal allocations before Ariane can check them"}}}};
}
