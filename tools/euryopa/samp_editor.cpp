#include "euryopa.h"
#include "samp_editor.h"
#include "agentbridge.h"
#include "samp_document.h"
#include "samp_rotation.h"
#include <map>
#include <set>
#include <cmath>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstring>
#include <tuple>
#include <algorithm>
#ifdef _WIN32
#include <commdlg.h>
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comdlg32.lib")
#endif

namespace {
using samp::Json;
samp::Document document;
bool active=false, applying=false, showSampWindow=true;
#ifdef _WIN32
static bool browseFile(char *buffer, size_t bufferSize, bool save, const char *filter, const char *defaultExt) {
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = (DWORD)bufferSize;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
    if(save) ofn.Flags |= OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    else ofn.Flags |= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return save ? (GetSaveFileNameA(&ofn) != 0) : (GetOpenFileNameA(&ofn) != 0);
}
#endif
std::map<int,ObjectInst*> objects;
std::map<ObjectInst*,int> identities;
std::map<ObjectInst*,std::string> rendered;
std::map<std::string,rw::Texture*> textTextures;
std::vector<std::pair<ObjectInst*,Json>> cutRows;
std::vector<Json> copiedRows;
std::set<int> checkedObjectIds;
std::string message;
int selected=0, slot=0;
Json pending;
rw::V3d vector(const Json &j) { return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()}; }
rw::Quat rotation(const Json &j) {
    return samp::ToStoredRotation({j[0].get<double>(),j[1].get<double>(),j[2].get<double>()});
}
Json angles(const ObjectInst *i) {
    auto result=samp::FromRenderMatrix(i->m_matrix);
    return {result[0],result[1],result[2]};
}
Json *row(int id) {
    static unsigned indexedRevision=~0u;
    static std::map<int,size_t> indices;
    if(indexedRevision!=document.revision) {
        indices.clear();
        for(size_t n=0;n<document.data["objects"].size();++n)
            indices[document.data["objects"][n]["id"].get<int>()]=n;
        indexedRevision=document.revision;
    }
    auto found=indices.find(id);
    return found==indices.end()?nullptr:&document.data["objects"][found->second];
}
void updateFrame(ObjectInst *i) {
    i->UpdateMatrix(); if(!i->m_rwObject) return;
    auto def=GetObjectDef(i->m_objectId); if(!def) return;
    rw::Frame *f=def->m_type==ObjectDef::ATOMIC?((rw::Atomic*)i->m_rwObject)->getFrame():((rw::Clump*)i->m_rwObject)->getFrame();
    f->transform(&i->m_matrix,rw::COMBINEREPLACE);
}
void retire(ObjectInst *i) {
    // Keep the runtime pointer stable for editor references and document Undo.
    // Retired records must not retain cloned geometry or animation resources.
    i->Deselect();
    if(!i->m_isDeleted) RemoveInstFromSectors(i);
    i->m_isDeleted=true;
    i->DestroyRwObject();
}
void sync() {
    applying=true;
    std::set<int> live;
    for(auto &o:document.data["objects"]) {
        int id=o["id"]; live.insert(id); ObjectInst *i=objects.count(id)?objects[id]:nullptr;
        auto def=GetObjectDef(o["model"].get<int>());
        if(!def) {
            if(i) retire(i);
            continue;
        }
        auto pos=vector(o["position"]);
        if(i && i->m_objectId!=o["model"].get<int>()) {
            retire(i);
            i->m_objectId=o["model"].get<int>();
            i->m_isBigBuilding=def->m_isBigBuilding;
        }
        if(!i) { i=SampCreateInstance(o["model"],pos.x,pos.y,pos.z); objects[id]=i; identities[i]=id; }
        i->m_isDeleted=false; RemoveInstFromSectors(i); i->m_translation=pos; i->m_rotation=rotation(o["rotation"]);
        i->m_area=o["interior"].get<int>()<0?0:o["interior"].get<int>(); updateFrame(i);
        if(!i->m_rwObject) {
            if(!def->IsLoaded()) { RequestObject(i->m_objectId); LoadAllRequestedObjects(); }
            i->CreateRwObject();
        }
        InsertInstIntoSectors(i);
        SampApplyMaterials(i);
    }
    for(auto &item:objects) if(!live.count(item.first)) retire(item.second);
    applying=false;
}
rw::RGBA rgba(uint32_t c) { return {(uint8)(c>>16),(uint8)(c>>8),(uint8)c,(uint8)(c>>24)}; }
rw::Texture *lookup(const Json &m, bool quiet = false) {
    int txd=FindTxdSlot(m.at("txd").get<std::string>().c_str());
    if(txd<0) { if(!quiet) message="Missing TXD: "+m.at("txd").get<std::string>(); return nullptr; }
    static std::set<int> failedTxds;
    if(failedTxds.count(txd)) return nullptr;
    if(!IsTxdLoaded(txd)) {
        LoadTxd(txd);
        if(!IsTxdLoaded(txd)) { failedTxds.insert(txd); return nullptr; }
    }
    auto def=GetTxdDef(txd); if(!def || !def->txd) return nullptr;
    auto texture=def->txd->find(m.at("texture").get<std::string>().c_str());
    if(!texture && !quiet) message="Missing texture: "+m.at("texture").get<std::string>();
    return texture;
}
rw::Texture *textTexture(const Json &m) {
    std::string key=m.dump(); if(textTextures.count(key)) return textTextures[key];
#ifdef _WIN32
    static const int sizes[][2]={{32,32},{64,32},{64,64},{128,32},{128,64},{128,128},{256,32},{256,64},{256,128},{256,256},{512,64},{512,128},{512,256},{512,512}};
    int n=m.at("size").get<int>()/10-1, w=sizes[n][0], h=sizes[n][1];
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO info={}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void *bits=nullptr; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap) { DeleteDC(dc); message="Cannot rasterize text"; return nullptr; }
    auto oldBitmap=SelectObject(dc,bitmap);
    std::string face=m.at("font"), text=m.at("text");
    auto wide=[](const std::string &s) { int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0); std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n); return w; };
    HFONT font=CreateFontW(-m.at("font_size").get<int>(),0,0,0,m.at("bold").get<bool>()?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,wide(face).c_str());
    auto oldFont=SelectObject(dc,font);
    wchar_t actual[128]={}; GetTextFaceW(dc,128,actual);
    if(_wcsicmp(actual,wide(face).c_str())) message="Font substitution: "+face;
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    auto fg=rgba(m.at("foreground")), bg=rgba(m.at("background"));
    auto image=rw::Image::create(w,h,32); image->allocate();
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        auto pixel=image->pixels+y*image->stride+x*4;
        pixel[0]=bg.red; pixel[1]=bg.green; pixel[2]=bg.blue; pixel[3]=bg.alpha;
    }
    struct Glyph { wchar_t character; rw::RGBA color; int width; };
    std::vector<std::vector<Glyph>> lines(1); int lineWidth=0;
    auto content=wide(text); content.pop_back();
    for(size_t n=0;n<content.size();++n) {
        wchar_t character=content[n];
        if(character=='{' && n+7<content.size() && content[n+7]=='}') {
            auto code=content.substr(n+1,6); bool valid=true;
            for(auto c:code) if(!iswxdigit(c)) valid=false;
            if(valid) { fg=rgba(((uint32_t)fg.alpha<<24)|(uint32_t)wcstoul(code.c_str(),nullptr,16)); n+=7; continue; }
        }
        if(character=='\r') continue;
        if(character=='\n') { lines.emplace_back(); lineWidth=0; continue; }
        SIZE extent={}; GetTextExtentPoint32W(dc,&character,1,&extent);
        if(lineWidth+extent.cx>w && !lines.back().empty()) { lines.emplace_back(); lineWidth=0; }
        lines.back().push_back({character,fg,(int)extent.cx}); lineWidth+=extent.cx;
    }
    TEXTMETRICW metrics={}; GetTextMetricsW(dc,&metrics); int y=0, alignment=m.at("align");
    for(auto &line:lines) {
        int width=0; for(auto &glyph:line) width+=glyph.width;
        int x=alignment==1?(w-width)/2:alignment==2?w-width:0;
        for(auto &glyph:line) {
            memset(bits,0,w*h*4); TextOutW(dc,x,y,&glyph.character,1); GdiFlush();
            for(int py=std::max(0,y);py<std::min(h,y+(int)metrics.tmHeight);++py) for(int px=std::max(0,x-2);px<std::min(w,x+glyph.width+3);++px) {
                auto coverage=((unsigned char*)bits)[(py*w+px)*4]; if(!coverage) continue;
                auto pixel=image->pixels+py*image->stride+px*4;
                double a=coverage/255.0*glyph.color.alpha/255.0, ba=(1-a)*pixel[3]/255.0, total=a+ba;
                pixel[0]=total?(uint8)((glyph.color.red*a+pixel[0]*ba)/total):0;
                pixel[1]=total?(uint8)((glyph.color.green*a+pixel[1]*ba)/total):0;
                pixel[2]=total?(uint8)((glyph.color.blue*a+pixel[2]*ba)/total):0; pixel[3]=(uint8)(total*255);
            }
            x+=glyph.width;
        }
        y+=metrics.tmHeight; if(y>=h) break;
    }
    SelectObject(dc,oldFont); SelectObject(dc,oldBitmap); DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
    auto raster=rw::Raster::createFromImage(image); image->destroy();
    if(!raster) return nullptr;
    auto texture=rw::Texture::create(raster); texture->setFilter(rw::Texture::LINEAR); if(textTextures.size()>=128) { auto oldest=textTextures.begin(); oldest->second->destroy(); textTextures.erase(oldest); }
    textTextures[key]=texture; return texture;
