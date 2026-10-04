Json vectorJson(const rw::V3d &value) {
    return Json::array({value.x, value.y, value.z});
}

Json boundsJson(const rw::V3d &minimum, const rw::V3d &maximum, const char *source) {
    return {{"available", true}, {"min", vectorJson(minimum)}, {"max", vectorJson(maximum)},
        {"size", Json::array({maximum.x-minimum.x, maximum.y-minimum.y, maximum.z-minimum.z})},
        {"source", source}};
}

void accumulateBounds(const rw::V3d &point, rw::V3d &minimum, rw::V3d &maximum, bool &hasBounds) {
    if(!hasBounds) {
        minimum = maximum = point;
        hasBounds = true;
        return;
    }
    minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y); minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y); maximum.z = std::max(maximum.z, point.z);
}

std::string textureDictionaryName(const rw::TexDictionary *dictionary, const ObjectDef *object) {
    if(dictionary) {
        for(int slot = 0; slot < NUMTEXDICTS; ++slot) {
            auto def = GetTxdDef(slot);
            if(def && def->txd == dictionary) return def->name;
        }
    }
    (void)object;
    return std::string();
}

void destroyPreviewObject(rw::Atomic *atomic, rw::Clump *clump) {
    if(atomic) {
        auto frame = atomic->getFrame();
        if(frame) {
            atomic->setFrame(nullptr);
            frame->destroyHierarchy();
        }
        atomic->destroy();
    }
    if(clump) clump->destroy();
}

