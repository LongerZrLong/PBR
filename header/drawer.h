#ifndef DRAWER_H
#define DRAWER_H

#include <vector>

#include "common.h"
#include "scenegraph.h"
#include "uniforms.h"

class Drawer : public SgNodeVisitor {
  protected:
    std::vector<RigTForm> rbtStack_;
    Uniforms &uniforms_;

  public:
    Drawer(const RigTForm &initialRbt, Uniforms &uniforms)
        : rbtStack_(1, initialRbt), uniforms_(uniforms) {}

    virtual bool visit(SgTransformNode &node) {
        rbtStack_.push_back(rbtStack_.back() * node.getRbt());
        return true;
    }

    virtual bool postVisit(SgTransformNode &node) {
        rbtStack_.pop_back();
        return true;
    }

    virtual bool visit(SgShapeNode &shapeNode) {
        const Matrix4 modelMat = rigTFormToMatrix(rbtStack_.back()) * shapeNode.getAffineMatrix();
        sendModelMatrix(uniforms_, modelMat);
        shapeNode.draw(uniforms_);
        return true;
    }

    virtual bool postVisit(SgShapeNode &shapeNode) { return true; }

    Uniforms &getUniforms() { return uniforms_; }
};

#endif
