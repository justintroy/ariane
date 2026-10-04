#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "samp_document.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace samp {
namespace {
struct Token { std::string s; int line; bool string = false; };
struct LexError : std::runtime_error {
    int line;
    LexError(int sourceLine,const std::string &message) : std::runtime_error(message), line(sourceLine) {}
};
std::vector<Token> lex(const std::string &s) {
    std::vector<Token> out;
    int line = 1;
    for(size_t i=0; i<s.size();) {
        char c=s[i];
        if(c=='\n') { ++line; ++i; continue; }
        if(isspace((unsigned char)c)) { ++i; continue; }
        if(s.compare(i,2,"//")==0) { while(i<s.size() && s[i]!='\n') ++i; continue; }
        if(s.compare(i,2,"/*")==0) {
            int startLine=line;
            i+=2; while(i<s.size() && s.compare(i,2,"*/")) { if(s[i++]=='\n') ++line; }
            if(i==s.size()) throw LexError(startLine,"unterminated comment");
            i+=2; continue;
        }
        Token t{"",line,false};
        if(c=='"') {
            int startLine=line;
            t.string=true; ++i; bool closed=false;
            while(i<s.size()) {
                c=s[i++]; if(c=='"') { closed=true; break; }
                if(c=='\\') {
                    if(i==s.size()) break;
                    c=s[i++];
                    if(c=='\r' && i<s.size() && s[i]=='\n') ++i;
                    if(c=='\n' || c=='\r') { ++line; continue; }
                    if(c=='n') c='\n'; else if(c=='r') c='\r'; else if(c=='t') c='\t';
                    else if(c!='"' && c!='\\' && c!='\'') throw LexError(startLine,"unsupported Pawn string escape");
                }
                if(c=='\n') ++line;
                t.s+=c;
            }
            if(!closed) throw LexError(startLine,"unterminated string");
        } else if(isalnum((unsigned char)c) || c=='_' || c=='.') {
            do { t.s+=s[i++]; } while(i<s.size() && (isalnum((unsigned char)s[i]) || s[i]=='_' || s[i]=='.' ||
                ((s[i]=='+' || s[i]=='-') && (isdigit((unsigned char)t.s[0]) || t.s[0]=='.') && t.s.find_first_of("xX")==std::string::npos && (t.s.back()=='e' || t.s.back()=='E'))));
        } else {
            t.s+=s[i++];
            if(i<s.size() && (t.s=="<" || t.s==">") && s[i]==t.s[0]) t.s+=s[i++];
        }
        out.push_back(t);
    }
    return out;
}
using Tokens=std::vector<Token>;
int64_t checkedInteger(double value, double minimum, double maximum) {
    if(!std::isfinite(value) || value!=std::floor(value) || value<minimum || value>maximum)
        throw std::runtime_error("integer outside supported range");
    return static_cast<int64_t>(value);
}
uint32_t pawnBits(double value) {
    return static_cast<uint32_t>(checkedInteger(value,-2147483648.,4294967295.));
}
struct Expression {
    const Tokens &t; const std::map<std::string,double> &constants; size_t i=0;
    static int precedence(const std::string &s) {
        if(s=="|") return 1; if(s=="^") return 2; if(s=="&") return 3;
        if(s=="<<" || s==">>") return 4; if(s=="+" || s=="-") return 5;
        if(s=="*" || s=="/" || s=="%") return 6; return 0;
    }
    double atom() {
        if(i==t.size()) throw std::runtime_error("missing expression");
        std::string s=t[i++].s;
        if(s=="-" || s=="+" || s=="~") { double v=atom(); return s=="-"?-v:s=="~"?static_cast<double>(static_cast<uint32_t>(~pawnBits(v))):v; }
        if(s=="(") { double v=expr(1); if(i==t.size() || t[i++].s!=")") throw std::runtime_error("missing )"); return v; }
        if(i<t.size() && t[i].s==":") { ++i; return atom(); }
        auto it=constants.find(s); if(it!=constants.end()) return it->second;
        size_t used=0; double v;
        try { v=s.compare(0,2,"0x")==0 || s.compare(0,2,"0X")==0 ? double(std::stoull(s,&used,16)):std::stod(s,&used); }
        catch(...) { throw std::runtime_error("unresolved expression: "+s); }
        if(used!=s.size() || !std::isfinite(v)) throw std::runtime_error("invalid number: "+s);
        return v;
    }
    double expr(int minp) {
        double a=atom();
        while(i<t.size()) {
            std::string op=t[i].s; int p=precedence(op); if(p<minp) break;
            ++i; double b=expr(p+1);
            if(op=="+") a+=b; else if(op=="-") a-=b; else if(op=="*") a*=b;
            else if(op=="/" || op=="%") { if(b==0) throw std::runtime_error("division by zero"); a=op=="/"?a/b:std::fmod(a,b); }
            else if(op=="|") a=double(pawnBits(a) | pawnBits(b));
            else if(op=="&") a=double(pawnBits(a) & pawnBits(b));
            else if(op=="^") a=double(pawnBits(a) ^ pawnBits(b));
            else {
                int shift=static_cast<int>(checkedInteger(b,0,31));
                a=op=="<<"?double(pawnBits(a) << shift):double(pawnBits(a) >> shift);
            }
        }
        return a;
    }
    double value() { double v=expr(1); if(i!=t.size() || !std::isfinite(v)) throw std::runtime_error("unsupported expression"); return v; }
};
std::string quote(const std::string &s) {
    std::string r="\"";
    for(char c:s) { if(c=='\\' || c=='"') { r+='\\'; r+=c; } else if(c=='\n') r+="\\n"; else if(c=='\r') r+="\\r"; else if(c=='\t') r+="\\t"; else r+=c; }
    return r+'"';
}
std::string num(double n) { std::ostringstream s; s<<std::setprecision(10)<<n; std::string r=s.str(); if(r.find_first_of(".eE")==std::string::npos) r+=".0"; return r; }
std::string color(const Json &j) { std::ostringstream s; s<<"0x"<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<j.get<uint32_t>(); return s.str(); }
std::string sourceComment(const std::string &source) {
    std::string out="// Source: ";
    for(unsigned char c:source) out+=c<32 || c==127?'?':static_cast<char>(c);
    return out;
}
Json readJson(const std::string &path) { std::ifstream f(path); if(!f) throw std::runtime_error("cannot open "+path); Json j; f>>j; return j; }
void writeFile(const std::string &path,const std::string &text) {
    std::string temporary=path+".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
        if(!f) throw std::runtime_error("cannot write "+path);
        f<<text; f.flush(); if(!f) throw std::runtime_error("write failed: "+path); f.close();
#ifdef _WIN32
        if(!MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("cannot replace "+path);
#else
        if(std::rename(temporary.c_str(),path.c_str())!=0) throw std::runtime_error("cannot replace "+path);
#endif
    } catch(...) { std::remove(temporary.c_str()); throw; }
}
void requireExtension(const std::string &path,const std::string &extension) {
    if(path.size()<extension.size() || path.substr(path.size()-extension.size())!=extension) throw std::runtime_error("expected "+extension+" path");
}
int allocateId(Json &document) {
    int id=document.at("next_id").get<int>();
    if(id>=2147483647) throw std::runtime_error("SA-MP ID space exhausted");
    document["next_id"]=id+1;
    return id;
}
Json changedRecords(const Json &before,const Json &after) {
    Json changed=Json::array();
    for(const char *kind:{"objects","removals"}) {
        std::map<int,Json> oldRecords, newRecords;
        for(const auto &row:before.at(kind)) oldRecords[row.at("id").get<int>()]=row;
        for(const auto &row:after.at(kind)) newRecords[row.at("id").get<int>()]=row;
        std::set<int> ids;
        for(const auto &entry:oldRecords) ids.insert(entry.first);
        for(const auto &entry:newRecords) ids.insert(entry.first);
        for(int id:ids) {
            auto old=oldRecords.find(id), next=newRecords.find(id);
            Json left=old==oldRecords.end()?Json():old->second;
            Json right=next==newRecords.end()?Json():next->second;
            if(left!=right) changed.push_back({{"kind",kind},{"id",id},{"before",left},{"after",right}});
        }
    }
    return changed;
}
}