Json modelInfo(const Json &request) {
    bool byModel = request.contains("model"), byId = request.contains("id");
    if(byModel == byId) throw std::runtime_error("model_info requires exactly one of model or id");
    int model = -1;
    int objectId = -1;
    if(byModel) {
        model = (int)checkedJsonInteger(request.at("model"),"model_info model",0,NUMOBJECTDEFS-1);
    } else {
        objectId = (int)checkedJsonInteger(request.at("id"),"model_info id",0,2147483647);
        auto object = row(objectId);
        if(!object) throw std::runtime_error("model_info document object id does not exist");
        model = object->at("model").get<int>();
    }
    if(model < 0 || model >= NUMOBJECTDEFS) throw std::runtime_error("model_info model is out of range");

    auto definition = GetObjectDef(model);
    Json result = {{"model", model}, {"object_id", byId ? Json(objectId) : Json(nullptr)},
        {"name", definition ? std::string(definition->m_name) : std::string()},
        {"render_type", !definition ? std::string("missing") :
            (definition->m_type == ObjectDef::CLUMP ? std::string("clump") : std::string("atomic"))},
        {"txd", nullptr},
        {"origin_axes", {{"origin", Json::array({0,0,0})}, {"x", Json::array({1,0,0})},
            {"y", Json::array({0,1,0})}, {"z", Json::array({0,0,1})}}},
        {"visual_bounds", {{"available", false}, {"min", nullptr}, {"max", nullptr}, {"size", nullptr}, {"source", "render_geometry_vertices"}}},
        {"collision_bounds", {{"available", false}, {"min", nullptr}, {"max", nullptr}, {"size", nullptr}, {"source", "collision_model"}}},
        {"provenance", {{"defined", definition != nullptr}, {"installed", false}, {"loaded_before", false},
            {"loaded_after", false}, {"renderable", false}, {"availability", "missing"}, {"source_kind", "missing"}}},
        {"submeshes", Json::array()}, {"material_slots", Json::array()}, {"diagnostics", Json::array()},
        {"revision", document.revision}};
    if(objectId >= 0) result["object_id"] = objectId;
    if(!definition) {
        result["diagnostics"].push_back({{"code","model_missing"},{"severity","error"},{"message","No local object definition exists for this model"}});
        return result;
    }
    const bool loadedBefore = definition->IsLoaded();
    const char *looseDff = ModloaderFindOverride(definition->m_name,"dff");
    const char *archive = definition->m_imageIndex >= 0 ? GetCdImageLogicalName(definition->m_imageIndex) : nullptr;
    bool installed = looseDff != nullptr || definition->m_imageIndex >= 0;
    result["provenance"] = {{"defined",true},{"installed",installed},{"loaded_before",loadedBefore},
        {"loaded_after",loadedBefore},{"renderable",false},
        {"availability",loadedBefore?"loaded_unverified":(installed?"defined_unloaded":"definition_only")},
        {"source_kind",looseDff?"modloader_override":(archive?"game_archive":"definition_only")},
        {"source_archive",archive?Json(archive):Json(nullptr)}};
    auto txdDefinition = GetTxdDef(definition->m_txdSlot);
    if(txdDefinition) result["txd"] = txdDefinition->name;
    else result["diagnostics"].push_back({{"code","model_txd_missing"},{"severity","warning"},{"message","Model has no local texture dictionary definition"}});

    auto refreshCollisionBounds = [&]() {
        Json diagnostics=Json::array();
        for(const auto &diagnostic:result["diagnostics"])
            if(diagnostic.value("code",std::string())!="collision_bounds_unavailable") diagnostics.push_back(diagnostic);
        if(definition->m_colModel) {
            const CBox &box=definition->m_colModel->boundingBox;
            result["collision_bounds"]=boundsJson(box.min,box.max,"collision_model");
        } else {
            result["collision_bounds"]={{"available",false},{"min",nullptr},{"max",nullptr},{"size",nullptr},{"source","collision_model"}};
            diagnostics.push_back({{"code","collision_bounds_unavailable"},{"severity","info"},{"message","No collision model is available for this model"}});
        }
        result["diagnostics"]=std::move(diagnostics);
    };
    refreshCollisionBounds();

    rw::Atomic *atomic = nullptr;
    rw::Clump *clump = nullptr;
    bool previewCreated=CreateObjectPreviewRwObject(model, &atomic, &clump);
    refreshCollisionBounds();
    if(!previewCreated) {
        result["provenance"]["loaded_after"] = definition->IsLoaded();
        result["provenance"]["availability"] = definition->m_cantLoad ? "load_failed" :
            (installed ? "preview_unavailable" : "definition_only");
        result["diagnostics"].push_back({{"code","visual_geometry_unavailable"},{"severity","warning"},{"message","Local render geometry could not be loaded into an isolated preview object"}});
        return result;
    }
    result["provenance"]["loaded_after"] = definition->IsLoaded();
    result["provenance"]["renderable"] = true;
    result["provenance"]["availability"] = "renderable";

    struct Submesh {
        int index;
        std::string name;
        rw::Atomic *atomic;
    };
    std::vector<Submesh> submeshes;
    if(atomic) {
        submeshes.push_back({0, std::string(definition->m_name), atomic});
    } else if(clump) {
        auto root = clump->getFrame();
        if(root) root->updateObjects();
        int index = 0;
        FORLIST(link, clump->atomics) {
            auto part = rw::Atomic::fromClump(link);
            const char *nodeName = part && part->getFrame() ? gta::getNodeName(part->getFrame()) : nullptr;
            std::string name = nodeName && nodeName[0] ? nodeName : std::string("atomic_") + std::to_string(index);
            submeshes.push_back({index++, name, part});
        }
    }

    bool visualAvailable = false, visualIncomplete = false;
    rw::V3d visualMin = {0,0,0}, visualMax = {0,0,0};
    std::map<int, Json> slots;
    for(const auto &part : submeshes) {
        Json submesh = {{"index", part.index}, {"name", part.name}, {"material_slots", Json::array()}};
        auto geometry = part.atomic ? part.atomic->geometry : nullptr;
        if(!geometry) {
            visualIncomplete = true;
            result["diagnostics"].push_back({{"code","submesh_geometry_unavailable"},{"severity","warning"},{"submesh_index",part.index},{"message","Submesh has no readable geometry"}});
            result["submeshes"].push_back(submesh);
            continue;
        }

        const rw::Matrix *transform = part.atomic->getFrame() ? part.atomic->getFrame()->getLTM() : nullptr;
        auto morph = geometry->numMorphTargets > 0 ? &geometry->morphTargets[0] : nullptr;
        if(!morph || !morph->vertices || geometry->numVertices <= 0 || !transform || (geometry->flags & rw::Geometry::NATIVE)) {
            visualIncomplete = true;
            result["diagnostics"].push_back({{"code","visual_vertices_unavailable"},{"severity","warning"},{"submesh_index",part.index},{"message","Submesh does not expose CPU-readable visual vertices"}});
        } else {
            for(int vertex = 0; vertex < geometry->numVertices; ++vertex) {
                rw::V3d point;
                rw::V3d::transformPoints(&point, &morph->vertices[vertex], 1, transform);
                accumulateBounds(point, visualMin, visualMax, visualAvailable);
            }
        }

        std::vector<int> localSlots;
        for(int slot = 0; slot < geometry->matList.numMaterials; ++slot) {
            auto material = geometry->matList.materials ? geometry->matList.materials[slot] : nullptr;
            auto texture = material ? material->texture : nullptr;
            auto materialSlot = slots.find(slot);
            if(materialSlot == slots.end()) {
                materialSlot = slots.emplace(slot, Json{{"slot",slot},{"surfaces",Json::array()},{"submeshes",Json::array()}}).first;
            }
            int triangleCount = 0;
            bool triangleUsageKnown = geometry->triangles != nullptr || geometry->numTriangles == 0;
            if(geometry->triangles) {
                for(int triangle = 0; triangle < geometry->numTriangles; ++triangle)
                    if(geometry->triangles[triangle].matId == slot) ++triangleCount;
            }
            std::string txdName = texture ? textureDictionaryName(texture->dict, definition) : std::string();
            Json surface = {{"submesh_index",part.index},{"submesh_name",part.name},{"geometry_slot",slot},
                {"texture",texture ? Json(texture->name) : Json(nullptr)},
                {"txd",txdName.empty() ? Json(nullptr) : Json(txdName)},
                {"original_txd_resolved",!texture || !txdName.empty()},
                {"triangle_count",triangleUsageKnown ? Json(triangleCount) : Json(nullptr)},
                {"used_by_geometry",triangleUsageKnown ? Json(triangleCount > 0) : Json(nullptr)}};
            materialSlot->second["surfaces"].push_back(surface);
            materialSlot->second["submeshes"].push_back({{"index",part.index},{"name",part.name}});
            localSlots.push_back(slot);
            if(texture && txdName.empty())
                result["diagnostics"].push_back({{"code","original_texture_dictionary_unknown"},{"severity","warning"},
                    {"submesh_index",part.index},{"slot",slot},{"texture",texture->name},
                    {"message","Texture name is readable but its original TXD could not be identified"}});
        }
        submesh["material_slots"] = localSlots;
        result["submeshes"].push_back(submesh);
    }
    if(visualAvailable) {
        result["visual_bounds"] = boundsJson(visualMin, visualMax, "render_geometry_vertices");
        result["visual_bounds"]["status"] = visualIncomplete ? "partial" : "complete";
    } else {
        result["visual_bounds"]["status"] = "unavailable";
        if(!visualIncomplete && submeshes.empty())
            result["diagnostics"].push_back({{"code","visual_submeshes_unavailable"},{"severity","warning"},{"message","Preview object contained no atomic submeshes"}});
    }
    for(auto &slot : slots) {
        std::sort(slot.second["submeshes"].begin(), slot.second["submeshes"].end(), [](const Json &a,const Json &b){return a["index"]<b["index"];});
        slot.second["submeshes"].erase(std::unique(slot.second["submeshes"].begin(), slot.second["submeshes"].end(), [](const Json &a,const Json &b){return a["index"]==b["index"]; }), slot.second["submeshes"].end());
        result["material_slots"].push_back(slot.second);
    }
    destroyPreviewObject(atomic, clump);
    return result;
}

