#include <array>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>


// Forward declarations

struct Vertex;
struct Face;
struct HalfEdge;

struct Vertex {
    double x = 0, y = 0, z = 0;
    HalfEdge* he = nullptr;      // a half-ridge leading to this summit
};

struct Face {
    HalfEdge* he = nullptr;      // halfedge of the face
};

struct HalfEdge {
    Vertex*   vertex = nullptr;  // target vertex
    Face*     face   = nullptr;  // face 
    HalfEdge* next   = nullptr;  // next
    HalfEdge* twin   = nullptr;  // twin
};

// ---------------------------------------------------------------
// Binary trie: key = (source, target) encoded on 2*B bits,
// with B = ceil(log2(nbVertices)). Depth = 2*B = O(log N),
// so insertion and search in O(log N).
// ---------------------------------------------------------------
class EdgeTrie {
    struct Node {
        int child[2] = {-1, -1};
        HalfEdge* value = nullptr;
    };
    std::vector<Node> nodes;
    int B;                       // bits par indice de sommet

    uint64_t key(int from, int to) const {
        return (static_cast<uint64_t>(from) << B) | static_cast<uint64_t>(to);
    }

public:
    explicit EdgeTrie(size_t nbVertices) : nodes(1) {
        B = 1;
        while ((size_t(1) << B) < nbVertices) ++B;
    }

    // Retourne false si la cle existe deja (arete orientee dupliquee).
    bool insert(int from, int to, HalfEdge* h) {
        uint64_t k = key(from, to);
        int cur = 0;
        for (int i = 2 * B - 1; i >= 0; --i) {
            int bit = (k >> i) & 1;
            if (nodes[cur].child[bit] == -1) {
                nodes[cur].child[bit] = static_cast<int>(nodes.size());
                nodes.emplace_back();
            }
            cur = nodes[cur].child[bit];
        }
        if (nodes[cur].value) return false;
        nodes[cur].value = h;
        return true;
    }

    HalfEdge* find(int from, int to) const {
        uint64_t k = key(from, to);
        int cur = 0;
        for (int i = 2 * B - 1; i >= 0; --i) {
            int bit = (k >> i) & 1;
            cur = nodes[cur].child[bit];
            if (cur == -1) return nullptr;
        }
        return nodes[cur].value;
    }
};

// ---------------------------------------------------------------
// Triangular mesh
// ---------------------------------------------------------------
class Mesh {
public:
    std::list<Vertex*>   listVertex;
    std::list<Face*>     listFace;
    std::list<HalfEdge*> listHE;

    Mesh(const std::vector<std::array<double, 3>>& V,
         const std::vector<std::array<int, 3>>& F) {
        // 1) Sommets
        std::vector<Vertex*> verts(V.size());
        for (size_t i = 0; i < V.size(); ++i) {
            Vertex* v = new Vertex;
            v->x = V[i][0];
            v->y = V[i][1];
            v->z = V[i][2];
            verts[i] = v;
            listVertex.push_back(v);
        }

        EdgeTrie trie(V.size());

        // 2) Faces et demi-aretes
        for (size_t i = 0; i < F.size(); ++i) {
            Face* f = new Face;
            listFace.push_back(f);

            HalfEdge* h[3];
            for (int k = 0; k < 3; ++k) {
                h[k] = new HalfEdge;
                listHE.push_back(h[k]);
            }
            f->he = h[0];

            for (int k = 0; k < 3; ++k) {
                int from = F[i][k];
                int to   = F[i][(k + 1) % 3];
                if (from < 0 || to < 0 ||
                    size_t(from) >= V.size() || size_t(to) >= V.size())
                    throw std::out_of_range("indice de sommet invalide");

                h[k]->vertex = verts[to];       // cible
                h[k]->face   = f;
                h[k]->next   = h[(k + 1) % 3];
                verts[to]->he = h[k];

                if (!trie.insert(from, to, h[k]))
                    throw std::runtime_error("arete orientee dupliquee (maillage non manifold)");

                // 3) twin : on cherche l'arete opposee (to -> from), O(log N)
                if (HalfEdge* t = trie.find(to, from)) {
                    h[k]->twin = t;
                    t->twin = h[k];
                }
            }
        }
    }

    ~Mesh() {
        for (auto* v : listVertex) delete v;
        for (auto* f : listFace)   delete f;
        for (auto* h : listHE)     delete h;
    }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
};


// Display identifiers

