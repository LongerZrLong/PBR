#ifndef SCENEGRAPH_H
#define SCENEGRAPH_H

#include <memory>
#include <stdexcept>
#include <vector>

#include "common.h"
#include "geometry.h"
#include "glsupport.h" // for Noncopyable
#include "matrix4.h"
#include "rigtform.h"
#include "uniforms.h"

using namespace std;

class SgNodeVisitor;

class SgNode : public enable_shared_from_this<SgNode>, Noncopyable {
  public:
    virtual bool accept(SgNodeVisitor &vistor) = 0;
    virtual ~SgNode() {}

    // Two nodes are equal if and only if they're the same, i.e.,
    // having the same in memory address
    bool operator==(const SgNode &other) const { return this == &other; }

    bool operator!=(const SgNode &other) const { return !(*this == other); }

  protected:
    SgNode() {}
};

//
// A transform node can have descendents nodes. It uses a
// rigid body transform to represent its frame with respect to
// the parent frame
//
class SgTransformNode : public SgNode {
  public:
    virtual bool accept(SgNodeVisitor &visitor);
    virtual RigTForm getRbt() = 0;

    void addChild(shared_ptr<SgNode> child);
    void removeChild(shared_ptr<SgNode> child);

    int getNumChildren() const { return children_.size(); }

    shared_ptr<SgNode> getChild(int i) { return children_[i]; }

  private:
    std::vector<shared_ptr<SgNode>> children_;
};

//
// A shape node has no decendents, but can perform drawing. It can also
// provide an affine matrix to be used to modify its geometry.
//
class SgShapeNode : public SgNode {
  public:
    virtual bool accept(SgNodeVisitor &visitor);

    virtual Matrix4 getAffineMatrix() = 0;
    virtual void draw(const Uniforms &uniforms) = 0;
};

// Visitor class for the scene graph nodes. If any of the
// visit/postVisit functions return false, the traverse
// will be terminated.
class SgNodeVisitor {
  public:
    virtual bool visit(SgTransformNode &node) { return true; }
    virtual bool visit(SgShapeNode &node) { return true; }

    virtual bool postVisit(SgTransformNode &node) { return true; }
    virtual bool postVisit(SgShapeNode &node) { return true; }
};

RigTForm getPathAccumRbt(shared_ptr<SgTransformNode> source,
                         shared_ptr<SgTransformNode> destination,
                         int offsetFromDestination = 0);

//----------------------------------------------------
// Concrete scene graph node implementations follow
//----------------------------------------------------

// A SgRoot node is a Transform node with identity Rbt
class SgRootNode : public SgTransformNode {
  public:
    SgRootNode() {}

    virtual RigTForm getRbt() { return RigTForm(); }
};

// A SgRbtNode is a Transform node that wraps a RigTForm
class SgRbtNode : public SgTransformNode {
  public:
    SgRbtNode(const RigTForm &rbt = RigTForm()) : rbt_(rbt) {}

    virtual RigTForm getRbt() { return rbt_; }

    void setRbt(const RigTForm &rbt) { rbt_ = rbt; }

  private:
    RigTForm rbt_;
};

class SgGeometryShapeNode : public SgShapeNode {
  public:
    shared_ptr<Geometry> geometry;
    shared_ptr<Material> material;
    Matrix4 affineMatrix;

    SgGeometryShapeNode(shared_ptr<Geometry> _geometry,
                        shared_ptr<Material> _material,
                        const Cvec3 &translation = Cvec3(0, 0, 0),
                        const Cvec3 &eulerAngles = Cvec3(0, 0, 0),
                        const Cvec3 &scales = Cvec3(1, 1, 1))
        : geometry(_geometry), material(_material),
          affineMatrix(Matrix4::makeTranslation(translation) *
                       Matrix4::makeXRotation(eulerAngles[0]) *
                       Matrix4::makeYRotation(eulerAngles[1]) *
                       Matrix4::makeZRotation(eulerAngles[2]) *
                       Matrix4::makeScale(scales)) {}

    virtual Matrix4 getAffineMatrix() { return affineMatrix; }

    void setAffineMatrix(const Cvec3 &translation = Cvec3(0, 0, 0),
                         const Cvec3 &eulerAngles = Cvec3(0, 0, 0),
                         const Cvec3 &scales = Cvec3(1, 1, 1)) {
        affineMatrix = Matrix4::makeTranslation(translation) *
                       Matrix4::makeXRotation(eulerAngles[0]) *
                       Matrix4::makeYRotation(eulerAngles[1]) *
                       Matrix4::makeZRotation(eulerAngles[2]) *
                       Matrix4::makeScale(scales);
    }

    virtual void draw(const Uniforms &uniforms) {
        if (g_overridingMaterial)
            g_overridingMaterial->draw(*geometry, uniforms);
        else
            material->draw(*geometry, uniforms);
    }
};

#endif