std::array<uint8_t, 7> smallGlyph(char value) {
    switch((char)toupper((unsigned char)value)) {
    case 'A': return {{14,17,17,31,17,17,17}}; case 'B': return {{30,17,17,30,17,17,30}};
    case 'C': return {{14,17,16,16,16,17,14}}; case 'D': return {{30,17,17,17,17,17,30}};
    case 'E': return {{31,16,16,30,16,16,31}}; case 'F': return {{31,16,16,30,16,16,16}};
    case 'G': return {{14,17,16,23,17,17,15}}; case 'H': return {{17,17,17,31,17,17,17}};
    case 'I': return {{14,4,4,4,4,4,14}}; case 'J': return {{7,2,2,2,18,18,12}};
    case 'K': return {{17,18,20,24,20,18,17}}; case 'L': return {{16,16,16,16,16,16,31}};
    case 'M': return {{17,27,21,21,17,17,17}}; case 'N': return {{17,25,21,19,17,17,17}};
    case 'O': return {{14,17,17,17,17,17,14}}; case 'P': return {{30,17,17,30,16,16,16}};
    case 'Q': return {{14,17,17,17,21,18,13}}; case 'R': return {{30,17,17,30,20,18,17}};
    case 'S': return {{15,16,16,14,1,1,30}}; case 'T': return {{31,4,4,4,4,4,4}};
    case 'U': return {{17,17,17,17,17,17,14}}; case 'V': return {{17,17,17,17,17,10,4}};
    case 'W': return {{17,17,17,21,21,21,10}}; case 'X': return {{17,17,10,4,10,17,17}};
    case 'Y': return {{17,17,10,4,4,4,4}}; case 'Z': return {{31,1,2,4,8,16,31}};
    case '0': return {{14,17,19,21,25,17,14}}; case '1': return {{4,12,4,4,4,4,14}};
    case '2': return {{14,17,1,2,4,8,31}}; case '3': return {{30,1,1,14,1,1,30}};
    case '4': return {{2,6,10,18,31,2,2}}; case '5': return {{31,16,16,30,1,1,30}};
    case '6': return {{14,16,16,30,17,17,14}}; case '7': return {{31,1,2,4,8,8,8}};
    case '8': return {{14,17,17,14,17,17,14}}; case '9': return {{14,17,17,15,1,1,14}};
    case '-': return {{0,0,0,31,0,0,0}}; case '_': return {{0,0,0,0,0,0,31}};
    case '/': return {{1,1,2,4,8,16,16}}; case ':': return {{0,4,4,0,4,4,0}};
    case '.': return {{0,0,0,0,0,12,12}}; case '|': return {{4,4,4,4,4,4,4}};
    case ' ': return {{0,0,0,0,0,0,0}}; default: return {{14,17,1,2,4,0,4}};
    }
}

