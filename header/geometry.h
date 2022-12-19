#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <cassert>
#include <map>
#include <cmath>
#include <string>
#include <stdexcept>
#include <memory>

#include "cvec.h"
#include "glsupport.h"
#include "geometrymaker.h"

// An abstract class that encapsulates geometry data that provides vertex attributes and
// know how to draw itself.
class Geometry {
public:
  // return names of vertex attributes provided by this geometry
  virtual const std::vector<std::string>& getVertexAttribNames() = 0;

  // Draw the geometry. attribIndices[i] corresponds to the index of the
  // shader vertex attribute location that the i-th vertex attribute provided
  // by this geometry should bind to. It can be -1 to indicate that this stream is
  // not used. The caller is responsible for enable/disable vertex attribute arrays.
  virtual void draw(int attribIndices[]) = 0;

  virtual ~Geometry() {}
};


// ============================================================================
// We provide a flexible implementation of Geometry called BufferObjectGeometry
// that allows drawing using vertices from one or more vertex buffers, zero or
// one index buffers, as well as using different primitives (GL_TRIANGLES,
// GL_POINTS, etc).  The following is its implementation.
//
// For very simple usage cases where such flexibility is not desired, you
// can used one of the SimpleGeometry* or SimpleIndexedGeometry* types defined
// at the end of this header. They are built on top of the BufferObjectGeometry
// structure, and demos its usage
// ============================================================================

// Helper class that describes the format of a vertex. Maintains
// a list of attribute descriptions
class VertexFormat {
public:
  // Parameters that you would pass into glVertexAttribPointer
  struct AttribDesc {
    std::string name;
    GLint size;
    GLenum type;
    GLboolean normalized;
    int offset;

    AttribDesc(const std::string& _name, GLint _size, GLenum _type, GLboolean _normalized, int _offset)
      : name(_name), size(_size), type(_type), normalized(_normalized), offset(_offset) {
      assert(_name != "");   // some basic sanity checks
      assert(_size > 0);
      assert(_offset >= 0);
    }
  };

  // Initialize to zero attributes
  VertexFormat(int vertexSize) : vertexSize_(vertexSize) {}

  // append a new attrib description
  VertexFormat& put(const std::string& name, GLint size, GLenum type, GLboolean normalized, int offset) {
    AttribDesc ad(name, size, type, normalized, offset);
    if (name2Idx_.find(name) == name2Idx_.end()) {
      name2Idx_[name] = attribDescs_.size();
      attribDescs_.push_back(ad);
    }
    else
      attribDescs_[name2Idx_[name]] = ad;

    return *this;
  }

  int getVertexSize() const {
    return vertexSize_;
  }

  int getNumAttribs() const {
    return attribDescs_.size();
  }

  const AttribDesc& getAttrib(int i) const {
    return attribDescs_[i];
  }

  // returns the index of the atribute with given name, or -1 if the name is not found
  int getAttribIndexForName(const std::string& name) const {
    std::map<std::string, int>::const_iterator i =  name2Idx_.find(name);
    return i == name2Idx_.end() ? -1 : i->second;
  }

  // Calls glVertexAttribPointer with appropirate arguments to bind the attribute
  // indexed by 'attribIndex' within this VertexFormat to vertex attribute location
  // specified by 'glAttribLocation'
  void setGlVertexAttribPointer(int attribIndex, int glAttribLocation) const {
    assert(glAttribLocation >= 0);
    const AttribDesc &ad = attribDescs_[attribIndex];
    glVertexAttribPointer(glAttribLocation, ad.size, ad.type, ad.normalized, vertexSize_, reinterpret_cast<const GLvoid*>(ad.offset));
  }

private:
  const int vertexSize_;
  std::vector<AttribDesc> attribDescs_;
  std::map<std::string, int> name2Idx_;
};

// Light wrapper for a GL buffer object storing vertices, together with format for its vertices.
class FormattedVbo : public GlBufferObject {
  const VertexFormat& format_;
  int length_;

public:
  // The passed in formatDesc_ is stored by reference. Hence the caller
  // should either pass in a static global variable, or ensure its lifespan
  // encompasses the lifespan of the FormmatedVbo
  FormattedVbo(const VertexFormat& formatDesc)
    : format_(formatDesc), length_(0) {}

  const VertexFormat& getVertexFormat() const {
    return format_;
  }

  // Number of vertices stored in the vertex
  int length() const {
    return length_;
  }

