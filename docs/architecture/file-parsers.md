# File Parser Architecture

## Overview

Qurcuma supports multiple molecular file formats through dedicated parser classes. Each parser extracts atomic coordinates, element symbols, bonds, and optional metadata.

---

## VTF Parser

**File:** `src/vtfparser.cpp/h`

**Format:** VTF (Visualization Toolkit Format)

### Supported Features

- Multi-frame trajectory data
- Atom positions, types, radii
- Bond connectivity
- Unit cell information (optional)

### Data Structures

```cpp
struct VTFAtom {
    int index;              // Atom ID
    QString element;        // Element symbol (H, C, N, O, etc.)
    float radius;           // VTF radius value
    QString type;           // Atom type (for coloring)
    QString name;           // Atom name
    float x, y, z;          // Coordinates
};

struct VTFBond {
    int atom1, atom2;       // Bond between atoms
};

struct VTFFrame {
    QVector<VTFAtom> atoms;
    QVector<VTFBond> bonds;
    bool hasUnitCell = false;
    float cellA, cellB, cellC;
};
```

### Parsing Interface

```cpp
class VTFParser {
public:
    // Parse single frame
    bool parseFile(const QString& filePath, VTFFrame& frame);

    // Parse all frames (trajectory)
    bool parseTrajectory(const QString& filePath);

    int getFrameCount() const;
    bool getFrame(int frameIndex, VTFFrame& frame) const;

    // Conversion to MoleculeViewer format
    static void convertToMoleculeViewer(
        const VTFFrame& vtfFrame,
        QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds);

    // Color & size extraction
    static QColor getAtomColor(const QString& type);
    static float getAtomRadius(float vtfRadius);
};
```

### Color Mapping

VTF atom types map to CPK colors:

| Type | Color | RGB |
|------|-------|-----|
| C | Gray | (169, 169, 169) |
| H | White | (255, 255, 255) |
| N | Blue | (30, 144, 255) |
| O | Red | (255, 0, 0) |
| S | Yellow | (255, 255, 0) |
| P | Orange | (255, 165, 0) |

### Performance

- Single frame parse: ~100ms for 5000 atoms
- Trajectory parse: ~500ms for 3 frames × 5000 atoms
- Memory: ~50MB for 5000 atoms × 3 frames

---

## XYZ Parser

**File:** `src/xyzparser.cpp/h`

**Format:** XYZ (extended molecular geometry format)

### Supported Features

- Multi-frame trajectories
- Element symbols
- Atomic coordinates in Ångströms
- Automatic bond detection (distance-based)

### Data Structures

```cpp
struct XYZAtom {
    QString element;        // Element symbol
    float x, y, z;          // Coordinates (Ångströms)
};

struct XYZFrame {
    int atomCount;
    QString comment;        // Frame comment
    QVector<XYZAtom> atoms;
};
```

### Parsing Interface

```cpp
class XYZParser {
public:
    bool parseTrajectory(const QString& filePath);
    int getFrameCount() const;
    QVector<QVector3D> getPositions(int frameIndex) const;
    QVector<QString> getElements() const;

    // Writing support
    bool writeFile(const QString& filePath, const XYZFrame& frame);
    bool writeTrajectory(const QString& filePath,
                        const QVector<XYZFrame>& frames);
};
```

### Bond Detection

Bonds are automatically detected using covalent radii:

```cpp
// Pseudocode
for (int i = 0; i < N; i++) {
    for (int j = i+1; j < N; j++) {
        float dist = distance(atoms[i], atoms[j]);
        float sumRadii = covalentRadius[elem_i] +
                        covalentRadius[elem_j];

        if (dist < sumRadii * 1.1)  // 10% tolerance
            createBond(i, j);
    }
}
```

### Writing Precision

- 6 decimal places for coordinates (0.000001 Å resolution)
- Format: `element x y z`

### Performance

- Parse 1000-atom file: ~50ms
- Write trajectory: ~100ms
- Automatic bond detection: O(N²) but optimized with spatial hashing

---

## PDB Parser

**File:** `src/pdbparser.cpp/h`

**Format:** PDB (Protein Data Bank format)

### Supported Features

- ATOM/HETATM record parsing
- Fixed-width column format (per PDB specification)
- Multi-model structures (NMR ensembles)
- Explicit connectivity (CONECT records)
- Distance-based bond detection fallback

### PDB Specification

- **ATOM record:** 80 columns, fixed positions
- **HETATM record:** Same format as ATOM
- **CONECT record:** Bond connectivity

```
 1234567890123456789012345678901234567890
ATOM      1  N   ALA A   1      20.154  29.699   5.276  1.00 16.77           N
          ^  ^   ^^^  ^ ^  ^^^  ^^^^^^  ^^^^^^  ^^^^^^  ^^^^  ^^^^           ^^^
        Index|  Name Type|Chain Res#    X        Y       Z    Occupancy  Element
          Seq         Num
```

### Data Structures

```cpp
struct PDBAtom {
    int serialNumber;       // Atom serial number (1-based)
    QString name;           // Atom name (e.g., CA, CB)
    QString resName;        // Residue name (e.g., ALA)
    char chain;             // Chain identifier
    int resSeq;             // Residue sequence number
    float x, y, z;          // Coordinates (Ångströms)
    QString element;        // Element symbol (extracted)
};

struct PDBFrame {
    int modelNumber = 1;
    QVector<PDBAtom> atoms;
};
```

### Parsing Interface

```cpp
class PDBParser {
public:
    bool parseFile(const QString& filePath, PDBFrame& frame);
    QString getLastError() const;
    QVector<Bond> getBonds() const;

    // Conversion
    static void convertToMoleculeViewer(
        const PDBFrame& pdbFrame,
        QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds,
        const QVector<Bond>& explicitBonds);
};

struct Bond {
    int atom1, atom2;       // Serial numbers (1-based)
};
```