void drawSmallText(rw::Image *image, int x, int y, const std::string &text, rw::RGBA color, int maxCharacters) {
    if(!image || !image->pixels) return;
    int character = 0;
    for(unsigned char raw : text) {
        if(character++ >= maxCharacters) break;
        auto glyph = smallGlyph((char)raw);
        int originX = x + (character - 1) * 6;
        for(int rowIndex = 0; rowIndex < 7; ++rowIndex) {
            int py = y + rowIndex;
            if(py < 0 || py >= image->height) continue;
            for(int col = 0; col < 5; ++col) {
                int px = originX + col;
                if(px < 0 || px >= image->width || !(glyph[rowIndex] & (1 << (4-col)))) continue;
                uint8_t *pixel = image->pixels + py * image->stride + px * 4;
                pixel[0]=color.red; pixel[1]=color.green; pixel[2]=color.blue; pixel[3]=color.alpha;
            }
        }
    }
}

rw::Image *checkerboardImage(int width, int height) {
    auto image = rw::Image::create(width, height, 32);
    if(!image) return nullptr;
    image->allocate();
    if(!image->pixels) { image->destroy(); return nullptr; }
    for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) {
        uint8_t shade = (((x / 8) + (y / 8)) & 1) ? 190 : 142;
        uint8_t *pixel = image->pixels + y * image->stride + x * 4;
        pixel[0]=shade; pixel[1]=shade; pixel[2]=shade; pixel[3]=255;
    }
    return image;
}

