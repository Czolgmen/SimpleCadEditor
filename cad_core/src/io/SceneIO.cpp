#include <cad_core/io/SceneIO.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include <cad_core/geometry/Arc.h>
#include <cad_core/model/ContinuityJoins.h>
#include <cad_core/solver/ConstraintSolver.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
    constexpr std::size_t maxBytes = 16 * 1024 * 1024;
    constexpr std::size_t maxRecords = 100000;

    void check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    std::string token(std::istream& input)
    {
        std::string value;
        check(static_cast<bool>(input >> value), "Nieoczekiwany koniec lub uszkodzona zawartość pliku.");
        return value;
    }

    void expect(std::istream& input, const char* expected)
    {
        check(token(input) == expected, "Niepoprawna kolejność lub nazwa sekcji pliku sceny.");
    }

    template<class T> T number(std::istream& input)
    {
        const auto text = token(input);
        T value{};
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        check(result.ec == std::errc{} && result.ptr == text.data() + text.size(),
            "Niepoprawna liczba w pliku sceny.");
        if constexpr (std::is_floating_point_v<T>)
            check(std::isfinite(value), "Współrzędne i parametry muszą być skończone.");
        return value;
    }

    std::size_t count(std::istream& input, const char* section)
    {
        expect(input, section);
        const auto value = number<std::size_t>(input);
        check(value <= maxRecords, "Za dużo obiektów w pliku sceny (limit 100000 na sekcję).");
        return value;
    }

    cad::Vec2 position(std::istream& input)
    {
        const double x = number<double>(input);
        return {x, number<double>(input)};
    }

    const char* continuityName(cad::Continuity value)
    {
        switch (value)
        {
        case cad::Continuity::G0: return "G0";
        case cad::Continuity::G1: return "G1";
        case cad::Continuity::C1: return "C1";
        default: throw std::runtime_error("Niepoprawny rodzaj ciągłości.");
        }
    }

    cad::Continuity continuity(std::istream& input)
    {
        const auto value = token(input);
        if (value == "G0") return cad::Continuity::G0;
        if (value == "G1") return cad::Continuity::G1;
        if (value == "C1") return cad::Continuity::C1;
        throw std::runtime_error("Nieznany rodzaj ciągłości.");
    }

    template<class Map> void validateIds(const Map& objects, std::uint64_t next)
    {
        check(objects.size() <= maxRecords, "Za dużo obiektów do zapisania.");
        check(next > 0 && next < (std::numeric_limits<std::uint64_t>::max)(), "Niepoprawny licznik identyfikatorów.");
        for (const auto& [id, object] : objects)
            check(id.value > 0 && id.value < next && object.id == id, "Niepoprawny identyfikator obiektu.");
    }
}