  // Upload vertex data to the vbo. Specify dynamicUsage = true if you intend
  // to upload different data multiple times
  template<typename Vertex>
  void upload(const Vertex* vertices, int length, bool dynamicUsage = false) {
    assert(sizeof(Vertex) == format_.getVertexSize());
    glBindBuffer(GL_ARRAY_BUFFER, *this);
    length_ = length;

    const int size = sizeof(Vertex) * length;
    if (dynamicUsage) {
      // We always call glBufferData with a NULL ptr here, so that OpenGL knows that
      // we're done with old data, and that if the old vbo is in use, it can  allocate
      // us a new one without stalling the pipeline
      glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
      glBufferSubData(GL_ARRAY_BUFFER, 0, size, vertices);
    }
    else {
      glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
    }
#ifndef NDEBUG
    checkGlErrors();
#endif
  }
};

// Light wrapper for a GL buffer object storing indices, together with format for its
// indices, one of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT
class FormattedIbo : public GlBufferObject {
  GLenum format_;
  int length_;
public:
  // format must be one of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT,or GL_UNSIGNED_INT
  // GL_UNSIGNED_SHORT is the default
  FormattedIbo(GLenum format = GL_UNSIGNED_SHORT) : format_(format), length_(0) {
    assert(format == GL_UNSIGNED_BYTE || format == GL_UNSIGNED_SHORT || format == GL_UNSIGNED_INT);
  }

  GLenum getIndexFormat() const {
    return format_;
  }

  int length() const {
    return length_;
  }

  template<typename Index>
  void upload(const Index *indices, int length, bool dynamicUsage = false) {
    assert((format_ == GL_UNSIGNED_BYTE && sizeof(Index) == 1) ||
           (format_ == GL_UNSIGNED_SHORT && sizeof(Index) == 2) ||
           (format_ == GL_UNSIGNED_INT && sizeof(Index) == 4));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *this);
    length_ = length;
    const int size = sizeof(Index) * length;
    if (dynamicUsage) {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
      glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, indices);
    }
    else {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
    }
#ifndef NDEBUG
    checkGlErrors();
#endif
  }

};

// A flexible light weight Geometry implementation allowing drawing using multiple vertex buffers,
// with or without an index buffer, and as different primitives (e.g., triangles, quads, points...).
//
// This essentially maintains a map of
//   vertex attribute names --> (FormattedVbo, attribute name)
//
// To draw its self, it binds all the vertex attributes that it is wired to, and calls
// the suitable OpenGL calls to draw either indexed or non-index geometry. There are optimizations
// to call glBindBuffer only once for each distince FormattedVbo it wires to.

class BufferObjectGeometry : public Geometry {
public:
  // Initialize, with zero vertex buffer and index buffer attached. Defaults to TRIANGLES primitive
  BufferObjectGeometry();

  // Declares and maps a vertex attribute named 'targetAttribName' to the
  // vertex attribute named 'sourceAttribName' of the 'source' FormattedVbo
  BufferObjectGeometry& wire(const std::string& targetAttribName,
                             std::shared_ptr<FormattedVbo> source,
                             const std::string& sourceAttribName);

  // Declares and maps a vertex attribute to the vertex attribute
  // named 'sourceAttribName' of the 'source' FormattedVbo. The declared
  // vertex attribute also holds the name 'sourceAttribName'
  BufferObjectGeometry& wire(std::shared_ptr<FormattedVbo> source, const std::string& sourceAttribName);

  // Declares and maps all vertex attributes contained in the 'source' FormattedVbo.
  // Same names are used for each attribute.
  BufferObjectGeometry& wire(std::shared_ptr<FormattedVbo> source);

  // Set the index buffer to be used. Pass in a null shared_ptr to mean non-indexed. Default is non-indexed
  BufferObjectGeometry& indexedBy(std::shared_ptr<FormattedIbo> ib);

  // Same as indexBy(null shared_ptr)
  BufferObjectGeometry& noIndex();

  // Set the primitive type to draw using. Default is GL_TRIANGLES.
  // Anything you can pass to glDrawArrays is fair game
  BufferObjectGeometry& primitiveType(GLenum primitiveType);

  // Return if we are in indexed mode
  bool isIndexed() const {
    return (bool)ib_;
  }

  // Return the primitive types we are drawing using. Default is GL_TRIANGLES
  GLenum getPrimitiveType() const {
    return primitiveType_;
  }