void compositeImage(rw::Image *destination, int destX, int destY, int destW, int destH, const rw::Image *source) {
    if(!destination || !destination->pixels || !source || !source->pixels || source->width <= 0 || source->height <= 0) return;
    for(int y = 0; y < destH; ++y) for(int x = 0; x < destW; ++x) {
        int sx = std::min(source->width-1, (int)((int64_t)x * source->width / destW));
        int sy = std::min(source->height-1, (int)((int64_t)y * source->height / destH));
        const uint8_t *src = source->pixels + sy * source->stride + sx * source->bpp;
        uint8_t *dst = destination->pixels + (destY+y) * destination->stride + (destX+x) * 4;
        double alpha = source->bpp >= 4 ? src[3] / 255.0 : 1.0;
        for(int c = 0; c < 3; ++c) dst[c] = (uint8_t)(src[c] * alpha + dst[c] * (1.0-alpha));
        dst[3] = 255;
    }
}

rw::Image *singleTextureImage(const rw::Image *source) {
    if(!source || !source->pixels || source->width <= 0 || source->height <= 0 || source->width > 8192 || source->height > 8192) return nullptr;
    auto image = checkerboardImage(source->width, source->height);
    if(image) compositeImage(image, 0, 0, image->width, image->height, source);
    return image;
}

rw::Image *candidateBoard(const std::vector<rw::Image*> &images, const std::vector<std::string> &labels) {
    if(images.empty() || images.size() != labels.size() || images.size() > 64) return nullptr;
    const int cellWidth = 240, cellHeight = 220, imageHeight = 166;
    int columns = std::min(4, (int)images.size());
    int rows = ((int)images.size() + columns - 1) / columns;
    auto board = rw::Image::create(columns * cellWidth, rows * cellHeight, 32);
    if(!board) return nullptr;
    board->allocate();
    if(!board->pixels) { board->destroy(); return nullptr; }
    for(int y = 0; y < board->height; ++y) for(int x = 0; x < board->width; ++x) {
        uint8_t *pixel = board->pixels + y * board->stride + x * 4;
        pixel[0]=42; pixel[1]=45; pixel[2]=51; pixel[3]=255;
    }
    rw::RGBA labelColor={238,240,244,255};
    for(size_t index = 0; index < images.size(); ++index) {
        int cellX = ((int)index % columns) * cellWidth;
        int cellY = ((int)index / columns) * cellHeight;
        auto checker = checkerboardImage(cellWidth-16, imageHeight);
        if(checker) {
            if(images[index]) {
                double scale = std::min((double)(cellWidth-24)/images[index]->width, (double)(imageHeight-8)/images[index]->height);
                int width = std::max(1, (int)(images[index]->width*scale));
                int height = std::max(1, (int)(images[index]->height*scale));
                compositeImage(checker, ((cellWidth-16)-width)/2, (imageHeight-height)/2, width, height, images[index]);
            } else {
                drawSmallText(checker, 10, imageHeight/2-3, "UNAVAILABLE", labelColor, 24);
            }
            for(int y = 0; y < checker->height; ++y)
                memcpy(board->pixels + (cellY+y)*board->stride + (cellX+8)*4, checker->pixels + y*checker->stride, checker->stride);
            checker->destroy();
        }
        std::string label=labels[index];
        size_t split=label.find('\n');
        drawSmallText(board,cellX+8,cellY+imageHeight+10,label.substr(0,split),labelColor,36);
        if(split!=std::string::npos) drawSmallText(board,cellX+8,cellY+imageHeight+20,label.substr(split+1),labelColor,36);
    }
    return board;
}

void normalizeImage32(rw::Image *&image) {
    if(!image) return;
    if(image->depth != 32 || image->bpp != 4) image->convertTo32();
}

std::string pathJoin(const std::string &base, const std::string &name) {
    if(base.empty()) return name;
    char last=base[base.size()-1];
    return base + ((last=='/' || last=='\\') ? "" : "/") + name;
}

