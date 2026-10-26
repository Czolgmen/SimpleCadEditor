#pragma once

namespace cad
{
    class Document;

    // Restores signed semicircles for G0/G0 and fits the single G1 end to its
    // neighboring edge. For two G1-connected arcs, the lower EdgeId keeps its bulge.
    // Validates cycles, references, positions and arc settings. Existing bulges
    // must be finite and nonzero. Failure changes nothing; success changes only bulges.
    bool updateArcBulges(Document& document);
} // namespace cad