namespace cad::io
{
    // Only this codec has access to the complete persistent state. UI and editor
    // still modify the model exclusively through the normal Document interface.
    class SceneArchive
    {
    public:
        static void validate(const Document& document)
        {
            validateIds(document.vertices, document.nextVertexId);
            validateIds(document.edges, document.nextEdgeId);
            validateIds(document.polygons, document.nextPolygonId);
            check(document.validate().empty(), "Scena zawiera niepoprawną geometrię lub połączenia.");
            std::set<VertexId> vertices;
            std::set<EdgeId> edges;
            for (const auto& [id, polygon] : document.polygons)
            {
                for (const auto vertex : polygon.vertices)
                    check(vertices.insert(vertex).second, "Wierzchołek występuje w więcej niż jednym miejscu obrysu.");
                for (const auto edge : polygon.edges)
                    check(edges.insert(edge).second, "Krawędź występuje w więcej niż jednym miejscu obrysu.");
            }
            check(vertices.size() == document.vertices.size() && edges.size() == document.edges.size(),
                "Scena zawiera obiekty nienależące do żadnego wielokąta.");
            for (const auto& [id, vertex] : document.vertices)
            {
                continuityName(vertex.continuity.value);
                check(vertex.continuity.source == ContinuitySource::Default || vertex.continuity.source == ContinuitySource::Explicit,
                    "Niepoprawne źródło ustawienia ciągłości.");
            }
            for (const auto& [id, edge] : document.edges)
            {
                if (const auto* curve = std::get_if<CubicBezier>(&edge.geometry))
                    check(std::isfinite(curve->control1.x) && std::isfinite(curve->control1.y) &&
                        std::isfinite(curve->control2.x) && std::isfinite(curve->control2.y), "Niepoprawne uchwyty Béziera.");
                if (const auto* arc = std::get_if<CircularArc>(&edge.geometry))
                    check(computeArcParameters(document.findVertex(edge.start)->position,
                        document.findVertex(edge.end)->position, arc->bulge).has_value(), "Niepoprawne parametry łuku.");
            }
            const solver::SolveOptions options;
            check(document.fixedLengthConstraints.size() <= maxRecords && document.parallelConstraints.size() <= maxRecords,
                "Za dużo ograniczeń do zapisania.");
            std::set<EdgeId> lengthEdges;
            for (const auto& relation : document.fixedLengthConstraints)
            {
                const auto error = constraint::fixedLengthError(document, relation);
                check(lengthEdges.insert(relation.edge).second && error && std::abs(*error) <= options.lengthTolerance,
                    "Niepoprawne, powtórzone lub niespełnione ograniczenie długości.");
            }
            std::set<std::pair<EdgeId, EdgeId>> parallelPairs;
            for (const auto& relation : document.parallelConstraints)
            {
                const auto pair = std::minmax(relation.first, relation.second);
                const auto error = constraint::parallelError(document, relation);
                check(parallelPairs.emplace(pair.first, pair.second).second && error && std::abs(*error) <= options.parallelTolerance,
                    "Niepoprawne, powtórzone lub niespełnione ograniczenie równoległości.");
            }
            const auto joins = collectContinuityJoins(document);
            check(joins.has_value(), "Niepoprawne ustawienia ciągłości.");
            for (const auto& join : *joins)
            {
                const auto errors = constraint::continuityErrors(document, join);
                check(errors.has_value(), "Nie można wyznaczyć stycznych w połączeniu.");
                for (const double error : *errors)
                    check(std::isfinite(error) && std::abs(error) <=
                        (join.continuity == Continuity::G1 ? options.g1Tolerance : options.c1Tolerance),
                        "Scena zawiera niespełniony warunek ciągłości.");
            }
        }

        static Document decode(std::istream& input)
        {
            expect(input, "CAD_SCENE");
            const auto version = number<unsigned>(input);
            check(version == 1 || version == 2, "Nieobsługiwana wersja pliku sceny.");
            Document document;
            expect(input, "NEXT_IDS");
            document.nextVertexId = number<std::uint64_t>(input);
            document.nextEdgeId = number<std::uint64_t>(input);
            document.nextPolygonId = number<std::uint64_t>(input);
            const auto vertexCount = count(input, "VERTICES");
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                expect(input, "V");
                Vertex vertex;
                vertex.id = VertexId{number<std::uint64_t>(input)};
                vertex.position = position(input);
                vertex.continuity.value = continuity(input);
                const auto source = token(input);
                check(source == "DEFAULT" || source == "EXPLICIT", "Nieznane źródło ustawienia ciągłości.");
                vertex.continuity.source = source == "DEFAULT" ? ContinuitySource::Default : ContinuitySource::Explicit;
                check(document.vertices.emplace(vertex.id, vertex).second, "Powtórzony identyfikator wierzchołka.");
            }
            const auto edgeCount = count(input, "EDGES");
            for (std::size_t i = 0; i < edgeCount; ++i)
            {
                expect(input, "E");
                Edge edge;
                edge.id = EdgeId{number<std::uint64_t>(input)};
                edge.start = VertexId{number<std::uint64_t>(input)};
                edge.end = VertexId{number<std::uint64_t>(input)};
                if (version >= 2) edge.strokeWidth = number<int>(input);
                const auto kind = token(input);
                if (kind == "LINE") edge.geometry = LineSegment{};
                else if (kind == "BEZIER")
                {
                    const auto first = position(input);
                    edge.geometry = CubicBezier{first, position(input)};
                }
                else if (kind == "ARC") edge.geometry = CircularArc{number<double>(input)};
                else throw std::runtime_error("Nieznany typ krawędzi.");
                check(document.edges.emplace(edge.id, edge).second, "Powtórzony identyfikator krawędzi.");
            }
            const auto polygonCount = count(input, "POLYGONS");
            std::size_t totalSlots = 0;
            for (std::size_t i = 0; i < polygonCount; ++i)
            {
                expect(input, "P");
                Polygon polygon;
                polygon.id = PolygonId{number<std::uint64_t>(input)};
                const auto slots = number<std::size_t>(input);
                check(slots >= 3 && slots <= vertexCount && slots <= edgeCount && totalSlots <= vertexCount - slots,
                    "Niepoprawna liczba wierzchołków wielokąta.");
                totalSlots += slots;
                for (std::size_t j = 0; j < slots; ++j) polygon.vertices.push_back(VertexId{number<std::uint64_t>(input)});
                for (std::size_t j = 0; j < slots; ++j) polygon.edges.push_back(EdgeId{number<std::uint64_t>(input)});
                check(document.polygons.emplace(polygon.id, std::move(polygon)).second, "Powtórzony identyfikator wielokąta.");
            }
            const auto lengthCount = count(input, "LENGTHS");
            for (std::size_t i = 0; i < lengthCount; ++i)
            {
                expect(input, "L");
                const EdgeId edge{number<std::uint64_t>(input)};
                document.fixedLengthConstraints.push_back({edge, number<double>(input)});
            }
            const auto parallelCount = count(input, "PARALLELS");
            for (std::size_t i = 0; i < parallelCount; ++i)
            {
                expect(input, "R");
                const EdgeId first{number<std::uint64_t>(input)};
                document.parallelConstraints.push_back({first, EdgeId{number<std::uint64_t>(input)}});
            }
            expect(input, "END");
            std::string trailing;
            check(!(input >> trailing) && input.eof(), "Nadmiarowe dane po końcu sceny.");
            validate(document);
            return document;
        }