### Element Symbol Extraction

PDB files often omit element symbols; extraction uses heuristics:

```cpp
// 1. Use ELEMENT column (newer PDB format)
// 2. Extract from atom name: "CA" → "C", "CB" → "C"
// 3. Heuristics:
//    - "O1P" → "O" (phosphate)
//    - "FE" → "Fe" (iron)
// 4. Default: "C" (carbon)
```

### Bond Detection

Two methods:

1. **Explicit Bonds (CONECT records):**
   - Direct connectivity from file
   - Most reliable

2. **Distance-based (fallback):**
   - Use covalent radii with 1.1× tolerance
   - Used when CONECT records missing

### Performance

- 1000-atom PDB: ~50ms
- 10,000-atom structure: ~500ms
- Memory: ~2MB per 1000 atoms

---

## MOL2 Parser

**File:** `src/mol2parser.cpp/h`

**Format:** MOL2 (Tripos Sybyl MOL2 format)

### Supported Features

- Section-based format (@<TRIPOS>MOLECULE, etc.)
- Sybyl atom types (C.ar, N.3, O.2, etc.)
- Bond types (single, double, triple, aromatic)
- Residue information

### Data Structures

```cpp
struct MOL2Atom {
    int atomNumber;         // 1-based
    QString name;           // Atom name
    QString type;           // Sybyl type (C.ar, N.3, etc.)
    float x, y, z;          // Coordinates
    QString charge;         // Charge information
};

struct MOL2Bond {
    int atom1, atom2;       // 1-based atom numbers
    int bondType;           // 1=single, 2=double, 3=triple, 4=aromatic
};

struct MOL2Molecule {
    QString name;
    QString comment;
    QVector<MOL2Atom> atoms;
    QVector<MOL2Bond> bonds;
    QString type;           // SMALL, PROTEIN, LIGAND, etc.
};
```

### Parsing Interface

```cpp
class MOL2Parser {
public:
    bool parseFile(const QString& filePath, MOL2Molecule& mol);
    QString getLastError() const;

    static void convertToMoleculeViewer(
        const MOL2Molecule& mol2,
        QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds);
};
```

### Sybyl Atom Type to Element Mapping

| Sybyl Type | Element | Count |
|------------|---------|-------|
| C.3, C.2, C.1, C.ar | C | - |
| N.3, N.2, N.1, N.ar | N | - |
| O.2, O.3 | O | - |
| S.2, S.3 | S | - |
| P.3 | P | - |
| H | H | - |
| F, Cl, Br, I | Halogen | - |

### MOL2 Section Structure

```
@<TRIPOS>MOLECULE
benzene
8 8 0 0 0
...

@<TRIPOS>ATOM
1 C1 0.0000 0.0000 0.0000 C.ar 0 BNZ 0.0000
2 C2 1.2000 0.0000 0.0000 C.ar 0 BNZ 0.0000
...

@<TRIPOS>BOND
1 1 2 ar
2 2 3 ar
...
```

### Performance

- Small molecules (<100 atoms): ~10ms
- Medium molecules (100-1000 atoms): ~50ms
- Large ligands (>1000 atoms): ~200ms

---

## Parser Comparison

| Feature | VTF | XYZ | PDB | MOL2 |
|---------|-----|-----|-----|------|
| **Trajectories** | ✅ | ✅ | ✅ Multi-model | ❌ |
| **Explicit Bonds** | ✅ | ❌ | ✅ CONECT | ✅ |
| **Auto Bonds** | ❌ | ✅ | ✅ | ❌ |
| **Element Symbol** | ✅ | ✅ | ✅ Heuristic | ✅ Sybyl type |
| **Async Loading** | ✅ Phase 5D | 🚧 Phase 5E | ✅ Phase 5D | 🚧 Phase 5E |
| **Error Handling** | ⚠️ Limited | ✅ Good | ✅ Excellent | ✅ Good |

---

## Integration with MoleculeViewer

All parsers convert to common format:

```cpp
struct MoleculeViewer::Atom {
    QVector3D position;     // World coordinates
    QString element;        // Element symbol
    QColor color;           // CPK or custom color
    float radius;           // Van der Waals radius
};

struct MoleculeViewer::Bond {
    int atom1, atom2;       // Atom indices (0-based)
};
```

**Conversion Pattern:**

```cpp
// 1. Parse file → Format-specific data structure
PDBFrame pdbFrame;
pdbParser.parseFile("protein.pdb", pdbFrame);

// 2. Convert to MoleculeViewer format
QVector<MoleculeViewer::Atom> atoms;
QVector<MoleculeViewer::Bond> bonds;
PDBParser::convertToMoleculeViewer(pdbFrame, atoms, bonds, bonds);

// 3. Add to viewer
moleculeViewer->addMolecule(atoms, bonds);
```

---

## Error Handling

All parsers follow consistent error handling:

```cpp
if (!parser.parseFile(filePath, frame)) {
    QString error = parser.getLastError();
    // Handle: file not found, parse error, invalid format
}
```

### Common Errors

| Error | Cause | Recovery |
|-------|-------|----------|
| File not found | Invalid path | User provides correct path |
| Parse error | Malformed format | Check file format |
| Unsupported version | Old/new format | Update parser |
| Missing elements | Incomplete data | Use heuristics |

---

## Future Enhancements (Phase 5E+)

- Velocity/forces from trajectory formats
- Multiple trajectory interpolation
- Format auto-detection
- Streaming parser for very large files (>100MB)
- Gzip-compressed file support (.pdb.gz)
