#include <rw.h>
#include "samp_rotation.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

static int checks=0;
static void check(bool condition) {
    ++checks;
    if(!condition) throw std::runtime_error("rotation check failed: "+std::to_string(checks));
}

int main() {
    const std::array<std::array<double,3>,10> samples={{
        {{0,0,0}}, {{0,0,90}}, {{10,20,30}}, {{-35,71,-123}},
        {{89.999,42,-18}}, {{-89.999,42,-18}}, {{90,45,120}},
        {{-90,45,120}}, {{180,0,180}}, {{179.5,-176.25,92.75}}
    }};
    for(const auto &source:samples) {
        auto stored=samp::ToStoredRotation(source);
        rw::Matrix matrix;
        matrix.rotate(rw::conj(stored),rw::COMBINEREPLACE);
        auto recovered=samp::FromRenderMatrix(matrix);
        auto again=samp::ToStoredRotation(recovered);
        double dot=stored.x*again.x+stored.y*again.y+stored.z*again.z+stored.w*again.w;
        check(std::fabs(dot)>0.99999);
    }
    std::cout<<checks<<" SA-MP orientation checks passed\n";
}