struct Ids {
    std::map<const Vertex*, int>   v;
    std::map<const Face*, int>     f;
    std::map<const HalfEdge*, int> h;
    explicit Ids(const Mesh& m) {
        int i = 0;
        for (auto* x : m.listVertex) v[x] = i++;
        i = 0;
        for (auto* x : m.listFace) f[x] = i++;
        i = 0;
        for (auto* x : m.listHE) h[x] = i++;
    }
};

inline const Vertex* origine(const HalfEdge* h) {
    return h->next->next->vertex;   // face triangulaire
}

// ---------------------------------------------------------------
// Affichage console
// ---------------------------------------------------------------
void afficher(const Mesh& m) {
    Ids id(m);

    std::cout << "=== SOMMETS (avec valence) ===\n";
    for (auto* v : m.listVertex) {
        // on tourne autour du sommet : h -> h->next->twin
        int valence = 0;
        HalfEdge* h = v->he;
        do {
            ++valence;
            h = h->next->twin;
        } while (h && h != v->he);
        std::cout << "  v" << id.v[v] << " = (" << v->x << ", " << v->y << ", "
                  << v->z << ")  valence " << valence << "\n";
    }

    std::cout << "\n=== FACES ===\n";
    for (auto* f : m.listFace) {
        std::cout << "  f" << id.f[f] << " : ";
        HalfEdge* h = f->he;
        do {
            std::cout << "h" << id.h[h] << "(v" << id.v[origine(h)] << "->v"
                      << id.v[h->vertex] << ")  ";
            h = h->next;
        } while (h != f->he);
        std::cout << "\n";
    }

    std::cout << "\n=== DEMI-ARETES ===\n";
    int sansTwin = 0;
    for (auto* h : m.listHE) {
        std::cout << "  h" << id.h[h] << " : v" << id.v[origine(h)] << " -> v"
                  << id.v[h->vertex] << "  face f" << id.f[h->face]
                  << "  next h" << id.h[h->next] << "  twin ";
        if (h->twin) std::cout << "h" << id.h[h->twin] << "\n";
        else { std::cout << "aucun\n"; ++sansTwin; }
    }

    size_t V = m.listVertex.size(), F = m.listFace.size(),
           E = m.listHE.size() / 2;
    std::cout << "\nV=" << V << "  E=" << E << "  F=" << F
              << "  ->  V-E+F = " << long(V) - long(E) + long(F)
              << " (2 pour un maillage ferme de genre 0)\n"
              << "Demi-aretes sans twin : " << sansTwin
              << (sansTwin == 0 ? "  -> maillage ferme\n" : "\n");
}

