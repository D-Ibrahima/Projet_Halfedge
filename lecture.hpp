// lecture.hpp : lit (presque) n'importe quel fichier 3D avec Assimp
//               et le convertit en : points + polygones (indices depuis 0)
#pragma once

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct MaillageBrut {
    std::vector<Eigen::Vector3d>  points;
    std::vector<std::vector<int>> polygones;
    size_t facesIgnorees = 0;
};

namespace detail {
struct CleePos {
    long long x, y, z;
    bool operator==(const CleePos& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct HashCleePos {
    size_t operator()(const CleePos& k) const {
        size_t h = std::hash<long long>()(k.x);
        h ^= std::hash<long long>()(k.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<long long>()(k.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};
}  // namespace detail

// path : lecture seule (const&)   |   brut : SORTIE (reference non const)
// retour : true si OK
inline bool lireMaillage(const std::string& path, MaillageBrut& brut) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_PreTransformVertices |   // applique les transformations des noeuds
        aiProcess_Triangulate |            // tout en triangles
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mNumMeshes == 0) {
        std::cerr << "Assimp : " << importer.GetErrorString() << "\n";
        return false;
    }

    // OBJ, STL... dupliquent les sommets (normales, uv) : on les soude par position
    // pour que les twins puissent se retrouver.
    std::unordered_map<detail::CleePos, int, detail::HashCleePos> index;

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        std::vector<int> remap(mesh->mNumVertices);

        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D& p = mesh->mVertices[i];
            detail::CleePos k{std::llround(p.x * 1e5), std::llround(p.y * 1e5), std::llround(p.z * 1e5)};
            auto it = index.emplace(k, int(brut.points.size()));
            if (it.second) brut.points.emplace_back(p.x, p.y, p.z);
            remap[i] = it.first->second;
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices < 3) { ++brut.facesIgnorees; continue; }   // points, segments

            std::vector<int> poly(face.mNumIndices);
            for (unsigned k = 0; k < face.mNumIndices; ++k) poly[k] = remap[face.mIndices[k]];

            std::vector<int> tri = poly;
            std::sort(tri.begin(), tri.end());
            if (std::adjacent_find(tri.begin(), tri.end()) != tri.end()) {   // face degeneree
                ++brut.facesIgnorees;
                continue;
            }
            brut.polygones.push_back(std::move(poly));
        }
    }
    return !brut.points.empty() && !brut.polygones.empty();
}