        static std::string encode(const Document& document)
        {
            validate(document);
            std::ostringstream output;
            output.imbue(std::locale::classic());
            output << std::setprecision(std::numeric_limits<double>::max_digits10);
            output << "CAD_SCENE 2\nNEXT_IDS " << document.nextVertexId << ' ' << document.nextEdgeId << ' ' << document.nextPolygonId;
            output << "\nVERTICES " << document.vertices.size() << '\n';
            for (const auto& [id, vertex] : document.vertices)
                output << "V " << id.value << ' ' << vertex.position.x << ' ' << vertex.position.y << ' '
                    << continuityName(vertex.continuity.value) << ' '
                    << (vertex.continuity.source == ContinuitySource::Default ? "DEFAULT" : "EXPLICIT") << '\n';
            output << "EDGES " << document.edges.size() << '\n';
            for (const auto& [id, edge] : document.edges)
            {
                output << "E " << id.value << ' ' << edge.start.value << ' ' << edge.end.value << ' ' << edge.strokeWidth << ' ';
                if (const auto* curve = std::get_if<CubicBezier>(&edge.geometry))
                    output << "BEZIER " << curve->control1.x << ' ' << curve->control1.y << ' ' << curve->control2.x << ' ' << curve->control2.y;
                else if (const auto* arc = std::get_if<CircularArc>(&edge.geometry)) output << "ARC " << arc->bulge;
                else output << "LINE";
                output << '\n';
            }
            output << "POLYGONS " << document.polygons.size() << '\n';
            for (const auto& [id, polygon] : document.polygons)
            {
                output << "P " << id.value << ' ' << polygon.vertices.size();
                for (const auto vertex : polygon.vertices) output << ' ' << vertex.value;
                for (const auto edge : polygon.edges) output << ' ' << edge.value;
                output << '\n';
            }
            output << "LENGTHS " << document.fixedLengthConstraints.size() << '\n';
            for (const auto& relation : document.fixedLengthConstraints) output << "L " << relation.edge.value << ' ' << relation.length << '\n';
            output << "PARALLELS " << document.parallelConstraints.size() << '\n';
            for (const auto& relation : document.parallelConstraints) output << "R " << relation.first.value << ' ' << relation.second.value << '\n';
            output << "END\n";
            auto text = output.str();
            check(text.size() <= maxBytes, "Plik sceny przekracza limit 16 MiB.");
            return text;
        }
    };

    SceneLoadResult readScene(std::istream& input)
    {
        try
        {
            std::string text;
            char buffer[4096];
            while (input.read(buffer, sizeof buffer) || input.gcount())
            {
                check(text.size() + static_cast<std::size_t>(input.gcount()) <= maxBytes, "Plik sceny przekracza limit 16 MiB.");
                text.append(buffer, static_cast<std::size_t>(input.gcount()));
            }
            check(input.eof() && !input.bad(), "Nie udało się odczytać całego pliku sceny.");
            std::istringstream parser(text);
            parser.imbue(std::locale::classic());
            return {SceneArchive::decode(parser), {}};
        }
        catch (const std::exception& exception) { return {std::nullopt, exception.what()}; }
    }

    bool writeScene(std::ostream& output, const Document& document, std::string& error)
    {
        error.clear();
        try
        {
            const auto text = SceneArchive::encode(document);
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            check(static_cast<bool>(output), "Nie udało się zapisać całej sceny.");
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