#else
    message="Material text rasterization currently requires Windows"; return nullptr;
#endif
}
bool applyAtomic(rw::Atomic *atomic,rw::Geometry *source,const Json &materials) {
    // Stream round-trip gives this instance independent geometry/materials.
    if(!source) { message="Missing source geometry"; return false; }
    std::vector<rw::uint8> bytes(source->streamGetSize()+12);
    rw::StreamMemory stream; stream.open(bytes.data(),0,(rw::uint32)bytes.size());
    if(!source->streamWrite(&stream)) { stream.close(); message="Geometry clone failed"; return false; }
    stream.seek(0,0);
    if(!rw::findChunk(&stream,rw::ID_GEOMETRY,nullptr,nullptr)) { stream.close(); message="Geometry clone failed"; return false; }
    rw::Geometry *geometry=rw::Geometry::streamRead(&stream); stream.close(); if(!geometry) { message="Geometry clone failed"; return false; }
    log("SA-MP material clone: %d slots, %d overrides\n", geometry->matList.numMaterials, (int)materials.size());
    atomic->setGeometry(geometry,0); geometry->destroy();
    for(auto it=materials.begin();it!=materials.end();++it) {
        int slot=std::stoi(it.key()); if(slot>=geometry->matList.numMaterials) continue;
        auto material=geometry->matList.materials[slot]; auto &m=it.value();
        auto texture=m.at("type")=="text"?textTexture(m):m.at("model")==-1?nullptr:lookup(m);
        if(texture) { material->setTexture(texture); log("SA-MP applied slot %d texture %p\n",slot,texture); }
        if(m.at("type")=="texture" && m.at("color").get<uint32_t>()!=0) material->color=rgba(m.at("color"));
        if(m.at("type")=="text") material->color={255,255,255,255};
    }
    return true;
}
std::map<int, std::vector<int>> txdToModels;
std::vector<int> discoveredTxdSlots;
std::set<int> discoveredTxdSet;
std::set<int> indexedTxdSet;
Json textureIndex = Json::array();
size_t textureCursor = 0;
size_t textureModelCursor = 0;
bool textureDiscoveryComplete = false;
bool textureIndexing = false;

void indexTxdSlot(int txdSlot) {
    if(txdSlot < 0 || !indexedTxdSet.insert(txdSlot).second) return;
    auto txd = GetTxdDef(txdSlot);
    if(!txd) return;
    if(!txd->txd) LoadTxd(txdSlot);
    if(!txd->txd) return;
    FORLIST(lnk, txd->txd->textures) {
        auto tex = rw::Texture::fromDict(lnk);
        if(!tex || !tex->name) continue;
        textureIndex.push_back({
            {"txd_slot", txdSlot},
            {"txd", txd->name},
            {"texture", tex->name}
        });
    }
}

void ensureModelTxdDiscovered(int model) {
    if(model < 0 || model >= NUMOBJECTDEFS) return;
    auto obj = GetObjectDef(model);
    if(obj && obj->m_txdSlot >= 0 && GetTxdDef(obj->m_txdSlot)) {
        auto &modelList = txdToModels[obj->m_txdSlot];
        if(std::find(modelList.begin(), modelList.end(), model) == modelList.end())
            modelList.push_back(model);
        if(discoveredTxdSet.insert(obj->m_txdSlot).second)
            discoveredTxdSlots.push_back(obj->m_txdSlot);
    }
}

void indexTextures(int budget) {
    int modelBudget = budget * 128;
    while(modelBudget-- > 0 && textureModelCursor < NUMOBJECTDEFS) {
        int model = (int)textureModelCursor++;
        auto obj = GetObjectDef(model);
        if(obj && obj->m_txdSlot >= 0 && GetTxdDef(obj->m_txdSlot)) {
            auto &modelList = txdToModels[obj->m_txdSlot];
            if(std::find(modelList.begin(), modelList.end(), model) == modelList.end())
                modelList.push_back(model);
            if(discoveredTxdSet.insert(obj->m_txdSlot).second)
                discoveredTxdSlots.push_back(obj->m_txdSlot);
        }
    }
    textureDiscoveryComplete = (textureModelCursor == NUMOBJECTDEFS);
    while(budget-- > 0 && textureCursor < discoveredTxdSlots.size()) {
        indexTxdSlot(discoveredTxdSlots[textureCursor++]);
    }
    if(textureDiscoveryComplete && textureCursor == discoveredTxdSlots.size())
        textureIndexing = false;
}