// ---------------------------------------------------------------
// Export SVG: orthographic view, visible faces only,
// each half-edge = arrow offset towards the interior of its face.
// The twins appear as two opposing arrows on the same edge.
// ---------------------------------------------------------------
void exporterSVG(const Mesh& m, const std::string& nom, double yawDeg,
                 double pitchDeg, const std::string& titre) {
    const double PI = std::acos(-1.0);
    const double a = yawDeg * PI / 180, b = pitchDeg * PI / 180;
    const double S = 230, W = 640, H = 600;

    auto rot = [&](double x, double y, double z) {
        double x1 = x * std::cos(a) - y * std::sin(a);
        double y1 = x * std::sin(a) + y * std::cos(a);
        double y2 = y1 * std::cos(b) - z * std::sin(b);
        double z2 = y1 * std::sin(b) + z * std::cos(b);
        return std::array<double, 3>{x1, y2, z2};
    };
    auto proj = [&](const Vertex* v) {
        auto r = rot(v->x - 0.5, v->y - 0.5, v->z - 0.5);
        return std::array<double, 2>{W / 2 + r[0] * S, H / 2 + 20 - r[2] * S};
    };

    const char* col[] = {"#e6550d", "#3182bd", "#31a354",
                         "#756bb1", "#d6a000", "#e7298a"};
    Ids id(m);
    std::ofstream out(nom);
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='"
        << H << "' font-family='sans-serif'>\n"
        << "<rect width='100%' height='100%' fill='white'/>\n"
        << "<text x='" << W / 2 << "' y='28' text-anchor='middle' font-size='18'>"
        << titre << "</text>\n<defs>\n";
    for (int c = 0; c < 6; ++c)
        out << "<marker id='a" << c
            << "' viewBox='0 0 10 10' refX='9' refY='5' markerWidth='6' "
               "markerHeight='6' orient='auto'><path d='M0,0 L10,5 L0,10 z' fill='"
            << col[c] << "'/></marker>\n";
    out << "</defs>\n";

    std::vector<const Face*> visibles;
    for (auto* f : m.listFace) {
        const HalfEdge* h0 = f->he;
        const Vertex *p0 = origine(h0), *p1 = h0->vertex, *p2 = h0->next->vertex;
        double ux = p1->x - p0->x, uy = p1->y - p0->y, uz = p1->z - p0->z;
        double vx = p2->x - p0->x, vy = p2->y - p0->y, vz = p2->z - p0->z;
        auto n = rot(uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx);
        if (n[1] < -1e-9) visibles.push_back(f);   // face tournee vers la camera
    }

    // face backgrounds
    for (auto* f : visibles) {
        int c = id.f[f] / 2;
        out << "<polygon fill='" << col[c] << "' fill-opacity='0.15' stroke='#444' "
               "stroke-width='1.5' points='";
        const HalfEdge* h = f->he;
        do {
            auto p = proj(h->vertex);
            out << p[0] << "," << p[1] << " ";
            h = h->next;
        } while (h != f->he);
        out << "'/>\n";
    }

    // halfedge
    for (auto* f : visibles) {
        int c = id.f[f] / 2;
        std::array<double, 2> g{0, 0};   // centre de la face
        const HalfEdge* h = f->he;
        do {
            auto p = proj(h->vertex);
            g[0] += p[0] / 3; g[1] += p[1] / 3;
            h = h->next;
        } while (h != f->he);

        do {
            auto o = proj(origine(h)), t = proj(h->vertex);
            double k = 0.17;   // decalage vers l'interieur
            double ox = o[0] + (g[0] - o[0]) * k, oy = o[1] + (g[1] - o[1]) * k;
            double tx = t[0] + (g[0] - t[0]) * k, ty = t[1] + (g[1] - t[1]) * k;
            double x1 = ox + (tx - ox) * 0.10, y1 = oy + (ty - oy) * 0.10;
            double x2 = ox + (tx - ox) * 0.90, y2 = oy + (ty - oy) * 0.90;
            out << "<line x1='" << x1 << "' y1='" << y1 << "' x2='" << x2
                << "' y2='" << y2 << "' stroke='" << col[c]
                << "' stroke-width='2.2' marker-end='url(#a" << c << ")'/>\n";
            double mx = (o[0] + t[0]) / 2, my = (o[1] + t[1]) / 2;
            double lx = mx + (g[0] - mx) * 0.40, ly = my + (g[1] - my) * 0.40;
            out << "<text x='" << lx << "' y='" << ly + 3
                << "' font-size='10' text-anchor='middle' fill='" << col[c]
                << "'>h" << id.h.at(h) << "</text>\n";
            h = h->next;
        } while (h != f->he);
    }

    // vertices (those touching a visible face)
    for (auto* f : visibles) {
        const HalfEdge* h = f->he;
        do {
            auto p = proj(h->vertex);
            out << "<circle cx='" << p[0] << "' cy='" << p[1]
                << "' r='11' fill='white' stroke='black'/>\n"
                << "<text x='" << p[0] << "' y='" << p[1] + 4
                << "' font-size='11' text-anchor='middle' font-weight='bold'>v"
                << id.v.at(h->vertex) << "</text>\n";
            h = h->next;
        } while (h != f->he);
    }

    out << "<text x='" << W / 2 << "' y='" << H - 12
        << "' text-anchor='middle' font-size='13' fill='#555'>fleche = demi-arete "
           "(origine -> cible) ; 2 fleches opposees = twins</text>\n</svg>\n";
}

 
// Interactive HTML export: the cube rotates with the mouse (standalone page)
// The data comes from the half-edge structure.