ImportResult ParsePawn(const std::string &source,const std::string &file) {
    ImportResult result; Tokens t;
    auto diagnostic=[&](int line,const std::string &message) { result.diagnostics.push_back({{"file",file},{"line",line},{"message",message}}); };
    try { t=lex(source); } catch(const LexError &e) { diagnostic(e.line,e.what()); return result; }
    std::map<std::string,double> constants={{"true",1},{"false",0},{"STREAMER_OBJECT_SD",300},{"STREAMER_OBJECT_DD",0},{"OBJECT_MATERIAL_TEXT_ALIGN_LEFT",0},{"OBJECT_MATERIAL_TEXT_ALIGN_CENTER",1},{"OBJECT_MATERIAL_TEXT_ALIGN_RIGHT",2}};
    const char *sizes[]={"32x32","64x32","64x64","128x32","128x64","128x128","256x32","256x64","256x128","256x256","512x64","512x128","512x256","512x512"};
    for(int n=0;n<14;++n) constants[std::string("OBJECT_MATERIAL_SIZE_")+sizes[n]]=10*(n+1);
    std::map<std::string,size_t> refs;
    auto number=[&](const Tokens &a) { return Expression{a,constants}.value(); };
    auto reference=[&](const Tokens &a) {
        if(a.empty() || a[0].string || !(isalpha((unsigned char)a[0].s[0]) || a[0].s[0]=='_')) throw std::runtime_error("invalid object reference");
        std::string key=a[0].s;
        for(size_t k=1;k<a.size();) {
            if(a[k++].s!="[") throw std::runtime_error("unsupported object reference");
            size_t start=k; while(k<a.size() && a[k].s!="]") ++k;
            if(k==a.size()) throw std::runtime_error("missing array ]");
            double index=number(Tokens(a.begin()+start,a.begin()+k++));
            if(index!=std::floor(index)) throw std::runtime_error("noninteger array index");
            key+="["+std::to_string(checkedInteger(index,0,2147483647.))+"]";
        }
        return key;
    };
    std::set<std::string> calls={"CreateObject","CreateDynamicObject","SetObjectMaterial","SetDynamicObjectMaterial","SetObjectMaterialText","SetDynamicObjectMaterialText","RemoveBuildingForPlayer"};
    size_t statement=0;
    int braceDepth=0;
    std::vector<std::map<std::string,double>> constantScopes;
    for(size_t i=0;i<t.size();) {
        if(t[i].s=="#") {
            size_t end=i+1; while(end<t.size() && t[end].line==t[i].line) ++end;
            if(i+3<end && t[i+1].s=="define") { try { constants[t[i+2].s]=number(Tokens(t.begin()+i+3,t.begin()+end)); } catch(const std::exception &e) { diagnostic(t[i].line,e.what()); } }
            else if(i+1<end && t[i+1].s!="include" && t[i+1].s!="pragma") { diagnostic(t[i].line,"unsupported preprocessor directive; remaining source skipped"); break; }
            i=end; statement=i; continue;
        }
        if(t[i].s=="if" || t[i].s=="for" || t[i].s=="while" || t[i].s=="switch" || t[i].s=="do") {
            diagnostic(t[i].line,"runtime control flow unsupported; remaining source skipped"); break;
        }
        if(t[i].s=="const" && i+3<t.size()) {
            size_t eq=i+1,end=i+1; while(end<t.size() && t[end].s!=";") ++end;
            while(eq<end && t[eq].s!="=") ++eq;
            if(eq<end) try { constants[t[eq-1].s]=number(Tokens(t.begin()+eq+1,t.begin()+end)); } catch(const std::exception &e) { diagnostic(t[i].line,e.what()); }
            i=end; continue;
        }
        if(t[i].s=="=" && i+1<t.size() && !calls.count(t[i+1].s)) {
            size_t start=statement, end=i+1;
            if(start<i && t[start].s=="new") ++start;
            if(start+1<i && t[start+1].s==":") start+=2;
            while(end<t.size() && t[end].s!=";") ++end;
            try {
                auto key=reference(Tokens(t.begin()+start,t.begin()+i));
                refs.erase(key);
                try {
                    auto sourceKey=reference(Tokens(t.begin()+i+1,t.begin()+end));
                    auto found=refs.find(sourceKey); if(found!=refs.end()) refs[key]=found->second;
                } catch(...) {}
            } catch(...) {}
        }
        if(!calls.count(t[i].s)) {
            if(t[i].s=="{") {
                if(braceDepth==0) refs.clear();
                constantScopes.push_back(constants);
                ++braceDepth;
            } else if(t[i].s=="}") {
                if(braceDepth>0) {
                    constants=constantScopes.back(); constantScopes.pop_back();
                    if(--braceDepth==0) refs.clear();
                } else diagnostic(t[i].line,"unmatched function block close");
            }
            if(t[i].s==";" || t[i].s=="{" || t[i].s=="}") statement=i+1;
            if(i+1<t.size() && t[i+1].s=="(" && t[i].s.compare(0,5,"SAMP_")!=0 && (t[i].s.find("Object")!=std::string::npos || t[i].s.find("Building")!=std::string::npos)) diagnostic(t[i].line,"unsupported map call: "+t[i].s);
            ++i; continue;
        }
        std::string name=t[i].s; int line=t[i].line; size_t call=i++;
        if(i==t.size() || t[i++].s!="(") { diagnostic(line,"expected ("); continue; }
        std::vector<Tokens> args(1); int depth=1;
        for(;i<t.size();++i) {
            if(!t[i].string && t[i].s=="(") ++depth;
            if(!t[i].string && t[i].s==")" && --depth==0) { ++i; break; }
            else if(!t[i].string && t[i].s==")") {} // depth already decreased
            if(!t[i].string && t[i].s=="," && depth==1) args.emplace_back(); else args.back().push_back(t[i]);
        }
        try {
            if(depth) throw std::runtime_error("unterminated call");
            if(i==t.size() || t[i].s!=";") throw std::runtime_error("expected ; after map call");
            auto n=[&](size_t a,double def) { return a<args.size()?number(args[a]):def; };
            auto str=[&](size_t a,const std::string &def) { if(a>=args.size()) return def; if(args[a].size()!=1 || !args[a][0].string) throw std::runtime_error("expected literal string"); return args[a][0].s; };
            if(name=="CreateObject" || name=="CreateDynamicObject") {
                bool dynamic=name=="CreateDynamicObject";
                if(args.size()<7 || args.size()>(dynamic?14u:8u)) throw std::runtime_error("invalid creation argument count");
                Json o={{"id",result.objects.size()+1},{"group",file},{"model",n(0,0)},{"position",{n(1,0),n(2,0),n(3,0)}},{"rotation",{n(4,0),n(5,0),n(6,0)}},{"world",dynamic?n(7,-1):-1},{"interior",dynamic?n(8,-1):-1},{"player",dynamic?n(9,-1):-1},{"stream",dynamic?n(10,300):300},{"draw",dynamic?n(11,0):n(7,0)},{"area",dynamic?n(12,-1):-1},{"priority",dynamic?n(13,0):0},{"materials",Json::object()}};
                if(call>statement && t[call-1].s=="=") {
                    size_t start=statement; if(start<call && t[start].s=="new") ++start;
                    if(start+1<call && t[start+1].s==":") start+=2;
                    refs[reference(Tokens(t.begin()+start,t.begin()+call-1))]=result.objects.size();
                }
                result.objects.push_back(o);
            } else if(name=="RemoveBuildingForPlayer") {
                if(args.size()!=6) throw std::runtime_error("removal requires six arguments");
                result.removals.push_back({{"id",result.removals.size()+1},{"group",file},{"model",n(1,0)},{"position",{n(2,0),n(3,0),n(4,0)}},{"radius",n(5,0)}});
            } else {
                if(args.size()<(name=="SetObjectMaterialText"?2u:3u)) throw std::runtime_error("missing material arguments");
                auto it=refs.find(reference(args[0])); if(it==refs.end()) throw std::runtime_error("unresolved object reference");
                bool text=name.find("Text")!=std::string::npos, native=name=="SetObjectMaterialText";
                int slot=static_cast<int>(checkedInteger(n(native?2:1,0),0,15));
                Json m;
                if(text) {
                    if(args.size()>10) throw std::runtime_error("runtime formatted text unsupported");
                    m={{"type","text"},{"text",str(native?1:2,"")},{"size",n(3,90)},{"font",str(4,"Arial")},{"font_size",n(5,24)},{"bold",n(6,1)!=0},{"foreground",pawnBits(n(7,4294967295.))},{"background",pawnBits(n(8,0))},{"align",n(9,0)}};
                } else {
                    if(args.size()<5 || args.size()>6) throw std::runtime_error("invalid material argument count");
                    m={{"type","texture"},{"model",n(2,0)},{"txd",str(3,"")},{"texture",str(4,"")},{"color",pawnBits(n(5,0))}};
                }
                result.objects[it->second]["materials"][std::to_string(slot)]=m;
            }
        } catch(const std::exception &e) { diagnostic(line,e.what()); }
    }
    if(braceDepth!=0) diagnostic(t.empty()?1:t.back().line,"unbalanced function block");
    return result;
}