  // Methods declared by Geometry
  virtual const std::vector<std::string>& getVertexAttribNames();
  virtual void draw(int attribIndices[]);

private:
  typedef std::map<std::string, std::pair<std::shared_ptr<FormattedVbo>, std::string> > Wiring;

  GLenum primitiveType_;
  bool wiringChanged_;
  Wiring wiring_;
  std::shared_ptr<FormattedIbo> ib_;

  // Internal struct for optimized vb binding order
  struct PerVbWiring {
    // Use bare pointers since shared_ptrs are maintained by wiring_, hence
    // we do not need to worry about keeping it getting freed
    const FormattedVbo* vb;

    // A map from (attribute index in vb) --> relative index within BufferObjectGeometry's
    // exposed vertex attributes
    std::vector<std::pair<int, int> > vb2GeoIdx;

    PerVbWiring(const FormattedVbo* _vb) : vb(_vb) {}
  };

  std::vector<PerVbWiring> perVbWirings_;
  std::vector<std::string> vertexAttribNames_;

  // Setups up perVbWiring_ and vertexAttribNames_. Gets called whenever wiringChanged_ is true
  // and we need to draw or return list of vertex attributes.
  void processWiring();
};


// =============================================================================
// The BufferObjectGeometry is poweful and flexible, but for very simple cases,
// we also have a few predefined geometry implementations allowing
//   - some predefined vertex format
//   - index and non indexed rendering.
// These are based on the BufferObjectGeometry and demos its usage.
// =============================================================================


// First, some predefined vertex formats. It's perfectly easy for you to roll your own.
// Note that we define assignment operator (=) from GenericVertex
// of geometrymaker.h so that we can use geometrymaker on any of these format

// A vertex with float point Position
struct VertexPX {
    Cvec3f p;
    Cvec2f x;

    static const VertexFormat FORMAT;

    VertexPX() {}

    VertexPX(float x, float y, float z,
             float u, float v)
            : p(x,y,z), x(u, v) {}

    VertexPX(const Cvec3f& pos, const Cvec2f& texCoords)
            : p(pos), x(texCoords) {}

    // Define copy constructor and assignment operator from GenericVertex so we can
    // use make* functions from geometrymaker.h
    VertexPX(const GenericVertex& v) {
        *this = v;
    }

    VertexPX & operator = (const GenericVertex& v) {
        p = v.pos;
        x = v.tex;
        return *this;
    }

};

// A vertex with floating point Position, and Normal;
struct VertexPN {
  Cvec3f p, n;

  static const VertexFormat FORMAT;

  VertexPN() {}

  VertexPN(float x, float y, float z,
           float nx, float ny, float nz)
    : p(x,y,z), n(nx, ny, nz) {}

  VertexPN(const Cvec3f& pos, const Cvec3f& normal)
    : p(pos), n(normal) {}

  VertexPN(const Cvec3& pos, const Cvec3& normal)
    : p(pos[0], pos[1], pos[2]), n(normal[0], normal[1], normal[2]) {}


  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* functions from geometrymaker.h
  VertexPN(const GenericVertex& v) {
    *this = v;
  }

  VertexPN& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    return *this;
  }

};

// A vertex with floating point Position, Normal, and one set of teXture Coordinates;
struct VertexPNX {
  Cvec3f p, n;
  Cvec2f x; // texture coordinates

  static const VertexFormat FORMAT;

  VertexPNX() {}

  VertexPNX(float x, float y, float z,
            float nx, float ny, float nz,
            float u, float v)
    : p(x,y,z), n(nx, ny, nz), x(u, v) {}

  VertexPNX(const Cvec3f& pos, const Cvec3f& normal, const Cvec2f& texCoords)
    : p(pos), n(normal), x(texCoords) {}

  VertexPNX(const Cvec3& pos, const Cvec3& normal, const Cvec2& texCoords)
    : p(pos[0], pos[1], pos[2]), n(normal[0], normal[1], normal[2]), x(texCoords[0], texCoords[1]) {}


  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* functions from geometrymaker.h
  VertexPNX(const GenericVertex& v) {
    *this = v;
  }

  VertexPNX& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    x = v.tex;
    return *this;
  }
};


// A vertex with floating point Position, Normal, Tangent, Binormal, teXture Coord
struct VertexPNTBX {
  Cvec3f p, n;
  Cvec2f x; // texture coordinates
  Cvec3f t, b; // tangent, binormal

  static const VertexFormat FORMAT;

  VertexPNTBX() {}