void exporterHTML(const Mesh& m, const std::string& nom) {
    Ids id(m);
    std::ostringstream d;
    d << "{\"v\":[";
    bool first = true;
    for (auto* v : m.listVertex) {
        d << (first ? "" : ",") << "[" << v->x << "," << v->y << "," << v->z << "]";
        first = false;
    }
    d << "],\"f\":[";
    first = true;
    for (auto* f : m.listFace) {
        d << (first ? "" : ",") << "[";
        const HalfEdge* h = f->he;
        int k = 0;
        do {
            d << (k++ ? "," : "") << id.v.at(h->vertex);
            h = h->next;
        } while (h != f->he);
        d << "]";
        first = false;
    }
    d << "],\"h\":[";   // [origine, cible, face, twin(-1 si bord)]
    first = true;
    for (auto* h : m.listHE) {
        d << (first ? "" : ",") << "[" << id.v.at(origine(h)) << ","
          << id.v.at(h->vertex) << "," << id.f.at(h->face) << ","
          << (h->twin ? id.h.at(h->twin) : -1) << "]";
        first = false;
    }
    d << "]}";

    std::string page = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Maillage half-edge 3D</title>
<style>
body{margin:0;font-family:sans-serif;background:#fafafa;overflow:hidden}
canvas{display:block;cursor:grab}
#ui{position:fixed;top:8px;left:8px;background:#fffd;padding:8px 10px;
border-radius:6px;font-size:13px;line-height:1.7;box-shadow:0 1px 4px #0003}
</style></head><body>
<div id="ui"><b>Glisser</b> : tourner &middot; <b>Molette</b> : zoom<br>
<label><input type="checkbox" id="lab" checked> numeros h</label><br>
<label><input type="checkbox" id="back" checked> demi-aretes des faces cachees</label><br>
<label><input type="checkbox" id="auto"> rotation automatique</label><br>
<button id="reset">reinitialiser la vue</button></div>
<canvas id="c"></canvas>
<script>
const D = __DATA__;
const cv = document.getElementById('c'), ctx = cv.getContext('2d');
const cols = ['#e6550d','#3182bd','#31a354','#756bb1','#d6a000','#e7298a'];
let yaw = 0.6, pitch = 0.45, zoom = 1, drag = false, lx = 0, ly = 0;

function rot(p) {
  const x = p[0]-0.5, y = p[1]-0.5, z = p[2]-0.5;
  const x1 = x*Math.cos(yaw) - y*Math.sin(yaw);
  const y1 = x*Math.sin(yaw) + y*Math.cos(yaw);
  return [x1, y1*Math.cos(pitch) - z*Math.sin(pitch),
              y1*Math.sin(pitch) + z*Math.cos(pitch)];
}
function draw() {
  const W = cv.width, H = cv.height, S = Math.min(W, H)*0.30*zoom, dist = 4;
  ctx.clearRect(0, 0, W, H);
  const R = D.v.map(rot);
  const P = R.map(r => { const k = dist/(dist + r[1]);
    return [W/2 + r[0]*S*k, H/2 - r[2]*S*k]; });
  const showBack = document.getElementById('back').checked;
  const showLab = document.getElementById('lab').checked;

  const order = D.f.map((f, i) => i);
  const depth = i => (R[D.f[i][0]][1] + R[D.f[i][1]][1] + R[D.f[i][2]][1])/3;
  order.sort((a, b) => depth(b) - depth(a));          // du plus loin au plus proche

  for (const fi of order) {
    const f = D.f[fi], c = cols[Math.floor(fi/2)];
    const a = R[f[0]], b = R[f[1]], d = R[f[2]];
    const u = [b[0]-a[0], b[1]-a[1], b[2]-a[2]], v = [d[0]-a[0], d[1]-a[1], d[2]-a[2]];
    const ny = u[2]*v[0] - u[0]*v[2];
    const front = ny < 0;                              // face tournee vers la camera
    ctx.beginPath();
    ctx.moveTo(P[f[0]][0], P[f[0]][1]);
    ctx.lineTo(P[f[1]][0], P[f[1]][1]);
    ctx.lineTo(P[f[2]][0], P[f[2]][1]);
    ctx.closePath();
    ctx.globalAlpha = front ? 0.30 : 0.10;
    ctx.fillStyle = c; ctx.fill();
    ctx.globalAlpha = front ? 0.9 : 0.35;
    ctx.strokeStyle = '#444'; ctx.lineWidth = 1.5; ctx.stroke();

    if (!front && !showBack) continue;
    ctx.globalAlpha = front ? 1 : 0.3;
    const g = [(P[f[0]][0]+P[f[1]][0]+P[f[2]][0])/3, (P[f[0]][1]+P[f[1]][1]+P[f[2]][1])/3];
    D.h.forEach((h, hi) => {
      if (h[2] !== fi) return;
      const o = P[h[0]], t = P[h[1]], k = 0.17;
      const ox = o[0]+(g[0]-o[0])*k, oy = o[1]+(g[1]-o[1])*k;
      const tx = t[0]+(g[0]-t[0])*k, ty = t[1]+(g[1]-t[1])*k;
      const x1 = ox+(tx-ox)*0.10, y1 = oy+(ty-oy)*0.10;
      const x2 = ox+(tx-ox)*0.90, y2 = oy+(ty-oy)*0.90;
      ctx.strokeStyle = ctx.fillStyle = c; ctx.lineWidth = 2.2;
      ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
      const an = Math.atan2(y2-y1, x2-x1), s = 9;
      ctx.beginPath(); ctx.moveTo(x2, y2);
      ctx.lineTo(x2 - s*Math.cos(an-0.4), y2 - s*Math.sin(an-0.4));
      ctx.lineTo(x2 - s*Math.cos(an+0.4), y2 - s*Math.sin(an+0.4));
      ctx.closePath(); ctx.fill();
      if (showLab) {
        const mx = (o[0]+t[0])/2, my = (o[1]+t[1])/2;
        ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
        ctx.fillText('h'+hi, mx+(g[0]-mx)*0.42, my+(g[1]-my)*0.42+3);
      }
    });
  }
  ctx.globalAlpha = 1;
  P.forEach((p, i) => {                                 // sommets
    ctx.beginPath(); ctx.arc(p[0], p[1], 11, 0, 7);
    ctx.fillStyle = 'white'; ctx.fill();
    ctx.strokeStyle = 'black'; ctx.lineWidth = 1; ctx.stroke();
    ctx.fillStyle = 'black'; ctx.font = 'bold 11px sans-serif';
    ctx.textAlign = 'center'; ctx.fillText('v'+i, p[0], p[1]+4);
  });
}
function resize() { cv.width = innerWidth; cv.height = innerHeight; draw(); }
function start(x, y) { drag = true; lx = x; ly = y; cv.style.cursor = 'grabbing'; }
function move(x, y) {
  if (!drag) return;
  yaw += (x-lx)*0.01; pitch += (y-ly)*0.01;
  pitch = Math.max(-1.5, Math.min(1.5, pitch));
  lx = x; ly = y; draw();
}
function stop() { drag = false; cv.style.cursor = 'grab'; }
cv.addEventListener('mousedown', e => start(e.clientX, e.clientY));
addEventListener('mousemove', e => move(e.clientX, e.clientY));
addEventListener('mouseup', stop);
cv.addEventListener('touchstart', e => { start(e.touches[0].clientX, e.touches[0].clientY); e.preventDefault(); }, {passive:false});
cv.addEventListener('touchmove', e => { move(e.touches[0].clientX, e.touches[0].clientY); e.preventDefault(); }, {passive:false});
cv.addEventListener('touchend', stop);
cv.addEventListener('wheel', e => {
  zoom *= e.deltaY < 0 ? 1.1 : 0.9; zoom = Math.max(0.3, Math.min(4, zoom));
  draw(); e.preventDefault();
}, {passive:false});
document.getElementById('reset').onclick = () => { yaw = 0.6; pitch = 0.45; zoom = 1; draw(); };
['lab','back'].forEach(i => document.getElementById(i).onchange = draw);
(function loop() {
  if (document.getElementById('auto').checked && !drag) { yaw += 0.01; draw(); }
  requestAnimationFrame(loop);
})();
addEventListener('resize', resize);
resize();
</script></body></html>
)HTML";

    size_t pos = page.find("__DATA__");
    page.replace(pos, 8, d.str());
    std::ofstream out(nom);
    out << page;
}


// Program: cube (8 vertices, 12 triangles, 36 half-edges)

int main() {
    std::vector<std::array<double, 3>> V = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

    // CCW-oriented triangles viewed from the outside
    std::vector<std::array<int, 3>> F = {
        {0, 3, 2}, {0, 2, 1},   // bas    (z=0)
        {4, 5, 6}, {4, 6, 7},   // haut   (z=1)
        {0, 1, 5}, {0, 5, 4},   // avant  (y=0)
        {3, 7, 6}, {3, 6, 2},   // arriere(y=1)
        {0, 4, 7}, {0, 7, 3},   // gauche (x=0)
        {1, 2, 6}, {1, 6, 5}};  // droite (x=1)

    Mesh m(V, F);
    afficher(m);
    exporterSVG(m, "cube_avant.svg", 30, 25, "Cube - vue avant (demi-aretes)");
    exporterSVG(m, "cube_arriere.svg", 210, 25, "Cube - vue arriere (demi-aretes)");
    exporterHTML(m, "cube3d.html");
    std::cout << "\nImages ecrites : cube_avant.svg et cube_arriere.svg\n"
              << "Vue 3D interactive : cube3d.html (ouvrir dans un navigateur)\n";
}