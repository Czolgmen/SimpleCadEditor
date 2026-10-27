#pragma once

#include <cad_core/model/Document.h>
#include <iosfwd>
#include <optional>
#include <string>

namespace cad::io
{
    struct SceneLoadResult
    {
        std::optional<Document> document;
        std::string error;
    };

    // Versioned, locale-independent text. Preserves IDs, allocation counters,
    // geometry, continuity settings and relations; excludes transient UI state.
    // Both directions validate topology and constraint residuals without moving points.
    SceneLoadResult readScene(std::istream& input);
    bool writeScene(std::ostream& output, const Document& document, std::string& error);
}
