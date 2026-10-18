Projet Half-Edge

Éditeur de maillage 3D interactif en C++17, basé sur la structure de données Half-Edge (demi-arêtes).

Projet réalisé dans le cadre du module C++ & Software Engineering — Université Bourgogne Europe, 2025–2026.

À propos

Un maillage triangulaire est un graphe plongé dans ℝ³ : les sommets portent des positions 3D et les arêtes décrivent la connectivité de la surface. La structure Half-Edge permet de parcourir efficacement les voisinages et, à terme, d'appliquer des modifications topologiques en temps réel : insertion de sommet, découpe d'arête, bascule d'arête, suppression de face.

Chaque arête est découpée en deux demi-arêtes orientées (une par face adjacente). Chaque demi-arête ne stocke que quatre pointeurs :

Champ	Description
target	Sommet d'arrivée
face	Face située à sa gauche
next	Demi-arête suivante dans la même face (sens anti-horaire)
twin	Demi-arête opposée (nullptr sur un bord du maillage)

L'origine d'une demi-arête n'est pas stockée : elle se retrouve via la cible de la demi-arête précédente dans la face (pour un triangle : h->next->next->target).

Fonctionnalités
Lecture de fichiers 3D via Assimp (.obj, .off, etc.) avec triangulation, soudure des sommets dupliqués (tolérance 10⁻⁵) et rejet des faces dégénérées
Construction du Half-Edge à partir des listes de sommets et de faces
Détection des arêtes dupliquées (maillage non orientable ou non-manifold)
Parcours des faces et du voisinage d'un sommet (sommets intérieurs et de bord)
Statistiques et vérification d'Euler (V, E, F, V − E + F, demi-arêtes de bord)
Export SVG des demi-arêtes
Visualisation 3D interactive via une page HTML générée (prototype)
Prérequis
Compilateur C++17 (GCC, Clang ou MSVC)
CMake
Eigen
OpenGL
GLFW
Assimp
Installation
bash
git clone https://github.com/D-Ibrahima/Projet_Halfedge.git
cd Projet_Halfedge
mkdir build && cd build
cmake ..
cmake --build .

Le prototype structure.cpp n'a aucune dépendance externe et peut être compilé seul :

bash
g++ -std=c++17 -O2 structure.cpp -o structure
Utilisation

Lancez l'exécutable généré. Le programme construit un cube (8 sommets, 12 triangles, 36 demi-arêtes), le parcourt, vérifie sa topologie et produit :

les statistiques en console
un schéma SVG des demi-arêtes
une page HTML de visualisation 3D interactive

Exemple de sortie pour le cube :

V = 8, E = 18, F = 12
V - E + F = 2
Demi-arêtes sans jumelle : 0
Structure du projet
.
├── halfedge.hpp          # Structure Half-Edge et conteneur HalfEdgeMesh
├── lecture.hpp           # Lecture de fichiers 3D (Assimp)
├── structure.cpp         # Prototype autonome (console, SVG, HTML interactif)
├── CMakeLists.txt        # Configuration CMake (C++17)
├── roadmap_halfedge.tex  # Roadmap
├── roadmap_mentale.jpeg  # Carte mentale (XMind)
├── Gant.jpeg             # Diagramme de Gantt
└── uml diagram           # Lien vers l'UML (Lucidchart)
Architecture
Un seul propriétaire : HalfEdgeMesh possède tous les sommets, faces et demi-arêtes dans trois std::vector. Les autres structures ne portent que des pointeurs bruts non propriétaires.
Pas de réallocation après câblage : les vecteurs sont dimensionnés une seule fois, les pointeurs restent donc valides.
Copie interdite : constructeur et affectation par copie supprimés (= delete).
Lecture séparée de la topologie : lireMaillage remplit une structure intermédiaire (points + polygones), puis construire() câble le Half-Edge.

À terme, le rendu et la sélection n'accéderont au maillage qu'en lecture (const), et toute modification passera par une commande (patron Command) enregistrée dans un historique pour fournir l'annulation et le rétablissement.

Feuille de route
 M0 — Dépôt, UML, roadmap, CMake
 M1 — Parsing et construction du graphe Half-Edge (en grande partie)
 M1 — Export de fichier, checkIntegrity(), tests unitaires
 M2a — Fenêtre OpenGL/GLFW, rendu filaire et ombré, caméra orbitale
 M2b — Sélection de sommets, arêtes et faces
 M3 — Création de face, découpe et bascule d'arête, suppression
 M4 — Undo/Redo, séparation données / rendu / interaction
 Rendu final — Rapport LaTeX, manuel, dépôt propre
Limitations connues
Deux implémentations coexistent (structure.cpp et halfedge.hpp) avec une convention différente pour le pointeur de sommet (demi-arête entrante vs sortante) ; elles doivent être unifiées.
lecture.hpp et halfedge.hpp ne sont pas encore appelés depuis le programme principal.
Pas encore d'export de fichier, de checkIntegrity() ni de tests unitaires.
Le rendu actuel est une page HTML ; la fenêtre OpenGL/GLFW reste à écrire.
Auteurs
DIAKITE Ibrahima Sory
Tania
Licence

À définir.
