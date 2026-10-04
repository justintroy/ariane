#pragma once
#include <array>

namespace rw { struct Quat; struct Matrix; }

namespace samp {
rw::Quat ToStoredRotation(const std::array<double,3> &degrees);
std::array<double,3> FromRenderMatrix(const rw::Matrix &matrix);
}
