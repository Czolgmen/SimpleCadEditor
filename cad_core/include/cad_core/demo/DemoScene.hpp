#pragma once
#include <cad_core/model/Document.h>


namespace cad {
    // Two editable figures with a Bezier, G1 and G0/G0 arcs, fixed lengths and
    // cross-polygon parallel pairs. Validates and solves before returning.
    Document createDemoScene();
}
