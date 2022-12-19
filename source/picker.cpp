#ifndef __MAC__
#include <GL/glew.h>
#endif

#include "picker.h"
#include "uniforms.h"

using namespace std;

Picker::Picker(const RigTForm &initialRbt, Uniforms &uniforms)
    : drawer_(initialRbt, uniforms), idCounter_(0) {}

bool Picker::visit(SgTransformNode &node) {
    nodeStack_.push_back(node.shared_from_this());
    return drawer_.visit(node);
}

bool Picker::postVisit(SgTransformNode &node) {
    nodeStack_.pop_back();
    return drawer_.postVisit(node);
}

bool Picker::visit(SgShapeNode &node) {
    idCounter_++;
    for (int i = nodeStack_.size() - 1; i >= 0; --i) {
        shared_ptr<SgRbtNode> asRbtNode =
            dynamic_pointer_cast<SgRbtNode>(nodeStack_[i]);
        if (asRbtNode) {
            addToMap(idCounter_, asRbtNode);
            break;
        }
    }
    const Cvec3 idColor = idToColor(idCounter_);

    drawer_.getUniforms().put("uIdColor", idColor);
    return drawer_.visit(node);
}

bool Picker::postVisit(SgShapeNode &node) { return drawer_.postVisit(node); }

shared_ptr<SgRbtNode> Picker::getRbtNodeAtXY(int x, int y) {
    PackedPixel query;
    glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &query);
    const int id = colorToId(query);

    return find(id);
}

//------------------
// Helper functions
//------------------
//
void Picker::addToMap(int id, shared_ptr<SgRbtNode> node) {
    idToRbtNode_[id] = node;
}

shared_ptr<SgRbtNode> Picker::find(int id) {
    IdToRbtNodeMap::iterator it = idToRbtNode_.find(id);
    if (it != idToRbtNode_.end())
        return it->second;
    else
        return shared_ptr<SgRbtNode>(); // set to null
}

// encode 2^4 = 16 IDs in each of R, G, B channel, for a total of 16^3 number of
// objects
static const int NBITS = 4, N = 1 << NBITS, MASK = N - 1;

Cvec3 Picker::idToColor(int id) {
    assert(id > 0 && id < N * N * N);
    Cvec3 framebufferColor =
        Cvec3(id & MASK, (id >> NBITS) & MASK, (id >> (NBITS + NBITS)) & MASK);
    framebufferColor = framebufferColor / N + Cvec3(0.5 / N);

    return framebufferColor;
}

int Picker::colorToId(const PackedPixel &p) {
    const int UNUSED_BITS = 8 - NBITS;
    int id = p.r >> UNUSED_BITS;
    id |= ((p.g >> UNUSED_BITS) << NBITS);
    id |= ((p.b >> UNUSED_BITS) << (NBITS + NBITS));
    return id;
}
