// halfedge.hpp : structure half-edge du cours (pointeurs) + conteneur HalfEdgeMesh
#pragma once

#include <Eigen/Dense>

#include <cstdint>
#include <unordered_map>
#include <vector>


struct HalfEdge;  

struct HE_Vertex {
    Eigen::Vector3d Coord;
    HalfEdge* he = nullptr;       
};

struct HE_Face {
    HalfEdge* he = nullptr;       
};

struct HalfEdge {
    HE_Vertex* target = nullptr;  
    HalfEdge*  twin   = nullptr;  
    HalfEdge*  next   = nullptr;  
    HE_Face*   face   = nullptr;
};


struct Stats {
    long   V = 0, E = 0, F = 0;
    long   euler = 0;             // V - E + F
    size_t sansTwin = 0;          
};


class HalfEdgeMesh {
public:
    HalfEdgeMesh() = default;
    
    HalfEdgeMesh(const HalfEdgeMesh&) = delete;
    HalfEdgeMesh& operator=(const HalfEdgeMesh&) = delete;

    
    void construire(const std::vector<Eigen::Vector3d>& points,
                    const std::vector<std::vector<int>>& polygones) {
        
        size_t totalHE = 0;
        for (const auto& p : polygones) totalHE += p.size();

        vertices_.assign(points.size(), HE_Vertex());
        faces_.assign(polygones.size(), HE_Face());
        halfedges_.assign(totalHE, HalfEdge());
        aretesDupliquees_ = 0;

        for (size_t i = 0; i < points.size(); ++i) vertices_[i].Coord = points[i];

        
        const uint64_t N = points.size();
        std::unordered_map<uint64_t, HalfEdge*> aretes;
        aretes.reserve(totalHE);

        size_t debut = 0;   
        for (size_t f = 0; f < polygones.size(); ++f) {
            const std::vector<int>& poly = polygones[f];
            const size_t n = poly.size();

            faces_[f].he = &halfedges_[debut];

            for (size_t k = 0; k < n; ++k) {
                HalfEdge* h = &halfedges_[debut + k];
                const int from = poly[k];
                const int to   = poly[(k + 1) % n];

                h->target = &vertices_[to];
                h->face   = &faces_[f];
                h->next   = &halfedges_[debut + (k + 1) % n];   
                vertices_[from].he = h;                          

                // twin
                if (!aretes.emplace(uint64_t(from) * N + to, h).second) {
                    ++aretesDupliquees_;   
                    continue;           
                }
                auto it = aretes.find(uint64_t(to) * N + from);
                if (it != aretes.end()) {
                    h->twin = it->second;
                    it->second->twin = h;
                }
            }
            debut += n;
        }
    }

    

   
    static const HE_Vertex* origine(const HalfEdge* h) {
        return precedente(h)->target;
    }

    // Demi-arete precedente dans la face (tour complet via next)
    static const HalfEdge* precedente(const HalfEdge* h) {
        const HalfEdge* p = h;
        while (p->next != h) p = p->next;
        return p;
    }

    static bool estBord(const HalfEdge* h) { return h->twin == nullptr; }

    
    std::vector<const HalfEdge*> sortantes(const HE_Vertex* v) const {
        std::vector<const HalfEdge*> res;
        if (v == nullptr || v->he == nullptr) return res;

        const HalfEdge* premiere = v->he;
        while (true) {                                   
            const HalfEdge* p = precedente(premiere);
            if (p->twin == nullptr) break;               
            premiere = p->twin;
            if (premiere == v->he) break;                
        }

        const HalfEdge* h = premiere;                    
        do {
            res.push_back(h);
            if (h->twin == nullptr) break;              
            h = h->twin->next;
        } while (h != premiere);
        return res;
    }

    // Sommets du contour d'une face
    std::vector<const HE_Vertex*> sommetsDeFace(const HE_Face* f) const {
        std::vector<const HE_Vertex*> res;
        const HalfEdge* h = f->he;
        do {
            res.push_back(h->target);
            h = h->next;
        } while (h != f->he);
        return res;
    }

    
    const std::vector<HE_Vertex>& sommets()    const { return vertices_; }
    const std::vector<HE_Face>&   faces()      const { return faces_; }
    const std::vector<HalfEdge>&  demiAretes() const { return halfedges_; }
    size_t aretesDupliquees() const { return aretesDupliquees_; }

    
    Stats stats() const {
        Stats s;
        for (const auto& h : halfedges_) if (h.twin == nullptr) ++s.sansTwin;
        s.V = long(vertices_.size());
        s.F = long(faces_.size());
        s.E = (long(halfedges_.size()) + long(s.sansTwin)) / 2;
        s.euler = s.V - s.E + s.F;
        return s;
    }

private:
    std::vector<HE_Vertex> vertices_;
    std::vector<HE_Face>   faces_;
    std::vector<HalfEdge>  halfedges_;
    size_t aretesDupliquees_ = 0;
};
