// halfedge.hpp : structure half-edge du cours (pointeurs bruts) + conteneur HalfEdgeMesh
#pragma once

#include <Eigen/Dense>

#include <cstdint>
#include <unordered_map>
#include <vector>

// ===============================================================
// PARTIE 1 : les 3 structures du cours
// ===============================================================
struct HalfEdge;   // declaration anticipee : HE_Vertex a besoin de connaitre HalfEdge

struct HE_Vertex {
    Eigen::Vector3d Coord;
    HalfEdge* he = nullptr;       // UNE demi-arete SORTANTE de ce sommet
};

struct HE_Face {
    HalfEdge* he = nullptr;       // UNE demi-arete de cette face
};

struct HalfEdge {
    HE_Vertex* target = nullptr;  // sommet d'arrivee
    HalfEdge*  twin   = nullptr;  // demi-arete opposee (nullptr si arete de bord)
    HalfEdge*  next   = nullptr;  // suivante dans la face (sens anti-horaire)
    HE_Face*   face   = nullptr;  // face a laquelle elle appartient
};

// Plusieurs valeurs de retour -> une struct (cf. cours "How to pass parameters")
struct Stats {
    long   V = 0, E = 0, F = 0;
    long   euler = 0;             // V - E + F
    size_t sansTwin = 0;          // demi-aretes de bord
};

// ===============================================================
// PARTIE 2 : le conteneur
//   - Il POSSEDE tous les Vertex / Face / HalfEdge (dans des std::vector).
//   - Les 3 structures ne connaissent que des POINTEURS vers les autres.
//   - Les vector sont dimensionnes UNE SEULE FOIS, avant de creer les pointeurs :
//     pas de reallocation -> les pointeurs restent valides.
// ===============================================================
class HalfEdgeMesh {
public:
    HalfEdgeMesh() = default;
    // Interdit la copie : une copie contiendrait des pointeurs vers l'ORIGINAL.
    HalfEdgeMesh(const HalfEdgeMesh&) = delete;
    HalfEdgeMesh& operator=(const HalfEdgeMesh&) = delete;

    // ---------------------------------------------------------------
    // PARTIE 3 : construction (cablage de la topologie)
    //   points   : coordonnees des sommets
    //   polygones: pour chaque face, les indices de ses sommets en sens anti-horaire
    //   (const& = pas de copie, lecture seule)
    // ---------------------------------------------------------------
    void construire(const std::vector<Eigen::Vector3d>& points,
                    const std::vector<std::vector<int>>& polygones) {
        // 1) Tailles connues d'avance -> on alloue tout d'un coup
        size_t totalHE = 0;
        for (const auto& p : polygones) totalHE += p.size();

        vertices_.assign(points.size(), HE_Vertex());
        faces_.assign(polygones.size(), HE_Face());
        halfedges_.assign(totalHE, HalfEdge());
        aretesDupliquees_ = 0;

        for (size_t i = 0; i < points.size(); ++i) vertices_[i].Coord = points[i];

        // 2) Une demi-arete par (face, cote). On retient (origine,arrivee) -> demi-arete
        //    pour retrouver le twin : celui de (a,b) est (b,a).
        const uint64_t N = points.size();
        std::unordered_map<uint64_t, HalfEdge*> aretes;
        aretes.reserve(totalHE);

        size_t debut = 0;   // indice de la 1ere demi-arete de la face courante
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
                h->next   = &halfedges_[debut + (k + 1) % n];   // boucle sur elle-meme
                vertices_[from].he = h;                          // sortante de 'from'

                // twin
                if (!aretes.emplace(uint64_t(from) * N + to, h).second) {
                    ++aretesDupliquees_;   // meme arete orientee deja vue : non manifold
                    continue;              // on garde h, sans twin
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

    // ---------------------------------------------------------------
    // PARTIE 4 : parcours (la raison d'etre de la structure)
    // ---------------------------------------------------------------

    // Origine d'une demi-arete : la structure ne la stocke pas.
    // On fait le tour de la face jusqu'a la demi-arete juste AVANT h :
    // sa cible est l'origine de h.
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

    // Toutes les demi-aretes SORTANTES de v (pointeur = valeur optionnelle possible : nullptr ok)
    //   suivante autour de v :  h -> h->twin->next
    //   precedente autour de v: h -> precedente(h)->twin
    // Si v est au bord, le tour n'est pas ferme : on recule d'abord jusqu'a la
    // premiere demi-arete, puis on avance jusqu'a tomber sur un twin nul.
    std::vector<const HalfEdge*> sortantes(const HE_Vertex* v) const {
        std::vector<const HalfEdge*> res;
        if (v == nullptr || v->he == nullptr) return res;

        const HalfEdge* premiere = v->he;
        while (true) {                                   // 1) reculer
            const HalfEdge* p = precedente(premiere);
            if (p->twin == nullptr) break;               //    bord atteint
            premiere = p->twin;
            if (premiere == v->he) break;                //    tour complet (sommet interieur)
        }

        const HalfEdge* h = premiere;                    // 2) avancer
        do {
            res.push_back(h);
            if (h->twin == nullptr) break;               //    bord atteint
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

    // ---------------------------------------------------------------
    // PARTIE 5 : acces en lecture + statistiques
    // ---------------------------------------------------------------
    const std::vector<HE_Vertex>& sommets()    const { return vertices_; }
    const std::vector<HE_Face>&   faces()      const { return faces_; }
    const std::vector<HalfEdge>&  demiAretes() const { return halfedges_; }
    size_t aretesDupliquees() const { return aretesDupliquees_; }

    // Retourne plusieurs valeurs dans une struct
    Stats stats() const {
        Stats s;
        for (const auto& h : halfedges_) if (h.twin == nullptr) ++s.sansTwin;
        s.V = long(vertices_.size());
        s.F = long(faces_.size());
        s.E = (long(halfedges_.size()) + long(s.sansTwin)) / 2;   // interieures comptees 2x, bord 1x
        s.euler = s.V - s.E + s.F;
        return s;
    }

private:
    std::vector<HE_Vertex> vertices_;
    std::vector<HE_Face>   faces_;
    std::vector<HalfEdge>  halfedges_;
    size_t aretesDupliquees_ = 0;
};