Json EmptyDocument() { return {{"version",1},{"next_id",1},{"objects",Json::array()},{"removals",Json::array()},{"groups",Json::array()},{"asset_paths",Json::array()},{"preview",{{"world",-1},{"interior",-1}}}}; }
void ValidateDocument(const Json &d) {
    if(d.at("version")!=1 || !d.at("objects").is_array() || !d.at("removals").is_array()) throw std::runtime_error("unsupported SA-MP project");
    if(!d.at("groups").is_array() || !d.at("asset_paths").is_array() || !d.at("preview").is_object()) throw std::runtime_error("invalid project metadata");
    std::set<std::string> groups;
    for(const auto &group:d.at("groups")) {
        if(!group.is_string() || !groups.insert(group.get<std::string>()).second) throw std::runtime_error("invalid or duplicate group");
    }
    for(const auto &path:d.at("asset_paths")) if(!path.is_string()) throw std::runtime_error("invalid asset path");
    auto integer=[](const Json &v,double low,double high) { if(!v.is_number() || !std::isfinite(v.get<double>()) || v.get<double>()!=std::floor(v.get<double>()) || v.get<double>()<low || v.get<double>()>high) throw std::runtime_error("integer outside supported range"); };
    integer(d.at("next_id"),1,2147483647);
    for(const char *field:{"world","interior"}) integer(d.at("preview").at(field),-1,2147483647);
    std::set<int> ids;
    for(const char *kind:{"objects","removals"}) for(const auto &o:d.at(kind)) {
        integer(o.at("id"),1,2147483646); integer(o.at("model"),-1,39999);
        int id=o.at("id").get<int>(); if(id<=0 || !ids.insert(id).second) throw std::runtime_error("duplicate or invalid ID");
        if(id>=d.at("next_id").get<int>() || !o.at("group").is_string() || !groups.count(o.at("group").get<std::string>())) throw std::runtime_error("invalid ID sequence or group");
        const auto &p=o.at("position"); if(!p.is_array() || p.size()!=3) throw std::runtime_error("position requires XYZ");
        for(auto &v:p) if(!v.is_number() || !std::isfinite(v.get<double>())) throw std::runtime_error("invalid position");
        if(!o.at("model").is_number() || o.at("model").get<double>()!=o.at("model").get<int>()) throw std::runtime_error("invalid model");
        if(std::string(kind)=="removals") { if(!o.at("radius").is_number() || !std::isfinite(o.at("radius").get<double>()) || o.at("radius").get<double>()<0) throw std::runtime_error("invalid removal radius"); continue; }
        integer(o.at("model"),0,39999);
        for(const char *field:{"world","interior","player","area","priority"}) integer(o.at(field),-2147483648.,2147483647.);
        for(const char *field:{"stream","draw"}) if(!o.at(field).is_number() || !std::isfinite(o.at(field).get<double>()) || o.at(field).get<double>()<0) throw std::runtime_error("invalid distance");
        if(o.at("rotation").size()!=3) throw std::runtime_error("rotation requires XYZ");
        for(auto &v:o.at("rotation")) if(!v.is_number() || !std::isfinite(v.get<double>())) throw std::runtime_error("invalid rotation");
        if(!o.at("materials").is_object()) throw std::runtime_error("invalid materials");
        for(auto it=o.at("materials").begin();it!=o.at("materials").end();++it) {
            int slot=std::stoi(it.key()); if(slot<0 || slot>15 || std::to_string(slot)!=it.key()) throw std::runtime_error("material slot outside 0..15");
            auto &m=it.value(); if(m.at("type")=="text") {
                if(!m.at("text").is_string() || !m.at("font").is_string() || !m.at("bold").is_boolean()) throw std::runtime_error("invalid text parameters");
                integer(m.at("size"),10,140); integer(m.at("font_size"),1,255); integer(m.at("align"),0,2);
                integer(m.at("foreground"),0,4294967295.); integer(m.at("background"),0,4294967295.);
                int size=m.at("size"), align=m.at("align"), fs=m.at("font_size");
                if(size<10 || size>140 || size%10 || align<0 || align>2 || fs<1 || fs>255) throw std::runtime_error("invalid text settings");
            } else if(m.at("type")=="texture") {
                integer(m.at("model"),-1,39999); integer(m.at("color"),0,4294967295.);
                if(!m.at("txd").is_string() || !m.at("texture").is_string()) throw std::runtime_error("invalid texture name");
            } else throw std::runtime_error("invalid material type");
        }
    }
}
std::string ExportPawn(const Json &d,const std::string &group) {
    ValidateDocument(d); std::ostringstream s;
    std::string suffix;
    if(!group.empty()) {
        auto it=std::find(d.at("groups").begin(),d.at("groups").end(),group);
        if(it==d.at("groups").end()) throw std::runtime_error("unknown export group");
        suffix="_G"+std::to_string(std::distance(d.at("groups").begin(),it)+1);
    }
    std::string create="SAMP_CreateMap"+suffix, remove="SAMP_RemoveBuildings"+suffix;
    s<<"// Requires streamer.inc. Call "<<create<<"() from OnGameModeInit.\n// Call "<<remove<<"(playerid) from OnPlayerConnect.\n";
    if(!group.empty()) s<<"// Suffix follows the group's project index.\n";
    s<<"\nstock "<<create<<"()\n{\n";
    for(const auto &o:d.at("objects")) {
        if(!group.empty() && o.at("group")!=group) continue;
        s<<"    "<<sourceComment(o.at("group").get<std::string>())<<"\n";
        std::string id="samp_"+std::to_string(o.at("id").get<int>());
        s<<"    new "<<id<<" = CreateDynamicObject("<<o.at("model").get<int>();
        for(auto &v:o.at("position")) s<<", "<<num(v.get<double>());
        for(auto &v:o.at("rotation")) s<<", "<<num(v.get<double>());
        for(const char *key:{"world","interior","player"}) s<<", "<<o.at(key).get<int>();
        s<<", "<<num(o.at("stream"))<<", "<<num(o.at("draw"))<<", "<<o.at("area").get<int>()<<", "<<o.at("priority").get<int>()<<");\n";
        if(o.at("materials").empty()) s<<"    #pragma unused "<<id<<"\n";
        for(auto it=o.at("materials").begin();it!=o.at("materials").end();++it) {
            auto &m=it.value();
            if(m.at("type")=="texture") s<<"    SetDynamicObjectMaterial("<<id<<", "<<it.key()<<", "<<m.at("model").get<int>()<<", "<<quote(m.at("txd"))<<", "<<quote(m.at("texture"))<<", "<<color(m.at("color"))<<");\n";
            else s<<"    SetDynamicObjectMaterialText("<<id<<", "<<it.key()<<", "<<quote(m.at("text"))<<", "<<m.at("size").get<int>()<<", "<<quote(m.at("font"))<<", "<<m.at("font_size").get<int>()<<", "<<(m.at("bold").get<bool>()?1:0)<<", "<<color(m.at("foreground"))<<", "<<color(m.at("background"))<<", "<<m.at("align").get<int>()<<");\n";
        }
    }
    s<<"    return 1;\n}\n\nstock "<<remove<<"(playerid)\n{\n";
    bool hasRemovals=false;
    for(const auto &r:d.at("removals")) {
        if(!group.empty() && r.at("group")!=group) continue;
        hasRemovals=true;
        s<<"    "<<sourceComment(r.at("group").get<std::string>())<<"\n";
        s<<"    RemoveBuildingForPlayer(playerid, "<<r.at("model").get<int>();
        for(auto &v:r.at("position")) s<<", "<<num(v.get<double>());
        s<<", "<<num(r.at("radius"))<<");\n";
    }
    if(!hasRemovals) s<<"    #pragma unused playerid\n";
    s<<"    return 1;\n}\n"; return s.str();
}
void Document::commit(const Json &next,const std::string &label) {
    ValidateDocument(next);
    Json entry={{"label",label},{"revision",revision+1},{"code",ExportPawn(next)},{"affected",changedRecords(data,next)}};
    undo_.push_back(data); if(undo_.size()>64) undo_.erase(undo_.begin()); redo_.clear(); data=next; ++revision;
    history.push_back(std::move(entry));
    if(history.size()>64) history.erase(0);
}
void Document::restore(const Json &snapshot) { ValidateDocument(snapshot); data=snapshot; ++revision; undo_.clear(); redo_.clear(); }
Json Document::snapshot() const { return {{"data",data},{"undo",undo_},{"redo",redo_},{"history",history}}; }
void Document::restoreSnapshot(const Json &s) {
    Json next=s.at("data"); ValidateDocument(next);
    std::vector<Json> nextUndo=s.at("undo").get<std::vector<Json>>();
    std::vector<Json> nextRedo=s.at("redo").get<std::vector<Json>>();
    for(const auto &entry:nextUndo) ValidateDocument(entry);
    for(const auto &entry:nextRedo) ValidateDocument(entry);
    Json nextHistory=s.at("history");
    if(!nextHistory.is_array()) throw std::runtime_error("invalid history snapshot");
    for(const auto &entry:nextHistory) {
        if(!entry.is_object() || !entry.contains("label") || !entry.at("label").is_string() ||
           !entry.contains("revision") || !entry.at("revision").is_number_unsigned() && !entry.at("revision").is_number_integer() ||
           !entry.contains("code") || !entry.at("code").is_string())
            throw std::runtime_error("invalid history entry");
        if(entry.at("revision").get<double>()<0 || entry.at("revision").get<double>()>4294967295.)
            throw std::runtime_error("invalid history revision");
        if(entry.contains("affected")) {
            if(!entry.at("affected").is_array()) throw std::runtime_error("invalid affected history");
            for(const auto &record:entry.at("affected"))
                if(!record.is_object() || !record.contains("kind") || !record.at("kind").is_string() ||
                   !record.contains("id") || !record.at("id").is_number_integer() ||
                   !record.contains("before") || !record.contains("after"))
                    throw std::runtime_error("invalid affected record");
        }
    }
    if(nextUndo.size()>64 || nextRedo.size()>64 || nextHistory.size()>64) throw std::runtime_error("snapshot history limit exceeded");
    data=std::move(next); undo_=std::move(nextUndo); redo_=std::move(nextRedo);
    history=std::move(nextHistory); ++revision;
}
Json Document::request(const Json &r) {
    std::string op=r.at("op");
    if(r.contains("expected_revision") && r.at("expected_revision")!=revision) throw std::runtime_error("stale document revision");
    if(op=="inspect") {
        Json result={{"document",data},{"revision",revision},{"history",r.value("include_history",false)?history:Json::array()}};
        if(r.value("include_snapshot",false)) result["snapshot"]=snapshot();
        return result;
    }
    if(op=="code") return {{"code",ExportPawn(data,r.value("group",std::string()))},{"revision",revision}};
    if(op=="patch") {
        Document staged; staged.data=data;
        for(auto &operation:r.at("operations")) {
            std::string action=operation.at("op");
            if(action!="place" && action!="update" && action!="delete" && action!="material" && action!="duplicate" && action!="removal" && action!="import" && action!="preview") throw std::runtime_error("unsupported patch operation");
            staged.request(operation);
        }
        commit(staged.data,"SA-MP patch"); return {{"revision",revision},{"document",data}};
    }
    if(op=="preview_import" || op=="import") {
        Json next=data, diagnostics=Json::array();
        for(auto &f:r.at("files")) {
            std::string path=f.at("name"), source;
            if(f.contains("source")) source=f.at("source").get<std::string>();
            else { std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("cannot open "+path); source.assign(std::istreambuf_iterator<char>(in),{}); }
            auto parsed=ParsePawn(source,path);
            for(auto &e:parsed.diagnostics) diagnostics.push_back(e);
            for(const char *kind:{"objects","removals"}) {
                auto &list=std::string(kind)=="objects"?parsed.objects:parsed.removals;
                for(auto &o:list) { o["id"]=allocateId(next); next[kind].push_back(o); }
            }
            if(std::find(next["groups"].begin(),next["groups"].end(),path)==next["groups"].end()) next["groups"].push_back(path);
        }
        ValidateDocument(next);
        if(op=="import") { if(!diagnostics.empty() && !r.value("accept_diagnostics",false)) throw std::runtime_error("import has diagnostics; preview and explicitly accept supported records"); commit(next,"Import Pawn"); }
        return {{"document",next},{"diagnostics",diagnostics},{"revision",revision}};
    }
    if(op=="save") { requireExtension(r.at("path"),".samp.json"); ValidateDocument(data); writeFile(r.at("path"),data.dump(2)); return {{"revision",revision}}; }
    if(op=="export") { requireExtension(r.at("path"),".pwn"); writeFile(r.at("path"),ExportPawn(data,r.value("group",std::string()))); return {{"revision",revision}}; }
    if(op=="open") { requireExtension(r.at("path"),".samp.json"); commit(readJson(r.at("path")),"Open project"); }
    else if(op=="undo" || op=="redo") {
        auto &from=op=="undo"?undo_:redo_; auto &to=op=="undo"?redo_:undo_;
        if(from.empty()) throw std::runtime_error("nothing to "+op);
        Json next=from.back(); ValidateDocument(next);
        Json entry={{"label",op},{"revision",revision+1},{"code",ExportPawn(next)},{"affected",changedRecords(data,next)}};
        to.push_back(data); if(to.size()>64) to.erase(to.begin());
        data=std::move(next); from.pop_back(); ++revision;
        history.push_back(std::move(entry));
        if(history.size()>64) history.erase(0);
    } else if(op=="replace") {
        if(r.contains("snapshot")) restoreSnapshot(r.at("snapshot"));
        else commit(r.at("document"),r.value("label",std::string("Edit")));
    }
    else if(op=="clear") commit(EmptyDocument(),"New project");
    else if(op=="preview") {
        Json next=data;
        if(r.contains("preview") && r.at("preview").is_object()) {
            if(r.at("preview").contains("world")) next["preview"]["world"]=r.at("preview").at("world");
            if(r.at("preview").contains("interior")) next["preview"]["interior"]=r.at("preview").at("interior");
        }
        if(r.contains("world")) next["preview"]["world"]=r.at("world");
        if(r.contains("interior")) next["preview"]["interior"]=r.at("interior");
        ValidateDocument(next);
        commit(next,r.value("label",std::string("Preview filters")));
    }
    else if(op=="update" || op=="delete" || op=="material" || op=="duplicate") {
        Json next=data; std::string kind=r.value("kind",std::string("objects"));
        if(kind!="objects" && kind!="removals") throw std::runtime_error("invalid record kind");
        bool found=false;
        for(size_t i=0;i<next[kind].size();++i) if(next[kind][i]["id"]==r.at("id")) {
            found=true; auto &o=next[kind][i];
            if(op=="delete") next[kind].erase(i);
            else if(op=="duplicate") { Json copy=o; copy["id"]=allocateId(next); next[kind].push_back(copy); }
            else if(op=="material") { std::string slot=std::to_string(r.at("slot").get<int>()); if(r.at("material").is_null()) o["materials"].erase(slot); else o["materials"][slot]=r.at("material"); }
            else { Json changes=r.at("changes"); if(changes.contains("id")) throw std::runtime_error("IDs are immutable"); o.update(changes); }
            break;
        }
        if(!found) throw std::runtime_error("unknown ID"); commit(next,op);
    } else if(op=="place") {
        Json next=data;
        Json o=ParsePawn("CreateDynamicObject(0,0,0,0,0,0,0);","untitled").objects[0];
        o.update(r.at("object")); o["id"]=allocateId(next);
        next["objects"].push_back(o);
        if(std::find(next["groups"].begin(),next["groups"].end(),o["group"])==next["groups"].end()) next["groups"].push_back(o["group"]);
        commit(next,"Place object");
    } else if(op=="removal") {
        Json next=data, row=r.at("removal"); row["id"]=allocateId(next);
        row["group"]=row.value("group",std::string("untitled")); next["removals"].push_back(row);
        if(std::find(next["groups"].begin(),next["groups"].end(),row["group"])==next["groups"].end()) next["groups"].push_back(row["group"]);
        commit(next,"Remove building");
    } else throw std::runtime_error("unknown SA-MP operation: "+op);
    return {{"revision",revision},{"document",data}};
}
}
