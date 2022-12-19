#ifndef TEXTURE_H
#define TEXTURE_H

#include <vector>
#include <string>

#include "glsupport.h"

class Texture {
protected:
    GlTexture tex;

public:
    // Must return one of GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D,
    // GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, or GL_SAMPLER_2D_SHADOW, as its
    // intended usage by GLSL shader
    virtual GLenum getSamplerType() const = 0;

    // Binds the texture. (The caller is responsible for setting the active
    // texture unit)
    virtual void bind() const = 0;

    virtual ~Texture() {}

    virtual const GlTexture& getGlTexture() const { return tex; }
};

//----------------------------------------
// concrete implementations of Texture
//----------------------------------------

class ImageTexture : public Texture {
public:
    ImageTexture() {};
    ImageTexture(const char *filename);

    GLenum getSamplerType() const { return GL_SAMPLER_2D; }

    void bind() const { glBindTexture(GL_TEXTURE_2D, tex); }
};

class CubeMapTexture : public Texture {
public:
    CubeMapTexture() {};
    CubeMapTexture(std::vector<std::string> faces);

    GLenum getSamplerType() const { return GL_SAMPLER_CUBE; }

    void bind() const { glBindTexture(GL_TEXTURE_CUBE_MAP, tex); }
};

#endif