bool isDirectory(const std::string &path) {
#ifdef _WIN32
    struct _stat info;
#else
    struct stat info;
#endif
#ifdef _WIN32
    if(_stat(path.c_str(),&info)!=0) return false;
    return (info.st_mode & _S_IFDIR)!=0;
#else
    if(stat(path.c_str(),&info)!=0) return false;
    return S_ISDIR(info.st_mode);
#endif
}

bool createDirectory(const std::string &path) {
    if(isDirectory(path)) return true;
#ifdef _WIN32
    return _mkdir(path.c_str())==0 || isDirectory(path);
#else
    return mkdir(path.c_str(),0777)==0 || isDirectory(path);
#endif
}

bool pathExists(const std::string &path) {
#ifdef _WIN32
    struct _stat info;
#else
    struct stat info;
#endif
#ifdef _WIN32
    return _stat(path.c_str(),&info)==0;
#else
    return stat(path.c_str(),&info)==0;
#endif
}

bool createDirectoryTree(const std::string &path) {
    if(path.empty()) return false;
    std::string current;
    size_t start=0;
#ifdef _WIN32
    if(path.size()>=2 && path[1]==':') { current=path.substr(0,2); start=2; }
#endif
    if(start<path.size() && (path[start]=='/' || path[start]=='\\')) { current+=path[start++]; }
    for(size_t i=start;i<=path.size();++i) {
        if(i<path.size() && path[i]!='/' && path[i]!='\\') continue;
        if(i>start) {
            if(!current.empty() && current.back()!='/' && current.back()!='\\') current+='/';
            current+=path.substr(start,i-start);
            if(!createDirectory(current)) return false;
        }
        start=i+1;
    }
    return isDirectory(path);
}

std::string texturePreviewPath(const std::string &directory, bool board, size_t index, bool compared) {
    if(board) return pathJoin(directory,"candidate-board.png");
    if(index == (size_t)-1) return pathJoin(directory,"texture.png");
    char name[48];
    snprintf(name,sizeof(name),compared ? "material_%03u.png" : "texture_%03u.png",(unsigned)index);
    return pathJoin(directory,name);
}

bool savePng(rw::Image *image, const std::string &path) {
    if(!image || !image->pixels) return false;
    rw::writePNG(image, path.c_str());
#ifdef _WIN32
    struct _stat info;
    return _stat(path.c_str(),&info)==0 && info.st_size>0;
#else
    struct stat info;
    return stat(path.c_str(),&info)==0 && info.st_size>0;
#endif
}

