#ifndef MODEL_H
#define MODEL_H

#include <memory>

#include "tiny_obj_loader.h"
#include "geometry.h"

using namespace std;

shared_ptr<Geometry> loadObj(const char *filePath) {
    tinyobj::attrib_t attrib;
    vector<tinyobj::shape_t> shapes;
    vector<tinyobj::material_t> materials;

    std::string err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filePath);

    if (!err.empty()) {
        cerr << "ERR: " << err << endl;
    }

    if (!ret) {
        cerr << "Failed to load/parse .obj.\n";
        assert(false);
    }

    auto vertices = vector<VertexPNX>();

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {

            VertexPNX vertex;

            vertex.p = Cvec3f(
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
            );

            vertex.x = Cvec2f(
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
            );

            vertex.n = Cvec3f(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
            );

            // Not using indices here for simplicity
            vertices.push_back(vertex);
        }
    }

    return make_shared<SimpleGeometryPNX>(vertices.data(), vertices.size());
}

shared_ptr<Material> loadPBRTextures(const Material &prototype, const char *texDir, const char *imgType) {
    auto ptr = make_shared<Material>(prototype);

    string strTexDir = texDir;
    string ext = "." + string(imgType);

    const string texType[] = {"albedo", "normal", "metallic", "roughness", "ao"};
    const string uniformNames[] = {"uAlbedoMap", "uNormalMap", "uMetallicMap", "uRoughnessMap", "uAoMap"};

    for (int i = 0; i < 5; i++) {
        string path = strTexDir;
        path += "/";
        path += texType[i];
        path += ext;

        ptr->getUniforms().put(uniformNames[i], shared_ptr<ImageTexture>(new ImageTexture(path.c_str())));
    }

    return ptr;
}

#endif //MODEL_H