std::string lower(std::string value) { for(auto &c:value) c=(char)tolower((unsigned char)c); return value; }

Json textures(const std::string &query, int limit) {
    Json found = Json::array();
    std::string needle = lower(query);
    limit = std::max(1, std::min(1000, limit));

    int requestedModel = -1, requestedTxd = -1;
    bool isNumeric = !needle.empty() && std::all_of(needle.begin(), needle.end(), [](unsigned char c){ return std::isdigit(c) != 0; });
    if(isNumeric) {
        try {
            long long value = std::stoll(needle);
            if(value >= 0 && value < NUMOBJECTDEFS) {
                requestedModel = (int)value;
                ensureModelTxdDiscovered(requestedModel);
                auto def = GetObjectDef(requestedModel);
                if(def && def->m_txdSlot >= 0) {
                    requestedTxd = def->m_txdSlot;
                    indexTxdSlot(requestedTxd);
                }
            }
        } catch(const std::exception &) {}
    }

    std::vector<int> matchingModels;
    if(!needle.empty() && !isNumeric) {
        for(int m = 0; m < NUMOBJECTDEFS; ++m) {
            auto obj = GetObjectDef(m);
            if(!obj || !obj->m_name[0]) continue;
            if(lower(obj->m_name).find(needle) != std::string::npos) {
                matchingModels.push_back(m);
                ensureModelTxdDiscovered(m);
                if(obj->m_txdSlot >= 0) indexTxdSlot(obj->m_txdSlot);
                if(matchingModels.size() >= 30) break;
            }
        }
    }

    if(!needle.empty()) {
        int directTxd = FindTxdSlot(needle.c_str());
        if(directTxd >= 0) indexTxdSlot(directTxd);
    }

    std::set<std::tuple<int, std::string, std::string>> seen;

    auto addResult = [&](int modelId, const std::string &txdName, const std::string &texName) {
        if(found.size() >= (size_t)limit) return false;
        if(!seen.insert(std::make_tuple(modelId, txdName, texName)).second) return true;
        found.push_back({
            {"model", modelId},
            {"txd", txdName},
            {"texture", texName}
        });
        return found.size() < (size_t)limit;
    };

    if(requestedModel >= 0 && requestedTxd >= 0) {
        for(const auto &entry : textureIndex) {
            if(entry["txd_slot"].get<int>() == requestedTxd) {
                if(!addResult(requestedModel, entry["txd"].get<std::string>(), entry["texture"].get<std::string>()))
                    return found;
            }
        }
    }

    for(int m : matchingModels) {
        auto obj = GetObjectDef(m);
        if(!obj || obj->m_txdSlot < 0) continue;
        int slot = obj->m_txdSlot;
        for(const auto &entry : textureIndex) {
            if(entry["txd_slot"].get<int>() == slot) {
                if(!addResult(m, entry["txd"].get<std::string>(), entry["texture"].get<std::string>()))
                    return found;
            }
        }
    }

    for(const auto &entry : textureIndex) {
        int slot = entry["txd_slot"].get<int>();
        std::string txdName = entry["txd"].get<std::string>();
        std::string texName = entry["texture"].get<std::string>();

        bool match = needle.empty() ||
                     lower(txdName).find(needle) != std::string::npos ||
                     lower(texName).find(needle) != std::string::npos;
        if(!match) continue;

        auto it = txdToModels.find(slot);
        if(it != txdToModels.end() && !it->second.empty()) {
            for(int m : it->second) {
                if(!addResult(m, txdName, texName)) return found;
            }
        } else {
            if(!addResult(-1, txdName, texName)) return found;
        }
    }

    return found;
}

Json assetDiagnostics() {
    Json diagnostics=Json::array();
    for(const auto &object:document.data["objects"]) {
        int id=object["id"], model=object["model"];
        if(!GetObjectDef(model)) {
            diagnostics.push_back({{"code","model_missing"},{"severity","error"},{"kind","object"},{"id",id},{"model",model},{"message","Model is unavailable; document record is preserved but has no viewport preview"}});
            continue;
        }
        for(auto material=object["materials"].begin();material!=object["materials"].end();++material) {
            const auto &value=material.value();
            if(value["type"]=="texture" && value["model"].get<int>()!=-1) {
                std::string txdName=value["txd"], textureName=value["texture"];
                int txdSlot=FindTxdSlot(txdName.c_str());
                if(txdSlot<0) {
                    diagnostics.push_back({{"code","txd_missing"},{"severity","error"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"txd",txdName},{"texture",textureName},{"message","Texture dictionary is unavailable"}});
                    continue;
                }
                if(!IsTxdLoaded(txdSlot)) LoadTxd(txdSlot);
                auto txd=GetTxdDef(txdSlot);
                if(!txd || !txd->txd) {
                    diagnostics.push_back({{"code","txd_unavailable"},{"severity","error"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"txd",txdName},{"texture",textureName},{"message","Texture dictionary could not be loaded"}});
                } else if(!txd->txd->find(textureName.c_str())) {
                    diagnostics.push_back({{"code","texture_missing"},{"severity","error"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"txd",txdName},{"texture",textureName},{"message","Texture is unavailable in the requested dictionary"}});
                }
            }
#ifdef _WIN32
            if(value["type"]=="text") {
                std::string requested=value["font"];
                auto wide=[](const std::string &s) { int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0); std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n); return w; };
                HDC dc=CreateCompatibleDC(nullptr);
                HFONT font=CreateFontW(-value["font_size"].get<int>(),0,0,0,value["bold"].get<bool>()?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,wide(requested).c_str());
                if(!dc || !font) {
                    diagnostics.push_back({{"code","font_unavailable"},{"severity","error"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"font",requested},{"message","Font preview could not be created"}});
                } else {
                    auto oldFont=SelectObject(dc,font); wchar_t actual[128]={}; GetTextFaceW(dc,128,actual);
                    if(_wcsicmp(actual,wide(requested).c_str())) {
                        int length=WideCharToMultiByte(CP_UTF8,0,actual,-1,nullptr,0,nullptr,nullptr); std::string replacement(length?length:1,'\0');
                        if(length>1) { WideCharToMultiByte(CP_UTF8,0,actual,-1,&replacement[0],length,nullptr,nullptr); replacement.pop_back(); }
                        else replacement.clear();
                        diagnostics.push_back({{"code","font_substituted"},{"severity","warning"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"font",requested},{"replacement",replacement},{"message","Requested font is unavailable; Windows selected a replacement"}});
                    }
                    SelectObject(dc,oldFont);
                }
                if(font) DeleteObject(font); if(dc) DeleteDC(dc);
            }
#else
            if(value["type"]=="text") diagnostics.push_back({{"code","text_preview_unsupported"},{"severity","warning"},{"kind","material"},{"id",id},{"slot",std::stoi(material.key())},{"font",value["font"]},{"message","Material text preview currently requires Windows"}});
#endif
        }
    }
    return diagnostics;
}

