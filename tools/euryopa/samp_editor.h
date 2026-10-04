#pragma once
#include <string>
struct ObjectInst;
void SampDrawWindow();
void SampSetWindowVisible(bool visible);
bool SampIsWindowVisible();
bool SampActive();
bool SampOwns(const ObjectInst *instance);
float SampDrawDistance(const ObjectInst *instance, float fallback);
bool SampHidden(const ObjectInst *instance);
void SampSetCaptureFilters(int world, int interior);
void SampClearCaptureFilters();
unsigned SampDocumentRevision();
void SampTick();
void SampApplyMaterials(ObjectInst *instance);
void SampInvalidateMaterials(ObjectInst *instance);
void SampUndo(bool redo);
std::string SampRequest(const std::string &request);
std::string SampSnapshot();
void SampRestore(const std::string &snapshot);
ObjectInst *SampCreateInstance(int model, float x, float y, float z);

#include <vector>
ObjectInst *SampPlace(int model, const rw::V3d &position, const rw::Quat *rotation);
int SampCut(const std::vector<ObjectInst*> &source);
int SampCopy(const std::vector<ObjectInst*> &source);
int SampDelete(const std::vector<ObjectInst*> &source);
int SampPaste(const std::vector<ObjectInst*> &source, bool inPlace, bool cut);
