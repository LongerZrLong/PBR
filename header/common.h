#ifndef ASSTCOMMON_H
#define ASSTCOMMON_H

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include "glsupport.h"
#include "material.h"
#include "uniforms.h"

extern const bool g_Gl2Compatible;

extern std::shared_ptr<Material> g_overridingMaterial;

// takes a projection matrix and send to the the shaders
inline void sendProjectionMatrix(Uniforms &uniforms, const Matrix4 &projMatrix) {
    uniforms.put("uProjMatrix", projMatrix);
}

inline void sendViewMatrix(Uniforms &uniforms, const Matrix4 &viewMatrix) {
    uniforms.put("uViewMatrix", viewMatrix);
}

inline void sendModelMatrix(Uniforms &uniforms, const Matrix4 &modelMatrix) {
    uniforms.put("uModelMatrix", modelMatrix);
}

#endif