std::string currentMissingModelWarning() {
    std::set<int> missing;
    for(const auto &object:document.data["objects"]) {
        int model=object["model"];
        if(!GetObjectDef(model)) missing.insert(model);
    }
    if(missing.empty()) return {};
    std::string warning="Missing model";
    if(missing.size()>1) warning+="s";
    warning+=":";
    for(int model:missing) warning+=" "+std::to_string(model);
    return warning;
}

void invoke(Json r) { try { message.clear(); SampRequest(r.dump()); if(message.empty()) message="Done"; } catch(const std::exception &e) { message=e.what(); } }
int resizePawnInput(ImGuiInputTextCallbackData *data) {
    auto &buffer=*static_cast<std::vector<char>*>(data->UserData);
    buffer.resize(data->BufSize);
    data->Buf=buffer.data();
    return 0;
}
#include "../../tests/samp/renderer_validation.inl"
}

ObjectInst *SampPlace(int model,const rw::V3d &position,const rw::Quat *orientation) {
    auto parsed=samp::ParsePawn("CreateDynamicObject("+std::to_string(model)+",0,0,0,0,0,0);","untitled");
    Json o=parsed.objects[0];
    o["position"]={position.x,position.y,position.z};
    if(orientation) { ObjectInst temporary={}; temporary.m_rotation=*orientation; temporary.UpdateMatrix(); o["rotation"]=angles(&temporary); }
    document.request({{"op","place"},{"object",o}});
    int id=document.data["next_id"].get<int>()-1;
    sync();
    selected=id; if(objects.count(id)) return objects[id]; return nullptr;
}
int SampCut(const std::vector<ObjectInst*> &source) {
    cutRows.clear(); copiedRows.clear(); std::set<int> ids;
    for(auto i:source) {
        auto identity=identities.find(i); if(identity==identities.end()) continue;
        auto original=row(identity->second); if(!original) continue;
        cutRows.push_back({i,*original}); ids.insert(identity->second);
    }
    if(cutRows.empty()) return 0;
    Json next=document.data, kept=Json::array();
    for(const auto &o:next["objects"]) if(!ids.count(o["id"].get<int>())) kept.push_back(o);
    next["objects"]=std::move(kept);
    document.request({{"op","replace"},{"document",next},{"label","Cut objects"}});
    sync(); ClearSelection(); return (int)cutRows.size();
}
int SampCopy(const std::vector<ObjectInst*> &source) {
    cutRows.clear(); copiedRows.clear();
    for(auto i:source) {
        auto identity=identities.find(i); if(identity==identities.end()) continue;
        auto original=row(identity->second); if(original) copiedRows.push_back(*original);
    }
    return (int)copiedRows.size();
}
int SampDelete(const std::vector<ObjectInst*> &source) {
    std::set<int> ids;
    for(auto i:source) {
        auto identity=identities.find(i);
        if(identity!=identities.end() && row(identity->second)) ids.insert(identity->second);
    }
    if(ids.empty()) return 0;
    Json next=document.data, kept=Json::array();
    for(const auto &o:next["objects"]) if(!ids.count(o["id"].get<int>())) kept.push_back(o);
    next["objects"]=std::move(kept);
    document.request({{"op","replace"},{"document",next},{"label","Delete objects"}});
    sync(); ClearSelection(); return (int)ids.size();
}
int SampPaste(const std::vector<ObjectInst*> &source,bool inPlace,bool cut) {
    std::vector<int> created;
    if(cut) {
        if(cutRows.empty()) return 0;
        Json next=document.data; std::set<int> existing;
        for(const auto &o:next["objects"]) existing.insert(o["id"].get<int>());
        for(const auto &entry:cutRows) {
            Json o=entry.second; int id=o["id"].get<int>();
            if(!existing.insert(id).second) {
                auto current=row(id);
                if(!current || *current!=o) { message="Cut source ID now belongs to another object"; cutRows.clear(); return 0; }
                continue;
            }
            next["objects"].push_back(o); created.push_back(id);
        }
        if(!created.empty()) document.request({{"op","replace"},{"document",next},{"label","Paste cut objects"}});
        sync(); ClearSelection();
        for(const auto &entry:cutRows) {
            int id=entry.second["id"].get<int>();
            if(objects.count(id)) objects[id]->Select();
        }
        int count=(int)cutRows.size(); cutRows.clear(); return count;
    }
    Json operations=Json::array();
    for(const auto &original:copiedRows) {
        Json o=original;
        if(!inPlace) o["position"]={original["position"][0].get<double>()+1,original["position"][1],original["position"][2]};
        operations.push_back({{"op","place"},{"object",o}});
    }
    if(operations.empty()) return 0;
    int firstId=document.data["next_id"].get<int>();
    document.request({{"op","patch"},{"operations",operations}}); sync(); ClearSelection();
    for(int n=0;n<(int)operations.size();++n) created.push_back(firstId+n);
    for(int id:created) if(objects.count(id)) objects[id]->Select();
    return (int)created.size();
}

