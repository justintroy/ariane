#include <rw.h>
#include "samp_rotation.h"
#include <algorithm>
#include <cmath>

namespace samp {
namespace { constexpr double PI=3.14159265358979323846; }

rw::Quat ToStoredRotation(const std::array<double,3> &degrees) {
    // SA-MP render order is Rz * Rx * Ry. Ariane stores its conjugate.
    double x=degrees[0]*PI/360, y=degrees[1]*PI/360, z=degrees[2]*PI/360;
    rw::Quat qx={(float)sin(x),0,0,(float)cos(x)};
    rw::Quat qy={0,(float)sin(y),0,(float)cos(y)};
    rw::Quat qz={0,0,(float)sin(z),(float)cos(z)};
    return rw::conj(rw::mult(rw::mult(qz,qx),qy));
}

std::array<double,3> FromRenderMatrix(const rw::Matrix &m) {
    double x=asin(std::max(-1.,std::min(1.,double(m.up.z)))), y,z;
    // Single-precision RenderWare matrices lose independent yaw/roll near +/-90 X.
    if(fabs(cos(x))>1e-3) {
        y=atan2(-m.right.z,m.at.z);
        z=atan2(-m.up.x,m.up.y);
    } else {
        // At gimbal lock, only z +/- y is observable. Choose y=0.
        y=0;
        z=atan2(m.right.y,m.right.x);
    }
    return {x*180/PI,y*180/PI,z*180/PI};
}
}