  VertexPNTBX(float x, float y, float z,
              float nx, float ny, float nz,
              float tx, float ty, float tz,
              float bx, float by, float bz,
              float u, float v)
    : p(x,y,z), n(nx, ny, nz), x(u, v), t(tx, ty, tz), b(bx, by, bz) {}

  VertexPNTBX(const Cvec3f& pos, const Cvec3f& normal,
              const Cvec3f& tangent, const Cvec3f& binormal, const Cvec2f& texCoords)
    : p(pos), n(normal), x(texCoords), t(tangent), b(binormal) {}

  VertexPNTBX(const Cvec3& pos, const Cvec3& normal,
              const Cvec3& tangent, const Cvec3& binormal, const Cvec2& texCoords)
    : p(pos[0], pos[1], pos[2]), n(normal[0], normal[1], normal[2]), x(texCoords[0], texCoords[1]),
     t(tangent[0], tangent[1], tangent[2]), b(binormal[0], binormal[1], binormal[2]) {}

  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* functions from geometrymaker.h
  VertexPNTBX(const GenericVertex& v) {
    *this = v;
  }

  VertexPNTBX& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    t = v.tangent;
    b = v.binormal;
    x = v.tex;
    return *this;
  }
};

// Simple unindex geometry implementation based on BufferObjectGeometry
template<typename Vertex>
class SimpleUnindexedGeometry : public BufferObjectGeometry {
  std::shared_ptr<FormattedVbo> vbo;
public:
  SimpleUnindexedGeometry() : vbo(new FormattedVbo(Vertex::FORMAT)) {
    wire(vbo);
    primitiveType(GL_TRIANGLES);
  }

  SimpleUnindexedGeometry(const Vertex* vertices, int numVertices)
    : vbo(new FormattedVbo(Vertex::FORMAT)) {
    wire(vbo);
    primitiveType(GL_TRIANGLES);
    upload(vertices, numVertices);
  }

  void upload(const Vertex* vertices, int numVertices) {
    vbo->upload(vertices, numVertices, true);
  }
};


// Simple Index geometry implementation based on BufferObjectGeometry
template<typename Vertex, typename Index>
class SimpleIndexedGeometry : public BufferObjectGeometry {
  std::shared_ptr<FormattedVbo> vbo;
  std::shared_ptr<FormattedIbo> ibo;
public:
  SimpleIndexedGeometry()
    : vbo(new FormattedVbo(Vertex::FORMAT)), ibo(new FormattedIbo(size2IboFmt(sizeof(Index)))) {
    wire(vbo);
    indexedBy(ibo);
    primitiveType(GL_TRIANGLES);
  }

  SimpleIndexedGeometry(const Vertex* vertices,  const Index* indices, int numVertices, int numIndices)
    : vbo(new FormattedVbo(Vertex::FORMAT)), ibo(new FormattedIbo(size2IboFmt(sizeof(Index)))) {
    wire(vbo);
    indexedBy(ibo);
    primitiveType(GL_TRIANGLES);
    upload(vertices, indices, numVertices, numIndices);
  }

  void upload(const Vertex* vertices, const Index* indices, int numVertices, int numIndices) {
    vbo->upload(vertices, numVertices, true);
    ibo->upload(indices, numIndices, true);
  }

private:
  GLenum size2IboFmt(int size) {
    if (size == 1)
      return GL_UNSIGNED_BYTE;
    if (size == 2)
      return GL_UNSIGNED_SHORT;
    if (size == 4)
      return GL_UNSIGNED_INT;
    assert(0);
    throw std::runtime_error("Invalid index buffer format supplied");
  }
};


typedef SimpleUnindexedGeometry<VertexPX> SimpleGeometryPX;
typedef SimpleUnindexedGeometry<VertexPN> SimpleGeometryPN;
typedef SimpleUnindexedGeometry<VertexPNX> SimpleGeometryPNX;
typedef SimpleUnindexedGeometry<VertexPNTBX> SimpleGeometryPNTBX;

typedef SimpleIndexedGeometry<VertexPX, unsigned short> SimpleIndexedGeometryPX;
typedef SimpleIndexedGeometry<VertexPN, unsigned short> SimpleIndexedGeometryPN;
typedef SimpleIndexedGeometry<VertexPNX, unsigned short> SimpleIndexedGeometryPNX;
typedef SimpleIndexedGeometry<VertexPNTBX, unsigned short> SimpleIndexedGeometryPNTBX;

#endif