bool SampActive() { return active; }
bool SampOwns(const ObjectInst *i) { return identities.count(const_cast<ObjectInst*>(i))!=0; }
bool SampHidden(const ObjectInst *i) {
    if(!active) return SampOwns(i);
    if(SampOwns(i)) {
        auto r=row(identities[const_cast<ObjectInst*>(i)]); if(!r) return true;
        for(const char *field:{"world","interior"}) { int filter=document.data["preview"].value(field,-1), value=(*r)[field]; if(filter!=-1 && value!=-1 && filter!=value) return true; }
        return false;
    }
    if(i->m_isAdded) return false;
    for(auto &r:document.data["removals"]) {
        if(r["model"]!=-1 && r["model"]!=i->m_objectId) continue;
        auto p=vector(r["position"]); double dx=p.x-i->m_translation.x,dy=p.y-i->m_translation.y,dz=p.z-i->m_translation.z, radius=r["radius"];
        if(dx*dx+dy*dy+dz*dz<=radius*radius) return true;
    }
    return false;
}
void SampApplyMaterials(ObjectInst *i) {
    if(!SampOwns(i) || !i->m_rwObject) return;
    auto r=row(identities[i]); if(!r) return;
    std::string key=(*r)["materials"].dump()+std::to_string((uintptr_t)i->m_rwObject);
    if(rendered[i]==key) return;
    auto obj=GetObjectDef(i->m_objectId);
    TxdPush(); TxdMakeCurrent(obj->m_txdSlot);
    bool applied=true;
    if(obj->m_type==ObjectDef::ATOMIC) applied=applyAtomic((rw::Atomic*)i->m_rwObject,obj->m_atomics[0]->geometry,(*r)["materials"]);
    else {
        auto clump=(rw::Clump*)i->m_rwObject; auto original=obj->m_clump->atomics.link.next;
        FORLIST(lnk,clump->atomics) {
            applied=applyAtomic(rw::Atomic::fromClump(lnk),rw::Atomic::fromClump(original)->geometry,(*r)["materials"]) && applied;
            original=original->next;
        }
    }
    TxdPop();
    if(applied) rendered[i]=key;
}
void SampInvalidateMaterials(ObjectInst *i) { rendered.erase(i); }
void SampTick() {
    if(textureIndexing) indexTextures(4);
    if(!active || applying || ImGuizmo::IsUsing() || ImGui::IsMouseDown(0)) return;
    Json operations=Json::array();
    for(const auto &o:document.data["objects"]) {
        int id=o["id"]; if(!objects.count(id)) continue; auto i=objects[id];
        // A missing preview asset is not a document deletion or viewport edit.
        if(!GetObjectDef(o["model"].get<int>())) continue;
        if(i->m_selected) selected=id;
        if(i->m_isDeleted) { operations.push_back({{"op","delete"},{"id",id}}); continue; }
        Json changes=Json::object();
        auto p=vector(o["position"]); auto q=rotation(o["rotation"]);
        if(fabs(p.x-i->m_translation.x)+fabs(p.y-i->m_translation.y)+fabs(p.z-i->m_translation.z)>0.00001) changes["position"]={i->m_translation.x,i->m_translation.y,i->m_translation.z};
        double dot=q.x*i->m_rotation.x+q.y*i->m_rotation.y+q.z*i->m_rotation.z+q.w*i->m_rotation.w;
        if(fabs(dot)<0.999999) changes["rotation"]=angles(i);
        if(!changes.empty()) operations.push_back({{"op","update"},{"id",id},{"changes",changes}});
    }
    if(!operations.empty()) { document.request({{"op","patch"},{"operations",operations}}); sync(); }
}
void SampUndo(bool redo) { invoke({{"op",redo?"redo":"undo"}}); }
std::string SampSnapshot() { return Json({{"active",active},{"snapshot",document.snapshot()}}).dump(); }
void SampSetWindowVisible(bool visible) { showSampWindow = visible; }
bool SampIsWindowVisible() { return showSampWindow; }
void SampRestore(const std::string &s) { auto j=Json::parse(s); document.restoreSnapshot(j.at("snapshot")); cutRows.clear(); copiedRows.clear(); checkedObjectIds.clear(); active=j.at("active"); sync(); }
std::string SampRequest(const std::string &s) {
    Json r=Json::parse(s); std::string op=r.at("op");
    if(op=="__validate_renderer") {
        const char *enabled=std::getenv("ARIANE_SAMP_VALIDATION");
        if(!enabled || std::strcmp(enabled,"1")!=0 || !AgentBridgeSessionActive())
            throw std::runtime_error("renderer validation requires an enabled development process and session");
        return validateRenderer().dump();
    }
    if((op=="save" || op=="export") && AgentBridgeSessionActive()) throw std::runtime_error("Commit or rollback the agent session before saving/exporting");
    if(op=="window") {
        if(r.contains("show")) {
            if(!r.at("show").is_boolean()) throw std::runtime_error("window show parameter must be a boolean");
            showSampWindow=r.at("show").get<bool>();
        }
        return Json({{"show",showSampWindow},{"revision",document.revision}}).dump();
    }
    if(op=="textures") {
        textureIndexing=true;
        indexTextures(std::max(1,std::min(128,r.value("scan_budget",32))));
        return Json({{"textures",textures(r.value("query",std::string()),r.value("limit",100))},{"scanned_models",textureModelCursor},{"total_models",NUMOBJECTDEFS},{"indexed_dictionaries",textureCursor},{"total_dictionaries",discoveredTxdSlots.size()},{"complete",textureDiscoveryComplete && textureCursor==discoveredTxdSlots.size()}}).dump();
    }
    if(op=="replace" && r.value("validate_only",false)) {
        samp::Document staged=document;
        r.erase("validate_only");
        staged.request(r);
        return Json({{"valid",true},{"revision",document.revision}}).dump();
    }
    auto result=document.request(r);
    if(op=="inspect") {
        result["asset_diagnostics"]=assetDiagnostics();
        result["active"]=active;
    }
    if(op=="open" || op=="clear" || op=="replace" || op=="import") { cutRows.clear(); copiedRows.clear(); checkedObjectIds.clear(); }
    if(op!="inspect" && op!="code" && op!="preview_import" && op!="save" && op!="export" && op!="window") {
        active=op=="replace" && r.contains("active")?r.at("active").get<bool>():true;
        sync();
    }
    return result.dump();
}
void SampDrawWindow() {
    static char path[1024]="map.samp.json", exportPath[1024]="map.pwn", exportGroup[512]="", filter[128]="";
    static std::vector<char> input(65536, '\0');
    static Json results=Json::array();
    if(CPad::IsCtrlDown() && CPad::IsKeyJustDown('M')) showSampWindow = !showSampWindow;

    if(!showSampWindow) {
        ImGui::SetNextWindowSize(ImVec2(180,50),ImGuiCond_Always);
        if(ImGui::Begin("SA-MP Launcher##launcher",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)) {
            if(ImGui::Button("Reopen SA-MP (Ctrl+M)")) showSampWindow=true;
        }
        ImGui::End();
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(650,600),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("SA-MP",&showSampWindow)) { ImGui::End(); return; }
    if(!isSA()) { ImGui::TextUnformatted("SA-MP requires GTA San Andreas."); ImGui::End(); return; }
    ImGui::Checkbox("Enable SA-MP document",&active);
    ImGui::SameLine(); if(ImGui::Button("Undo")) SampUndo(false); ImGui::SameLine(); if(ImGui::Button("Redo")) SampUndo(true);
    ImGui::TextWrapped("%s",message.c_str());
    std::string missingModelWarning=currentMissingModelWarning();
    if(!missingModelWarning.empty()) ImGui::TextWrapped("%s",missingModelWarning.c_str());
    if(ImGui::BeginTabBar("samp-tabs")) {
        if(ImGui::BeginTabItem("Files & Objects")) {
            ImGui::InputText("Path",path,sizeof(path));
#ifdef _WIN32
            ImGui::SameLine(); if(ImGui::Button("Browse##project")) browseFile(path,sizeof(path),false,"SA-MP Project (*.samp.json)\0*.samp.json\0All Files (*.*)\0*.*\0","samp.json");
#endif
            if(ImGui::Button("Open project")) invoke({{"op","open"},{"path",path}});
            ImGui::SameLine(); if(ImGui::Button("Save project")) invoke({{"op","save"},{"path",path}});
            ImGui::InputText("Pawn export path",exportPath,sizeof(exportPath));
#ifdef _WIN32
            ImGui::SameLine(); if(ImGui::Button("Browse##export")) browseFile(exportPath,sizeof(exportPath),true,"Pawn Script (*.pwn)\0*.pwn\0All Files (*.*)\0*.*\0","pwn");
#endif
            const auto &groups = document.data["groups"];
            if(ImGui::BeginCombo("Export group", exportGroup[0] ? exportGroup : "(All groups combined)")) {
                if(ImGui::Selectable("(All groups combined)", exportGroup[0] == '\0')) {
                    exportGroup[0] = '\0';
                }
                for(const auto &g : groups) {
                    std::string gName = g.get<std::string>();
                    bool isSelected = (gName == exportGroup);
                    if(ImGui::Selectable(gName.c_str(), isSelected)) {
                        snprintf(exportGroup, sizeof(exportGroup), "%s", gName.c_str());
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::InputText("Custom export group",exportGroup,sizeof(exportGroup));
            if(ImGui::Button("Export Pawn")) invoke({{"op","export"},{"path",exportPath},{"group",exportGroup}});
            ImGui::SameLine();
            if(ImGui::Button("Copy Pawn to clipboard")) {
                try {
                    std::string code = samp::ExportPawn(document.data, exportGroup);
                    ImGui::SetClipboardText(code.c_str());
                    message = "Copied " + std::to_string(code.size()) + " bytes of Pawn code" + (exportGroup[0] ? (" (group: " + std::string(exportGroup) + ")") : " (combined)") + " to clipboard";
                } catch(const std::exception &e) { message = e.what(); }
            }
            if(groups.size() > 1) {
                ImGui::SameLine();
                if(ImGui::Button("Export all groups to separate files")) {
                    try {
                        std::string basePath = exportPath;
                        if(basePath.size() >= 4 && basePath.substr(basePath.size() - 4) == ".pwn")
                            basePath = basePath.substr(0, basePath.size() - 4);
                        int exportedCount = 0;
                        for(size_t gi = 0; gi < groups.size(); ++gi) {
                            std::string gName = groups[gi].get<std::string>();
                            std::string gFile = basePath + "_G" + std::to_string(gi + 1) + ".pwn";
                            SampRequest(Json({{"op", "export"}, {"path", gFile}, {"group", gName}}).dump());
                            ++exportedCount;
                        }
                        message = "Exported " + std::to_string(exportedCount) + " group files";
                    } catch(const std::exception &e) { message = e.what(); }
                }
            }
            ImGui::TextUnformatted("Pawn code or one file path per line");
            if(ImGui::InputTextMultiline("##pawn-input",input.data(),input.size(),ImVec2(-1,130),ImGuiInputTextFlags_CallbackResize,resizePawnInput,&input)) pending=nullptr;
            if(ImGui::Button("Replace input from clipboard")) {
                const char *clipboard=ImGui::GetClipboardText();
                if(clipboard && clipboard[0]) {
                    size_t length=std::strlen(clipboard);
                    input.resize(std::max((size_t)65536,length+1));
                    std::memcpy(input.data(),clipboard,length+1);
                    message="Pasted "+std::to_string(length)+" bytes into Pawn / file paths";
                    pending=nullptr;
                } else message="Clipboard has no text";
            }
            ImGui::SameLine(); if(ImGui::Button("Clear input")) { input[0]='\0'; pending=nullptr; message="Input cleared"; }
            if(ImGui::Button("Preview pasted Pawn")) {
                try { pending={{"op","import"},{"files",{{{"name","pasted.pwn"},{"source",input.data()}}}}}; message=Json::parse(SampRequest(Json({{"op","preview_import"},{"files",pending["files"]}}).dump())).at("diagnostics").dump(2); } catch(const std::exception &e) { message=e.what(); pending=nullptr; }
            }
            ImGui::SameLine(); if(ImGui::Button("Preview files (one path per line)")) {
                try { Json files=Json::array(); std::istringstream lines(input.data()); std::string line; while(std::getline(lines,line)) if(!line.empty()) files.push_back({{"name",line}});
                    pending={{"op","import"},{"files",files}}; message=Json::parse(SampRequest(Json({{"op","preview_import"},{"files",files}}).dump())).at("diagnostics").dump(2);
                } catch(const std::exception &e) { message=e.what(); pending=nullptr; }
            }
            if(!pending.is_null() && ImGui::Button("Import supported records")) { pending["accept_diagnostics"]=true; invoke(pending); pending=nullptr; }
            int world=document.data["preview"]["world"], interior=document.data["preview"]["interior"];
            bool filterChanged=ImGui::InputInt("Preview world (-1 = all)",&world);
            filterChanged=ImGui::InputInt("Preview interior (-1 = all)",&interior) || filterChanged;
            if(filterChanged) {
                invoke({{"op","preview"},{"world",world},{"interior",interior}});
            }
            if(ImGui::CollapsingHeader("Place object")) {
                static int model=19379; static float position[3]={}, rotation[3]={}; static char group[256]="untitled";
                ImGui::InputInt("New model",&model); ImGui::InputFloat3("New position",position); ImGui::InputFloat3("New rotation",rotation); ImGui::InputText("New group",group,sizeof(group));
                if(ImGui::Button("Use camera target")) { position[0]=TheCamera.m_target.x; position[1]=TheCamera.m_target.y; position[2]=TheCamera.m_target.z; }
                if(ImGui::Button("Place")) invoke({{"op","place"},{"object",{{"model",model},{"position",{position[0],position[1],position[2]}},{"rotation",{rotation[0],rotation[1],rotation[2]}},{"group",group}}}});
            }
            std::set<int> liveObjectIds;
            for(const auto &o:document.data["objects"]) liveObjectIds.insert(o["id"].get<int>());
            for(auto it=checkedObjectIds.begin();it!=checkedObjectIds.end();) {
                if(!liveObjectIds.count(*it)) it=checkedObjectIds.erase(it); else ++it;
            }
            ImGui::Text("Objects: %d checked of %d",(int)checkedObjectIds.size(),(int)document.data["objects"].size());
            ImGui::TextWrapped("Checks choose bulk deletion. Click a row to select and frame it in the viewport.");
            if(ImGui::Button("Select all objects")) {
                checkedObjectIds.clear();
                for(const auto &o:document.data["objects"]) checkedObjectIds.insert(o["id"].get<int>());
            }
            ImGui::SameLine(); if(ImGui::Button("Deselect all")) checkedObjectIds.clear();
            auto deleteObjects=[&](const std::set<int> &ids,const char *label) {
                if(ids.empty()) return;
                Json next=document.data, kept=Json::array();
                for(const auto &o:next["objects"]) if(!ids.count(o["id"].get<int>())) kept.push_back(o);
                if(kept.size()==next["objects"].size()) { checkedObjectIds.clear(); return; }
                int removed=(int)(next["objects"].size()-kept.size());
                next["objects"]=std::move(kept);
                try {
                    SampRequest(Json({{"op","replace"},{"document",next},{"label",label}}).dump());
                    if(!row(selected)) { selected=0; ClearSelection(); }
                    message=std::string(label)+": "+std::to_string(removed)+" object(s) removed; Undo restores them";
                } catch(const std::exception &e) { message=e.what(); }
            };
            static std::set<int> deleteIds;
            static unsigned deleteRevision=0;
            static std::string deleteLabel;
            auto confirmDelete=[&](const std::set<int> &ids,const char *label) {
                deleteIds=ids; deleteRevision=document.revision; deleteLabel=label;
                ImGui::OpenPopup("Confirm object deletion");
            };
            ImGui::BeginDisabled(checkedObjectIds.empty());
            if(ImGui::Button("Delete checked")) confirmDelete(checkedObjectIds,"Delete checked objects");
            ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::BeginDisabled(document.data["objects"].empty());
            if(ImGui::Button("Delete all objects")) {
                std::set<int> all;
                for(const auto &o:document.data["objects"]) all.insert(o["id"].get<int>());
                confirmDelete(all,"Delete all objects");
            }
            ImGui::EndDisabled();
            if(ImGui::BeginPopupModal("Confirm object deletion",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete %d object(s)?",(int)deleteIds.size());
                ImGui::TextUnformatted("Removals and project settings stay. One Undo restores the objects.");
                bool stale=deleteRevision!=document.revision;
                if(stale) ImGui::TextUnformatted("Document changed. Cancel and review the objects again.");
                ImGui::BeginDisabled(stale);
                if(ImGui::Button("Delete objects")) { deleteObjects(deleteIds,deleteLabel.c_str()); ImGui::CloseCurrentPopup(); }
                ImGui::EndDisabled(); ImGui::SameLine();
                if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }
            ImGui::BeginChild("object-list",ImVec2(0,180),true);
            ImGuiListClipper clipper;
            clipper.Begin((int)document.data["objects"].size(),ImGui::GetFrameHeightWithSpacing());
            while(clipper.Step()) for(int index=clipper.DisplayStart;index<clipper.DisplayEnd;++index) {
                auto &o=document.data["objects"][index];
                int id=o["id"], model=o["model"];
                ImGui::PushID(id);
                bool checked=checkedObjectIds.count(id)!=0;
                if(ImGui::Checkbox("##checked",&checked)) {
                    if(checked) checkedObjectIds.insert(id); else checkedObjectIds.erase(id);
                }
                ImGui::SameLine();
                std::string label=std::to_string(id)+": model "+std::to_string(model);
                if(ImGui::Selectable(label.c_str(),selected==id)) { selected=id; if(objects.count(id)) { ClearSelection(); objects[id]->Select(); objects[id]->JumpTo(); } }
                ImGui::PopID();
            }
            ImGui::EndChild();
            if(row(selected)) {
                static int edited=-1; static unsigned editRevision=0; static float pos[3],rot[3],stream,draw; static int world,interior;
                if(edited!=selected || editRevision!=document.revision) {
                    auto &o=*row(selected); edited=selected; editRevision=document.revision;
                    for(int n=0;n<3;++n) { pos[n]=o["position"][n]; rot[n]=o["rotation"][n]; }
                    world=o["world"]; interior=o["interior"]; stream=o["stream"]; draw=o["draw"];
                }
                ImGui::InputFloat3("Position",pos); ImGui::InputFloat3("Rotation",rot); ImGui::InputInt("World",&world); ImGui::InputInt("Interior",&interior);
                ImGui::InputFloat("Stream distance",&stream); ImGui::InputFloat("Draw distance",&draw);
                if(ImGui::Button("Apply object properties")) invoke({{"op","update"},{"id",selected},{"changes",{{"position",{pos[0],pos[1],pos[2]}},{"rotation",{rot[0],rot[1],rot[2]}},{"world",world},{"interior",interior},{"stream",stream},{"draw",draw}}}});
                if(ImGui::Button("Duplicate")) invoke({{"op","duplicate"},{"id",selected}});
                ImGui::SameLine(); if(ImGui::Button("Delete")) invoke({{"op","delete"},{"id",selected}});
            }
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Textures")) {
            ImGui::InputInt("Material slot",&slot); slot=std::max(0,std::min(15,slot));
            ImGui::InputText("Texture / TXD / model",filter,sizeof(filter));
            if(ImGui::Button("Index / search textures")) { textureIndexing=true; indexTextures(16); }
            ImGui::SameLine();
            if(textureDiscoveryComplete && textureCursor==discoveredTxdSlots.size()) {
                ImGui::TextColored(ImVec4(0.4f,1.0f,0.4f,1.0f),"Indexing complete (%d textures, %d TXDs)",(int)textureIndex.size(),(int)discoveredTxdSlots.size());
            } else {
                ImGui::Text("Indexing: %d / %d models; %d / %d TXDs",(int)textureModelCursor,NUMOBJECTDEFS,(int)textureCursor,(int)discoveredTxdSlots.size());
            }
            static char lastFilter[128] = "ÿ";
            static size_t lastIndexedCount = 0;
            static size_t lastModelCursor = 0;
            if(std::strcmp(filter, lastFilter) != 0 || textureIndex.size() != lastIndexedCount || textureModelCursor != lastModelCursor) {
                results=textures(filter,300);
                std::strncpy(lastFilter, filter, sizeof(lastFilter) - 1);
                lastFilter[sizeof(lastFilter) - 1] = '\0';
                lastIndexedCount = textureIndex.size();
                lastModelCursor = textureModelCursor;
            }
            static char tint[16]="00000000"; ImGui::InputText("ARGB tint (hex)",tint,sizeof(tint),ImGuiInputTextFlags_CharsHexadecimal);
            for(size_t n=0;n<results.size();++n) {
                auto &r=results[n]; ImGui::PushID((int)n);
                if(auto tex=lookup(r, true)) ImGui::Image((void*)tex,ImVec2(64,64));
                int mId = r.value("model", -1);
                std::string label=(mId >= 0 ? ("[" + std::to_string(mId) + "] ") : "") + r["txd"].get<std::string>()+" / "+r["texture"].get<std::string>();
                if(ImGui::Button(label.c_str()) && row(selected)) { Json m=r; m["type"]="texture"; m["color"]=(uint32_t)strtoul(tint,nullptr,16); invoke({{"op","material"},{"id",selected},{"slot",slot},{"material",m}}); }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Material Text")) {
            static char text[4096]="Your text", font[128]="Arial", fg[16]="FFFFFFFF", bg[16]="00000000";
            static int size=90, fontSize=24, align=0; static bool bold=true;
            static int loadedId=-1, loadedSlot=-1;
            static unsigned loadedRevision=~0u;
            if(loadedId!=selected || loadedSlot!=slot || loadedRevision!=document.revision) {
                loadedId=selected; loadedSlot=slot; loadedRevision=document.revision; auto o=row(selected);
                if(o && (*o)["materials"].contains(std::to_string(slot))) {
                    auto m=(*o)["materials"][std::to_string(slot)];
                    if(m["type"]=="text") {
                        snprintf(text,sizeof(text),"%s",m["text"].get<std::string>().c_str()); snprintf(font,sizeof(font),"%s",m["font"].get<std::string>().c_str());
                        size=m["size"]; fontSize=m["font_size"]; align=m["align"]; bold=m["bold"];
                        snprintf(fg,sizeof(fg),"%08X",m["foreground"].get<uint32_t>()); snprintf(bg,sizeof(bg),"%08X",m["background"].get<uint32_t>());
                    }
                }
            }
            ImGui::InputInt("Slot",&slot); ImGui::InputTextMultiline("Text",text,sizeof(text),ImVec2(-1,100));
            ImGui::InputText("Font",font,sizeof(font)); ImGui::InputInt("Material size (10..140)",&size); ImGui::InputInt("Font size",&fontSize);
            ImGui::Checkbox("Bold",&bold); ImGui::Combo("Alignment",&align,"Left\0Center\0Right\0");
            ImGui::InputText("Foreground ARGB",fg,sizeof(fg),ImGuiInputTextFlags_CharsHexadecimal); ImGui::InputText("Background ARGB",bg,sizeof(bg),ImGuiInputTextFlags_CharsHexadecimal);
            if(size>=10 && size<=140 && size%10==0 && fontSize>=1 && fontSize<=255) {
                Json preview={{"type","text"},{"text",text},{"font",font},{"size",size},{"font_size",fontSize},{"bold",bold},{"foreground",(uint32_t)strtoul(fg,nullptr,16)},{"background",(uint32_t)strtoul(bg,nullptr,16)},{"align",align}};
                if(auto texture=textTexture(preview)) ImGui::Image((void*)texture,ImVec2(256,128));
            }
            if(ImGui::Button("Apply text")) invoke({{"op","material"},{"id",selected},{"slot",slot},{"material",{{"type","text"},{"text",text},{"font",font},{"size",size},{"font_size",fontSize},{"bold",bold},{"foreground",(uint32_t)strtoul(fg,nullptr,16)},{"background",(uint32_t)strtoul(bg,nullptr,16)},{"align",align}}}});
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Removals")) {
            static int model=-1, editing=0; static float xyz[3]={}, radius=0.1f;
            ImGui::InputInt("Model (-1 = all)",&model); ImGui::InputFloat3("Center",xyz); ImGui::InputFloat("Radius",&radius);
            if(ImGui::Button("Use selected world object")) for(CPtrNode *p=selection.first;p;p=p->next) { auto i=(ObjectInst*)p->item; if(!SampOwns(i)) { model=i->m_objectId; xyz[0]=i->m_translation.x; xyz[1]=i->m_translation.y; xyz[2]=i->m_translation.z; break; } }
            if(ImGui::Button(editing?"Update removal":"Add removal")) {
                Json removal={{"model",model},{"position",{xyz[0],xyz[1],xyz[2]}},{"radius",radius}};
                if(editing) invoke({{"op","update"},{"kind","removals"},{"id",editing},{"changes",removal}});
                else invoke({{"op","removal"},{"removal",removal}});
            }
            ImGui::SameLine(); if(ImGui::Button("New removal")) editing=0;
            for(auto &r:document.data["removals"]) { int id=r["id"]; ImGui::PushID(id); ImGui::Text("%d: model %d, radius %.3f",id,r["model"].get<int>(),r["radius"].get<float>()); ImGui::SameLine(); if(ImGui::Button("Edit")) { editing=id; model=r["model"]; radius=r["radius"]; for(int k=0;k<3;++k) xyz[k]=r["position"][k]; }
                ImGui::SameLine(); if(ImGui::Button("Restore")) { editing=0; invoke({{"op","delete"},{"kind","removals"},{"id",id}}); ImGui::PopID(); break; } ImGui::PopID(); }
            int affected=0;
            if(radius>=0) for(CPtrNode *p=instances.first;p;p=p->next) {
                auto i=(ObjectInst*)p->item; if(SampOwns(i) || i->m_isAdded || (model!=-1 && model!=i->m_objectId)) continue;
                double dx=xyz[0]-i->m_translation.x,dy=xyz[1]-i->m_translation.y,dz=xyz[2]-i->m_translation.z;
                if(dx*dx+dy*dy+dz*dz<=double(radius)*radius) ++affected;
            }
            ImGui::Text("Current removal preview: %d world instances",affected);
            ImGui::TextUnformatted("Select a world object in the viewport, then use it above.");
            ImGui::TextUnformatted("LOD removals are explicit. No world files are changed."); ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Code & History")) {
            static char codeGroup[512] = "";
            const auto &groups = document.data["groups"];
            if(ImGui::BeginCombo("Group filter", codeGroup[0] ? codeGroup : "(All groups combined)")) {
                if(ImGui::Selectable("(All groups combined)", codeGroup[0] == '\0')) {
                    codeGroup[0] = '\0';
                }
                for(const auto &g : groups) {
                    std::string gName = g.get<std::string>();
                    bool isSelected = (gName == codeGroup);
                    if(ImGui::Selectable(gName.c_str(), isSelected)) {
                        snprintf(codeGroup, sizeof(codeGroup), "%s", gName.c_str());
                    }
                }
                ImGui::EndCombo();
            }
            std::string code;
            try { code = samp::ExportPawn(document.data, codeGroup); } catch(const std::exception &e) { code = "// Error: " + std::string(e.what()); }
            if(ImGui::Button("Copy current code")) {
                ImGui::SetClipboardText(code.c_str());
                message = "Copied " + std::to_string(code.size()) + " bytes of current code to clipboard";
            }
            ImGui::SameLine();
            ImGui::Text("%d characters, revision %u", (int)code.size(), document.revision);
            ImGui::BeginChild("code",ImVec2(0,250),true,ImGuiWindowFlags_HorizontalScrollbar); ImGui::TextUnformatted(code.c_str()); ImGui::EndChild();
            for(size_t n=0;n<document.history.size();++n) {
                auto &h=document.history[n];
                std::string label=std::to_string(n+1)+" "+h["label"].get<std::string>();
                if(ImGui::TreeNode(label.c_str())) {
                    ImGui::TextUnformatted("Historical state, not an incremental patch");
                    if(h.contains("affected") && h["affected"].is_array()) {
                        for(const auto &record:h["affected"])
                            ImGui::BulletText("%s #%d",record["kind"].get<std::string>().c_str(),record["id"].get<int>());
                    }
                    ImGui::PushID((int)n);
                    if(ImGui::Button("Copy snapshot code")) {
                        std::string snapCode = h["code"].get<std::string>();
                        ImGui::SetClipboardText(snapCode.c_str());
                        message = "Copied historical snapshot #" + std::to_string(n + 1) + " (" + std::to_string(snapCode.size()) + " bytes) to clipboard";
                    }
                    ImGui::PopID();
                    ImGui::TextUnformatted(h["code"].get<std::string>().c_str());
                    ImGui::TreePop();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