Json texturePreview(const Json &request) {
    if(!request.contains("output_dir") || !request.at("output_dir").is_string() || request.at("output_dir").get<std::string>().empty())
        throw std::runtime_error("texture_preview requires a non-empty output_dir");
    const bool boardRequest = request.contains("candidates");
    if(boardRequest == request.contains("texture"))
        throw std::runtime_error("texture_preview requires exactly one of texture or candidates");
    const bool comparing = request.contains("compare");
    if(comparing && !boardRequest) throw std::runtime_error("texture_preview compare requires candidates");
    Json compare = comparing ? request.at("compare") : Json::object();
    int compareModel=-1, compareSlot=-1, compareSize=256;
    float compareAngle=0.65f;
    if(comparing) {
        if(!compare.is_object() || !compare.contains("model") || !compare.contains("slot"))
            throw std::runtime_error("texture_preview compare requires model and slot");
        compareModel=(int)checkedJsonInteger(compare.at("model"),"texture_preview compare model",0,NUMOBJECTDEFS-1);
        compareSlot=(int)checkedJsonInteger(compare.at("slot"),"texture_preview compare slot",0,255);
        compareSize=compare.contains("size")?(int)checkedJsonInteger(compare.at("size"),"texture_preview compare size",96,1024):256;
        if(compare.contains("angle")) {
            if(!compare.at("angle").is_number()) throw std::runtime_error("texture_preview compare angle must be numeric");
            compareAngle=compare.at("angle").get<float>();
        }
        if(!std::isfinite(compareAngle))
            throw std::runtime_error("texture_preview compare values are out of range");
    }

    struct Candidate { int model; std::string txd, texture, label; uint32_t color; };
    std::vector<Json> candidateSpecs;
    if(boardRequest) {
        if(!request.at("candidates").is_array() || request.at("candidates").empty() || request.at("candidates").size()>64)
            throw std::runtime_error("texture_preview candidates must contain 1..64 entries");
        for(const auto &entry:request.at("candidates")) candidateSpecs.push_back(entry);
    } else candidateSpecs.push_back(request.at("texture"));
    std::vector<Candidate> candidates;
    for(const Json &spec:candidateSpecs) {
        if(!spec.is_object() || !spec.contains("texture") || !spec.at("texture").is_string() || spec.at("texture").get<std::string>().empty())
            throw std::runtime_error("each texture_preview item requires a non-empty texture name");
        int model=spec.contains("model")?(int)checkedJsonInteger(spec.at("model"),"texture_preview item model",-1,NUMOBJECTDEFS-1):-1;
        std::string txdName;
        if(spec.contains("txd")) {
            if(!spec.at("txd").is_string()) throw std::runtime_error("texture_preview item txd must be a string");
            txdName=spec.at("txd").get<std::string>();
        }
        if(txdName.empty() && model>=0) {
            auto object=GetObjectDef(model); auto txd=object?GetTxdDef(object->m_txdSlot):nullptr;
            if(txd) txdName=txd->name;
        }
        if(txdName.empty()) throw std::runtime_error("texture_preview item requires txd or a model with a TXD");
        std::string label;
        if(spec.contains("label")) {
            if(!spec.at("label").is_string()) throw std::runtime_error("texture_preview item label must be a string");
            label=spec.at("label").get<std::string>();
        }
        std::string textureName=spec.at("texture").get<std::string>();
        if(label.empty()) {
            auto def=model>=0?GetObjectDef(model):nullptr;
            label=(model>=0?std::to_string(model)+" "+(def?std::string(def->m_name):std::string()):txdName);
            label+="\n"+txdName+" / "+textureName;
        }
        uint32_t color=0xFFFFFFFFu;
        if(spec.contains("color")) {
            const Json &value=spec.at("color");
            if(value.is_string()) {
                std::string hex=value.get<std::string>();
                if(hex.compare(0,2,"0x")==0 || hex.compare(0,2,"0X")==0) hex.erase(0,2);
                if(hex.empty() || hex.size()>8) throw std::runtime_error("texture_preview item color must be a 32-bit ARGB value");
                size_t consumed=0; unsigned long long parsed=0;
                try { parsed=std::stoull(hex,&consumed,16); }
                catch(const std::exception &) { throw std::runtime_error("texture_preview item color must be hexadecimal ARGB"); }
                if(consumed!=hex.size() || parsed>0xFFFFFFFFull) throw std::runtime_error("texture_preview item color must be a 32-bit ARGB value");
                color=(uint32_t)parsed;
            } else color=(uint32_t)checkedJsonInteger(value,"texture_preview item color",0,0xFFFFFFFFll);
        }
        candidates.push_back({model,txdName,textureName,label,color});
    }

    std::string root=request.at("output_dir").get<std::string>();
    if(!createDirectoryTree(root)) throw std::runtime_error("texture_preview could not create output_dir");
    static uint64_t outputSequence=0;
    uint64_t stamp=(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    std::string directory;
    do {
        char folder[96];
        snprintf(folder,sizeof(folder),"samp_texture_preview_%llu_%llu",(unsigned long long)stamp,(unsigned long long)outputSequence++);
        directory=pathJoin(root,folder);
    } while(pathExists(directory));
    if(!createDirectoryTree(directory)) throw std::runtime_error("texture_preview could not create its output folder");

    std::string previousMessage=message;
    struct RestoreMessage { std::string &target; std::string saved; ~RestoreMessage(){target=saved;} } restoreMessage{message,previousMessage};
    std::vector<rw::Image*> images;
    struct ImageListGuard { std::vector<rw::Image*> &images; ~ImageListGuard(){for(auto image:images) if(image) image->destroy();} } imageGuard{images};
    std::vector<std::string> labels;
    Json items=Json::array(), files=Json::array();
    {
        struct TxdPushGuard { TxdPushGuard(){TxdPush();} ~TxdPushGuard(){TxdPop();} } txdGuard;
        for(size_t index=0;index<candidates.size();++index) {
            const Candidate &candidate=candidates[index];
            const bool tintOnlyComparison=comparing && candidate.model==-1;
            Json lookupSpec={{"model",candidate.model},{"txd",candidate.txd},{"texture",candidate.texture}};
            message.clear();
            rw::Texture *texture=comparing?nullptr:lookup(lookupSpec,true);
            rw::Image *source=nullptr;
            std::string diagnostic;
            if(texture && texture->raster) {
                source=texture->raster->toImage();
                normalizeImage32(source);
                if(!source || !source->pixels || source->width<=0 || source->height<=0) {
                    if(source) source->destroy();
                    source=nullptr; diagnostic="texture_image_unavailable";
                }
            } else if(!comparing) diagnostic=texture?"texture_raster_unavailable":"texture_unavailable";
            std::string outputPath=texturePreviewPath(directory,false,boardRequest?index:(size_t)-1,comparing);
            rw::Image *outputImage=nullptr;
            std::string itemKind=comparing?(tintOnlyComparison?"tint_on_object":"material_on_object"):"texture";
            if(comparing) {
                char renderError[256]={};
                bool rendered=CaptureObjectMaterialPreviewPng(compareModel,compareSlot,
                    tintOnlyComparison?nullptr:candidate.txd.c_str(),tintOnlyComparison?nullptr:candidate.texture.c_str(),
                    candidate.color,outputPath.c_str(),compareSize,compareAngle,renderError,sizeof(renderError),tintOnlyComparison);
                if(rendered) {
                    outputImage=rw::readPNG(outputPath.c_str());
                    normalizeImage32(outputImage);
                    if(!outputImage) diagnostic="material_preview_image_readback_failed";
                } else diagnostic=std::string("material_preview_unavailable: ")+renderError;
            } else if(source) {
                outputImage=singleTextureImage(source);
                source->destroy(); source=nullptr;
                if(outputImage && !savePng(outputImage,outputPath)) {
                    outputImage->destroy(); outputImage=nullptr; diagnostic="png_write_failed";
                }
            }
            bool available=outputImage && outputImage->pixels;
            int width=available?outputImage->width:(texture&&texture->raster?texture->raster->width:0);
            int height=available?outputImage->height:(texture&&texture->raster?texture->raster->height:0);
            if(available) {
                files.push_back(outputPath);
                images.push_back(outputImage);
            } else {
                images.push_back(nullptr);
                if(diagnostic.empty()) diagnostic="texture_unavailable";
            }
            labels.push_back(candidate.label);
            Json item={{"model",candidate.model},{"txd",candidate.txd},{"texture",candidate.texture},{"label",candidate.label},
                {"kind",itemKind},{"path",available?Json(outputPath):Json(nullptr)},
                {"width",width},{"height",height},{"available",available},
                {"source_valid",candidate.model>=0 && GetObjectDef(candidate.model)!=nullptr},
                {"usable",available}};
            if(comparing) {
                item["comparison_model"]=compareModel; item["slot"]=compareSlot;
                item["material_source"]=tintOnlyComparison?"original_geometry":"candidate_texture";
                item["tint_only"]=tintOnlyComparison;
            }
            if(!available) item["diagnostic"]=diagnostic;
            items.push_back(item);
        }
    }

    Json boardPath=nullptr;
    if(boardRequest) {
        auto board=candidateBoard(images,labels);
        if(!board) throw std::runtime_error("texture_preview candidate board allocation failed");
        std::string path=texturePreviewPath(directory,true,0,comparing);
        bool saved=savePng(board,path); board->destroy();
        if(!saved) throw std::runtime_error("texture_preview could not write candidate-board.png");
        boardPath=path; files.push_back(boardPath);
    }
    return {{"items",items},{"files",files},{"board_path",boardPath},{"output_dir",directory},
        {"comparison",comparing?compare:Json(nullptr)},{"revision",document.revision}};
}
