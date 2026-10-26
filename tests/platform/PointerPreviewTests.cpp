#include "framework.h"

#include <iostream>
#include <stdexcept>
#include <string>

static bool failTimerAllocation = false;
static unsigned presentRequests = 0;
static unsigned messageBoxRequests = 0;
static std::wstring lastMessageBoxText;
static int WINAPI testMessageBoxW(HWND, LPCWSTR text, LPCWSTR, UINT)
{
    ++messageBoxRequests;
    lastMessageBoxText = text;
    return IDOK;
}
static BOOL WINAPI testUpdateWindow(HWND window)
{
    ++presentRequests;
    return ::UpdateWindow(window);
}
static UINT_PTR WINAPI testSetTimer(HWND window, UINT_PTR id, UINT interval, TIMERPROC callback)
{
    return failTimerAllocation ? 0 : ::SetTimer(window, id, interval, callback);
}

// Exercise the actual window procedure, including its private timer ID. Inject
// timer-allocation failure and record modal messages without opening dialogs.
// Count presentation requests because
// Windows omits painting for hidden test windows; still call the real WinAPI.
#define SetTimer testSetTimer
#define UpdateWindow testUpdateWindow
#define MessageBoxW testMessageBoxW
#include "../../Projecr1_GK.cpp"
#undef SetTimer
#undef UpdateWindow
#undef MessageBoxW
#include "../CurveTestSupport.h"
#include <cad_core/model/EdgeSampling.h>
#include <cad_core/model/ContinuityJoins.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include <platform/win32/DibRenderer.h>

#include <algorithm>
#include <limits>

namespace
{
    using namespace cad;
    using namespace cad::tests;
    struct Fixture
    {
        win32::AppState state;
        ControlPointRef control;
        HWND window = nullptr;

        Fixture(render::LineAlgorithm algorithm = render::LineAlgorithm::Gdi)
        {
            state.lineAlgorithm = algorithm;
            Document document;
            const std::array positions{Vec2{80, 80}, Vec2{240, 80}, Vec2{240, 200}, Vec2{80, 200}};
            const auto polygon = *document.findPolygon(document.addPolygon(positions));
            control = {polygon.edges[0], BezierControlPoint::First};
            require(document.convertEdgeToBezier(control.edge) &&
                document.moveControlPoint(control, {80, 40}) &&
                document.moveControlPoint({control.edge, BezierControlPoint::Second}, {240, 40}) &&
                document.setFixedLength(polygon.edges[1], 120), "Preview fixture failed");
            state.editor = editor::EditorSession{document};
            window = CreateWindowExW(0, L"CadPointerPreviewTests", L"", WS_OVERLAPPEDWINDOW,
                0, 0, 640, 480, nullptr, nullptr, hInst, &state);
            require(window != nullptr, "Could not create test window");
        }

        ~Fixture() { if (window) DestroyWindow(window); }

        LPARAM position(Vec2 world) const
        {
            RECT rect{};
            require(GetClientRect(window, &rect) != FALSE, "Missing client rectangle");
            const POINT screen{rect.left + 20 + std::lround(world.x),
                rect.bottom - 20 - std::lround(world.y)};
            return MAKELPARAM(static_cast<WORD>(screen.x), static_cast<WORD>(screen.y));
        }

        void begin()
        {
            SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, position({80, 40}));
            require(state.editor.isDragging() && state.editor.selection().controlPoint == control,
                "Handle gesture did not start");
            require(GetCapture() == window, "Test window did not capture the mouse");
        }

        void move(Vec2 point) { SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, position(point)); }
        void tick() { SendMessageW(window, WM_TIMER, pointerFrameTimer, 0); }
        void release(Vec2 point) { SendMessageW(window, WM_LBUTTONUP, 0, position(point)); }

        void requireStopped() const
        {
            require(!state.pointerTimerActive && !state.pendingPointerPosition && !state.editor.isDragging(),
                "Finished gesture retained scheduled work");
        }
    };

    void burstUsesOnlyLatestPositionAndPresents()
    {
        Fixture f;
        f.begin();
        require(f.state.pointerTimerActive, "Preview timer not started");
        const Document before = f.state.editor.document();
        for (int i = 1; i <= 100; ++i)
            f.move({80.0 + i, 40.0 + i});
        requireSameCurveDocument(f.state.editor.document(), before);
        require(f.state.pendingPointerPosition.has_value(), "Latest sample missing");
        requireSameCurvePoint(*f.state.pendingPointerPosition, {180, 140});
        auto expected = editor::EditorSession{before};
        require(expected.pointerDown({80, 40}) && expected.pointerMove({180, 140}), "Reference gesture failed");
        const auto beforePaints = presentRequests;
        f.tick();
        requireSameCurveDocument(f.state.editor.document(), expected.document());
        require(!f.state.pendingPointerPosition && presentRequests > beforePaints, "Tick did not present the latest sample");
        const auto afterPaints = presentRequests;
        f.tick();
        requireSameCurveDocument(f.state.editor.document(), expected.document());
        require(presentRequests == afterPaints, "Idle tick unnecessarily repainted");
    }

    void releaseBeforeTickUsesFinalPosition()
    {
        Fixture f;
        f.begin();
        f.move({100, 50});
        f.release({120, 60});
        f.requireStopped();
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {120, 60});
        const Document committed = f.state.editor.document();
        f.tick();
        requireSameCurveDocument(f.state.editor.document(), committed);
        require(!f.state.editor.cancelInteraction(), "Released gesture can still be rolled back");
    }

    void releaseAfterPreviewFlushesNewerPosition()
    {
        Fixture f;
        f.begin();
        f.move({100, 50});
        f.tick();
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {100, 50});
        f.move({110, 55});
        f.release({120, 60});
        f.requireStopped();
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {120, 60});
    }

    void cancellationDiscardsPendingWorkAndRestoresSnapshot()
    {
        for (const UINT message : {WM_KEYDOWN, WM_CANCELMODE, WM_KILLFOCUS, WM_SIZE})
        {
            Fixture f;
            const Document before = f.state.editor.document();
            f.begin();
            f.move({100, 50});
            f.tick();
            f.move({120, 60});
            const WPARAM parameter = message == WM_KEYDOWN ? VK_ESCAPE : message == WM_SIZE ? SIZE_MINIMIZED : 0;
            SendMessageW(f.window, message, parameter, 0);
            f.requireStopped();
            f.tick();
            requireSameCurveDocument(f.state.editor.document(), before);
        }
    }

    void captureLossDiscardsPendingWork()
    {
        Fixture f;
        const Document before = f.state.editor.document();
        f.begin();
        f.move({100, 50});
        f.tick();
        f.move({120, 60});
        ReleaseCapture();
        f.requireStopped();
        f.tick();
        requireSameCurveDocument(f.state.editor.document(), before);
    }

    void rejectionRetainsPreviewAndAllowsRecovery()
    {
        Fixture f;
        f.begin();
        const Document before = f.state.editor.document();
        f.move({80, 80}); // Zero G1 tangent: invalid, not a partially valid preview.
        f.tick();
        require(f.state.showConsistencyWarning && f.state.editor.isDragging(), "Rejected frame lost warning or gesture");
        requireSameCurveDocument(f.state.editor.document(), before);
        f.move({100, 50});
        f.tick();
        require(!f.state.showConsistencyWarning, "Accepted frame retained warning");
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {100, 50});
    }

    void arcContinuityMenuDisablesC1OnlyForArcVertices()
    {
        for (const bool arc : {false, true})
        {
            Document document;
            const std::array positions{Vec2{0, 0}, Vec2{50, 0}, Vec2{50, 50}, Vec2{0, 50}};
            const auto polygon = *document.findPolygon(document.addPolygon(positions));
            if (arc)
                const_cast<Edge*>(document.findEdge(polygon.edges[0]))->geometry = CircularArc{};
            else
                require(document.convertEdgeToBezier(polygon.edges[0]), "Menu Bezier fixture failed");
            editor::EditorSession editor(document);
            require(editor.selectAt({0, 0}, 0), "Menu vertex selection failed");
            HMENU menu = CreatePopupMenu();
            require(menu != nullptr, "Menu allocation failed");
            const bool appended = appendContinuityMenu(menu, editor);
            const HMENU submenu = GetSubMenu(menu, 0);
            const UINT c1 = submenu ? GetMenuState(submenu, IDM_CONTINUITY_C1, MF_BYCOMMAND) : UINT(-1);
            const UINT g0 = submenu ? GetMenuState(submenu, IDM_CONTINUITY_G0, MF_BYCOMMAND) : UINT(-1);
            const UINT g1 = submenu ? GetMenuState(submenu, IDM_CONTINUITY_G1, MF_BYCOMMAND) : UINT(-1);
            DestroyMenu(menu);
            require(appended && c1 != UINT(-1) && g0 != UINT(-1) && g1 != UINT(-1), "Continuity submenu missing");
            require(((c1 & MF_GRAYED) != 0) == arc, "C1 availability does not match incident geometry");
            require((g0 & (MF_GRAYED | MF_DISABLED)) == 0 && (g1 & (MF_GRAYED | MF_DISABLED)) == 0,
                "Arc G0/G1 menu disabled");
        }
    }

    Vec2 edgeMiddle(const Document& document, EdgeId id)
    {
        const auto* edge = document.findEdge(id);
        const auto points = sampleEdge(edge->geometry, document.findVertex(edge->start)->position,
            document.findVertex(edge->end)->position);
        return points.size() == 2 ? (points.front() + points.back()) * 0.5 : points[points.size() / 2];
    }

    void edgeConversionMenuMatchesSelectedGeometry()
    {
        for (int kind = 0; kind < 4; ++kind)
        {
            Document document;
            const std::array positions{Vec2{80, 80}, Vec2{240, 80}, Vec2{240, 200}, Vec2{80, 200}};
            const auto polygon = *document.findPolygon(document.addPolygon(positions));
            const EdgeId id = polygon.edges[0];
            auto* edge = const_cast<Edge*>(document.findEdge(id));
            if (kind == 2) edge->geometry = CubicBezier{{120, 40}, {200, 40}};
            if (kind == 3) edge->geometry = CircularArc{};
            editor::EditorSession editor(document);
            if (kind != 0)
                require(editor.selectAt(edgeMiddle(document, id), 0) && editor.selection().edge == id,
                    "Conversion menu edge selection failed");
            HMENU menu = CreatePopupMenu();
            require(menu != nullptr, "Conversion menu allocation failed");
            const bool appended = appendEdgeConversionMenu(menu, editor);
            const std::array states{GetMenuState(menu, IDM_CONVERT_TO_SEGMENT, MF_BYCOMMAND),
                GetMenuState(menu, IDM_CONVERT_TO_BEZIER, MF_BYCOMMAND), GetMenuState(menu, IDM_CONVERT_TO_ARC, MF_BYCOMMAND)};
            DestroyMenu(menu);
            require(appended, "Conversion menu append failed");
            for (std::size_t i = 0; i < states.size(); ++i)
            {
                require(states[i] != UINT(-1), "Conversion command missing");
                const bool enabled = (states[i] & (MF_GRAYED | MF_DISABLED)) == 0;
                require(enabled == (kind != 0 && kind != static_cast<int>(i + 1)), "Wrong conversion menu availability");
            }
        }
    }

    void arcConversionCommandsRoundTripThroughWindowProcedure()
    {
        Fixture f;
        Document document = f.state.editor.document();
        const auto polygon = document.getPolygons().begin()->second;
        const EdgeId id = polygon.edges[2];
        require(document.setFixedLength(id, 160), "Command conversion length fixture failed");
        f.state.editor = editor::EditorSession(document);
        require(f.state.editor.selectAt(edgeMiddle(document, id), 0) && f.state.editor.selection().edge == id,
            "Command edge selection failed");
        const unsigned messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_CONVERT_TO_ARC, 0), 0);
        require(messageBoxRequests == messages && std::holds_alternative<CircularArc>(f.state.editor.document().findEdge(id)->geometry),
            "Arc WM_COMMAND did not convert selected edge");
        require(!f.state.editor.selectedEdgeLength() && !f.state.editor.selectedFixedLength(), "Arc retained segment length relation");
        const auto& result = f.state.editor.document();
        const Vec2 middle = edgeMiddle(result, id);
        require(editor::hitTestEdge(result, middle, 0) == id, "Converted arc was not hit-tested on its curve");
        f.requireStopped();
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_CONVERT_TO_SEGMENT, 0), 0);
        require(messageBoxRequests == messages && std::holds_alternative<LineSegment>(f.state.editor.document().findEdge(id)->geometry),
            "Segment WM_COMMAND did not restore arc chord");
        document.removeFixedLength(id);
        requireSameCurveDocument(f.state.editor.document(), document);
        require(f.state.editor.selection().edge == id, "Conversion lost selection");
    }

    void failedArcCommandShowsReasonAndPreservesDocument()
    {
        Fixture f;
        const EdgeId id = f.control.edge; // Default Bezier G1 at both endpoints.
        require(f.state.editor.selectAt(edgeMiddle(f.state.editor.document(), id), 0) &&
            f.state.editor.selection().edge == id, "Rejected command selection failed");
        const Document before = f.state.editor.document();
        const unsigned messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_CONVERT_TO_ARC, 0), 0);
        require(messageBoxRequests == messages + 1 && lastMessageBoxText.find(L"G0/G1") != std::wstring::npos,
            "Rejected arc conversion did not explain allowed continuity");
        requireSameCurveDocument(f.state.editor.document(), before);
        require(f.state.editor.selection().edge == id, "Rejected command lost selection");
    }

    void arcSplitCommandCommitsTwoSegmentsAndSelectsMidpoint()
    {
        for (const double bulge : {-1.0, 1.0})
        {
            Fixture f;
            Document initial = f.state.editor.document();
            const auto polygon = initial.getPolygons().begin()->second;
            const EdgeId id = polygon.edges[2];
            require(initial.convertEdgeToArc(id) && initial.setArcBulge(id, bulge), "Window arc split fixture failed");
            f.state.editor = editor::EditorSession(initial);
            require(f.state.editor.selectAt(edgeMiddle(initial, id), 0) && f.state.editor.selection().edge == id &&
                f.state.editor.canSplitSelectedEdge(), "Window arc split unavailable");
            Document expected = initial;
            const auto inserted = expected.splitEdge(id);
            require(inserted.has_value(), "Expected model arc split failed");
            const unsigned messages = messageBoxRequests;
            SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_SPLIT_EDGE, 0), 0);
            require(messageBoxRequests == messages, "Successful arc split displayed an error");
            requireSameCurveDocument(f.state.editor.document(), expected);
            require(f.state.editor.selection().vertex == inserted && !f.state.editor.selection().edge,
                "Window arc split did not select the midpoint");
            const auto* vertex = f.state.editor.document().findVertex(*inserted);
            requireSameCurvePoint(vertex->position, {160, 200 + 80 * bulge});
            require(vertex->continuity == ContinuitySetting{Continuity::G0, ContinuitySource::Default},
                "Window arc split lost G0/Default");
            f.requireStopped();
        }
    }

    void arcSplitCommandRefitsRemainingG1Arc()
    {
        Fixture f;
        Document initial;
        const std::array positions{Vec2{80, 80}, Vec2{240, 80}, Vec2{240, 200}, Vec2{80, 200}};
        const auto polygon = *initial.findPolygon(initial.addPolygon(positions));
        const EdgeId reference = polygon.edges[0], divided = polygon.edges[1];
        require(initial.convertEdgeToArc(reference) && initial.convertEdgeToArc(divided) &&
            initial.setContinuity(polygon.vertices[1], Continuity::G1) && initial.setArcBulge(reference, 0.5) &&
            solver::solveConstraints(initial, {}, {}, polygon.vertices[1]).status == solver::SolveStatus::Converged,
            "Window G1 arc pair fixture failed");
        f.state.editor = editor::EditorSession(initial);
        require(f.state.editor.selectAt(edgeMiddle(initial, divided), 0) && f.state.editor.selection().edge == divided &&
            f.state.editor.canSplitSelectedEdge(), "Window G1 arc split unavailable");
        Document expected = initial;
        const auto inserted = expected.splitEdge(divided);
        require(inserted.has_value(), "Expected G1 model split failed");
        const unsigned messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_SPLIT_EDGE, 0), 0);
        require(messageBoxRequests == messages && f.state.editor.selection().vertex == inserted, "G1 split command failed");
        const double bulge = std::get<CircularArc>(f.state.editor.document().findEdge(reference)->geometry).bulge;
        require(std::abs(bulge - 0.5) > 1e-3 && expected.setArcBulge(reference, bulge), "G1 command did not refit surviving arc");
        requireSameCurveDocument(f.state.editor.document(), expected);
        const auto joins = collectContinuityJoins(f.state.editor.document());
        require(joins && joins->size() == 1, "Window split lost G1 join");
        const auto errors = constraint::continuityErrors(f.state.editor.document(), joins->front());
        require(errors && errors->size() == 1 && std::abs(errors->front()) <= 1e-6, "Window split left unsatisfied G1");
        f.requireStopped();
    }

    void rejectedArcSplitCommandShowsErrorAndKeepsState()
    {
        Fixture f;
        Document initial = f.state.editor.document();
        const EdgeId id = initial.getPolygons().begin()->second.edges[2];
        require(initial.convertEdgeToArc(id), "Rejected split arc fixture failed");
        const std::array positions{Vec2{320, 20}, Vec2{340, 20}, Vec2{330, 40}};
        const auto triangle = *initial.findPolygon(initial.addPolygon(positions));
        for (std::size_t i = 0; i < 3; ++i)
            require(initial.setFixedLength(triangle.edges[i], i == 2 ? 30 : 10), "Rejected split conflict fixture failed");
        f.state.editor = editor::EditorSession(initial);
        require(f.state.editor.selectAt(edgeMiddle(initial, id), 0) && f.state.editor.selection().edge == id &&
            f.state.editor.canSplitSelectedEdge(), "Rejected arc split command unavailable");
        const unsigned messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_SPLIT_EDGE, 0), 0);
        require(messageBoxRequests == messages + 1 && lastMessageBoxText.find(L"podzielić") != std::wstring::npos,
            "Rejected arc split did not display its error");
        requireSameCurveDocument(f.state.editor.document(), initial);
        require(f.state.editor.selection().edge == id && f.state.editor.canSplitSelectedEdge(), "Rejected split lost arc selection");
        Document actualProbe = f.state.editor.document(), expectedProbe = initial;
        const std::array probe{Vec2{400, 0}, Vec2{420, 0}, Vec2{400, 20}};
        require(actualProbe.addPolygon(probe) == expectedProbe.addPolygon(probe), "Rejected window split consumed IDs");
        requireSameCurveDocument(actualProbe, expectedProbe);
        f.requireStopped();
    }

    struct DibFixture
    {
        win32::BackBuffer buffer;
        std::vector<render::PixelPoint> scratch{{999, 999}, {-999, -999}};
        static constexpr std::uint32_t background = 0x00EEEEEE;

        DibFixture()
        {
            const HDC reference = GetDC(nullptr);
            require(reference != nullptr, "Could not acquire reference DC");
            const bool created = buffer.resize(reference, 8, 6);
            ReleaseDC(nullptr, reference);
            require(created && buffer.pixels(), "Could not create test DIB");
            std::fill_n(buffer.pixels(), 48, background);
        }
    };

    void dibRasterClipsAndChangesOnlyExpectedPixels()
    {
        DibFixture f;
        constexpr std::uint32_t horizontal = 0x00123456, vertical = 0x00765432, point = 0x000078D7;
        win32::drawBresenhamLine(f.buffer, {-1e12, 1.2}, {1e12, 1.2}, horizontal, f.scratch);
        win32::drawBresenhamLine(f.buffer, {5.2, -1e12}, {5.2, 1e12}, vertical, f.scratch);
        win32::drawBresenhamLine(f.buffer, {7, 5}, {7, 5}, point, f.scratch);
        require(f.scratch.size() == 1, "Working vector retained pixels from previous lines");
        for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
        {
            auto expected = DibFixture::background;
            if (y == 1) expected = horizontal;
            if (x == 5) expected = vertical;
            if (x == 7 && y == 5) expected = point;
            require(f.buffer.pixels()[y * 8 + x] == expected, "DIB line changed an unexpected pixel or row");
        }
    }

    void dibRasterUsesRgbPackingAndPreservesGdiBackground()
    {
        DibFixture f;
        const RECT rect{0, 0, 8, 6};
        require(FillRect(f.buffer.dc(), &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH))) != 0,
            "GDI background fill failed");
        GdiFlush(); // The drawing batch owner synchronizes GDI before CPU writes.
        win32::drawBresenhamLine(f.buffer, {1.2, 2.2}, {4.2, 2.2}, 0x00123456, f.scratch);
        require(GetPixel(f.buffer.dc(), 1, 2) == RGB(0x12, 0x34, 0x56), "DIB color channels are reversed");
        require(GetPixel(f.buffer.dc(), 4, 2) == RGB(0x12, 0x34, 0x56), "Raster lost last endpoint");
        require(GetPixel(f.buffer.dc(), 1, 1) == RGB(255, 255, 255), "Raster overwrote GDI background");
        GdiFlush();
    }

    void dibRasterRejectsInvalidLinesAndHandlesBufferLifecycle()
    {
        DibFixture f;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double infinity = std::numeric_limits<double>::infinity();
        for (const auto segment : {std::array{Vec2{-10, -10}, Vec2{-1, -1}},
            std::array{Vec2{20, 20}, Vec2{30, 30}}, std::array{Vec2{nan, 0}, Vec2{4, 4}},
            std::array{Vec2{0, 0}, Vec2{infinity, 4}}})
            win32::drawBresenhamLine(f.buffer, segment[0], segment[1], 0x00123456, f.scratch);
        require(std::all_of(f.buffer.pixels(), f.buffer.pixels() + 48,
            [](std::uint32_t color) { return color == DibFixture::background; }),
            "Rejected line changed DIB memory");

        f.buffer.reset();
        require(!f.buffer.pixels() && f.buffer.width() == 0 && f.buffer.height() == 0,
            "Reset retained stale pixel storage");
        win32::drawBresenhamLine(f.buffer, {0, 0}, {7, 5}, 0x00123456, f.scratch);
        const HDC reference = GetDC(nullptr);
        require(reference != nullptr, "Could not acquire resize DC");
        const bool resized = f.buffer.resize(reference, 1, 1);
        ReleaseDC(nullptr, reference);
        require(resized && f.buffer.pixels() && f.buffer.width() == 1 && f.buffer.height() == 1,
            "Resize did not publish replacement pixel storage");
        f.buffer.pixels()[0] = DibFixture::background;
        win32::drawBresenhamLine(f.buffer, {-10, -10}, {10, 10}, 0x00123456, f.scratch);
        require(f.buffer.pixels()[0] == 0x00123456 && f.scratch.size() == 1,
            "Single-pixel buffer did not receive clipped point");
    }

    void radioControlsSwitchOnlyTheView()
    {
        Fixture f;
        const HWND gdi = GetDlgItem(f.window, IDC_LINE_GDI);
        const HWND bresenham = GetDlgItem(f.window, IDC_LINE_BRESENHAM);
        require(gdi && bresenham &&
            (GetWindowLongPtrW(gdi, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON &&
            (GetWindowLongPtrW(bresenham, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON,
            "Native algorithm radio controls missing");
        require(SendMessageW(gdi, BM_GETCHECK, 0, 0) == BST_CHECKED &&
            SendMessageW(bresenham, BM_GETCHECK, 0, 0) == BST_UNCHECKED, "Wrong default radio selection");
        const auto edge = f.state.editor.document().getPolygons().begin()->second.edges[1];
        require(f.state.editor.selectAt(edgeMiddle(f.state.editor.document(), edge), 0) &&
            f.state.editor.beginParallelSelection(), "View switch selection fixture failed");
        const Document before = f.state.editor.document();
        f.state.showConsistencyWarning = true;
        SendMessageW(f.window, WM_KILLFOCUS, reinterpret_cast<WPARAM>(bresenham), 0);
        for (const HWND control : {bresenham, bresenham, gdi})
        {
            SendMessageW(control, BM_CLICK, 0, 0);
            const bool useBresenham = control == bresenham;
            require(f.state.lineAlgorithm == (useBresenham ? render::LineAlgorithm::Bresenham : render::LineAlgorithm::Gdi),
                "Radio did not change selected algorithm");
            require(SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED &&
                SendMessageW(useBresenham ? gdi : bresenham, BM_GETCHECK, 0, 0) == BST_UNCHECKED,
                "Radio group lost exclusive check");
            requireSameCurveDocument(f.state.editor.document(), before);
            require(f.state.editor.selection().edge == edge && f.state.editor.pendingParallelEdge() == edge &&
                f.state.showConsistencyWarning, "View switch changed editing state");
        }
        Fixture initiallyBresenham{render::LineAlgorithm::Bresenham};
        require(SendMessageW(GetDlgItem(initiallyBresenham.window, IDC_LINE_BRESENHAM), BM_GETCHECK, 0, 0) == BST_CHECKED,
            "Window creation ignored initial renderer mode");
    }

    struct RenderFixture
    {
        win32::BackBuffer buffer;
        Document document;
        cad::Polygon polygon;
        const RECT rect{0, 0, 320, 240};

        RenderFixture()
        {
            const HDC reference = GetDC(nullptr);
            require(reference != nullptr, "Could not acquire renderer reference DC");
            const bool created = buffer.resize(reference, rect.right, rect.bottom);
            ReleaseDC(nullptr, reference);
            require(created, "Could not create renderer DIB");
            const std::array points{Vec2{40, 40}, Vec2{220, 40}, Vec2{220, 180}, Vec2{40, 180}};
            polygon = *document.findPolygon(document.addPolygon(points));
        }

        void draw(render::LineAlgorithm algorithm, std::optional<EdgeId> selected = {},
            std::optional<EdgeId> pending = {}, bool passBuffer = true)
        {
            FillRect(buffer.dc(), &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            win32::drawDocument(buffer.dc(), document, rect, {}, {}, selected, pending, {},
                algorithm, passBuffer ? &buffer : nullptr);
            GdiFlush();
        }

        std::uint32_t pixel(int x, int y)
        {
            return buffer.pixels()[y * rect.right + x] & 0x00FFFFFF;
        }
    };

    void rendererDispatchesSegmentsBezierAndArcs()
    {
        RenderFixture f;
        require(f.document.convertEdgeToBezier(f.polygon.edges[0]) &&
            f.document.moveControlPoint({f.polygon.edges[0], BezierControlPoint::First}, {60, 120}) &&
            f.document.moveControlPoint({f.polygon.edges[0], BezierControlPoint::Second}, {160, 120}),
            "Renderer Bezier fixture failed");
        for (const auto vertex : f.polygon.vertices)
            require(f.document.setContinuity(vertex, Continuity::G0), "Renderer G0 fixture failed");
        require(f.document.convertEdgeToArc(f.polygon.edges[1]), "Renderer arc fixture failed");
        for (const auto algorithm : {render::LineAlgorithm::Gdi, render::LineAlgorithm::Bresenham})
        {
            f.draw(algorithm);
            // Analytic midpoint of the cubic, rightmost point of the semicircle,
            // and midpoint of a straight edge; none lies on a control guide.
            require(f.pixel(135, 120) == 0, "Selected algorithm missed Bezier midpoint");
            require(f.pixel(310, 110) == 0, "Selected algorithm missed arc midpoint");
            require(f.pixel(150, 40) == 0, "Selected algorithm missed straight edge");
            require(f.pixel(135, 180) == 0x00FFFFFF, "Renderer drew Bezier chord");
            require(f.pixel(240, 110) == 0x00FFFFFF, "Renderer drew arc chord");
        }
        f.draw(render::LineAlgorithm::Bresenham, f.polygon.edges[0]);
        require(f.pixel(135, 120) == 0x000078D7, "Bezier selection lost blue color");
        f.draw(render::LineAlgorithm::Bresenham, f.polygon.edges[1], f.polygon.edges[1]);
        require(f.pixel(310, 110) == 0x00DC7800, "Pending parallel color did not override selection");
    }

    void rendererPreservesWidthsAndFallsBackToGdi()
    {
        RenderFixture f;
        const auto edge = f.polygon.edges[2];
        f.draw(render::LineAlgorithm::Bresenham);
        require(f.pixel(150, 39) == 0 && f.pixel(150, 40) == 0 &&
            f.pixel(150, 38) == 0x00FFFFFF && f.pixel(150, 41) == 0x00FFFFFF,
            "Normal Bresenham stroke is not two pixels wide");
        f.draw(render::LineAlgorithm::Bresenham, edge);
        for (int y = 38; y <= 41; ++y)
            require(f.pixel(150, y) == 0x000078D7, "Selected Bresenham stroke is not four pixels wide");
        require(f.pixel(150, 37) == 0x00FFFFFF && f.pixel(150, 42) == 0x00FFFFFF,
            "Selected stroke exceeded intended width");
        f.draw(render::LineAlgorithm::Gdi, edge);
        std::vector<std::uint32_t> reference(f.buffer.pixels(), f.buffer.pixels() + 320 * 240);
        f.draw(render::LineAlgorithm::Bresenham, edge, {}, false);
        for (std::size_t i = 0; i < reference.size(); ++i)
            require((f.buffer.pixels()[i] & 0x00FFFFFF) == (reference[i] & 0x00FFFFFF),
                "Bresenham without DIB did not fall back to GDI");
    }

    void thickDibBrushStaysWithinStorage()
    {
        DibFixture f;
        win32::drawBresenhamLine(f.buffer, {0, 0}, {0, 0}, 0x00123456, f.scratch, 4);
        for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            require(f.buffer.pixels()[y * 8 + x] == (x < 2 && y < 2 ? 0x00123456 : DibFixture::background),
                "Thick corner brush escaped clipped square");
        win32::drawBresenhamLine(f.buffer, {0, 0}, {0, 0}, 0x000078D7, f.scratch, (std::numeric_limits<int>::max)());
        require(std::all_of(f.buffer.pixels(), f.buffer.pixels() + 48,
            [](std::uint32_t color) { return color == 0x000078D7; }), "Oversized brush overflowed arithmetic");
        win32::drawBresenhamLine(f.buffer, {0, 0}, {0, 0}, 0, f.scratch, 0);
        win32::drawBresenhamLine(f.buffer, {0, 0}, {0, 0}, 0, f.scratch, -1);
        require(std::all_of(f.buffer.pixels(), f.buffer.pixels() + 48,
            [](std::uint32_t color) { return color == 0x000078D7; }), "Nonpositive stroke width changed storage");
    }

    void rasterClipBoundsTheEntireBrush()
    {
        DibFixture f;
        const RECT clip{2, 2, 6, 5};
        win32::drawBresenhamLine(f.buffer, {-1e12, 2}, {1e12, 2}, 0x00123456, f.scratch, 4, &clip);
        for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            require(f.buffer.pixels()[y * 8 + x] ==
                (x >= 2 && x < 6 && y >= 2 && y < 4 ? 0x00123456 : DibFixture::background),
                "Wide raster brush left the requested canvas rectangle");
        const std::vector<std::uint32_t> before(f.buffer.pixels(), f.buffer.pixels() + 48);
        for (const RECT empty : {RECT{2, 2, 2, 5}, RECT{7, 5, 2, 2}, RECT{-10, -10, -1, -1},
            RECT{20, 20, 30, 30}})
            win32::drawBresenhamLine(f.buffer, {0, 0}, {7, 5}, 0, f.scratch, 4, &empty);
        require(std::equal(before.begin(), before.end(), f.buffer.pixels()), "Empty clip changed the raster");

        RenderFixture scene;
        const RECT canvas{0, 40, 320, 240};
        FillRect(scene.buffer.dc(), &scene.rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        win32::drawDocument(scene.buffer.dc(), scene.document, canvas, {}, {}, scene.polygon.edges[2], {}, {},
            render::LineAlgorithm::Bresenham, &scene.buffer);
        GdiFlush();
        require(scene.pixel(150, 38) == 0x00FFFFFF && scene.pixel(150, 39) == 0x00FFFFFF &&
            scene.pixel(150, 40) == 0x000078D7 && scene.pixel(150, 41) == 0x000078D7,
            "Renderer brush overwrote reserved toolbar area");
    }

    void windowPaintUsesSelectedAlgorithmAfterResize()
    {
        Fixture f;
        Document document;
        const std::array points{Vec2{40, 40}, Vec2{220, 40}, Vec2{220, 180}, Vec2{40, 180}};
        const auto polygon = *document.findPolygon(document.addPolygon(points));
        f.state.editor = editor::EditorSession{document};
        require(f.state.editor.selectAt({130, 40}, 0) && f.state.editor.selection().edge == polygon.edges[0],
            "Paint selection fixture failed");
        SendMessageW(GetDlgItem(f.window, IDC_LINE_BRESENHAM), BM_CLICK, 0, 0);
        for (const bool resize : {false, true})
        {
            if (resize)
                require(SetWindowPos(f.window, nullptr, 0, 0, 400, 300, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE),
                    "Paint resize failed");
            RECT rect{};
            require(GetClientRect(f.window, &rect), "Paint client rectangle missing");
            InvalidateRect(f.window, nullptr, FALSE);
            SendMessageW(f.window, WM_PAINT, 0, 0);
            auto& buffer = f.state.backBuffer;
            require(buffer.pixels() && buffer.width() == rect.right && buffer.height() == rect.bottom,
                "WM_PAINT did not resize DIB");
            GdiFlush();
            const int y = rect.bottom - 60;
            require((buffer.pixels()[y * buffer.width() + 150] & 0x00FFFFFF) == 0x000078D7 &&
                (buffer.pixels()[(y - 2) * buffer.width() + 150] & 0x00FFFFFF) == 0x000078D7,
                "WM_PAINT did not use selected thick Bresenham stroke");
            require(f.state.lineAlgorithm == render::LineAlgorithm::Bresenham, "Resize reset algorithm");
        }
        SendMessageW(GetDlgItem(f.window, IDC_LINE_GDI), BM_CLICK, 0, 0);
        SendMessageW(f.window, WM_PAINT, 0, 0);
        require(f.state.lineAlgorithm == render::LineAlgorithm::Gdi, "Could not restore GDI after paint");
        requireSameCurveDocument(f.state.editor.document(), document);
        SendMessageW(f.window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(15, 15));
        require(f.state.editor.selection().edge == polygon.edges[0] && !f.state.editor.isDragging(),
            "Algorithm bar clicks affected geometry");
    }

    void timerFailureKeepsImmediateDraggingFunctional()
    {
        Fixture f;
        failTimerAllocation = true;
        f.begin();
        failTimerAllocation = false;
        require(!f.state.pointerTimerActive, "Failed timer reported as active");
        f.move({100, 50});
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {100, 50});
        f.release({120, 60});
        f.requireStopped();
        requireSameCurvePoint(curveHandlePosition(f.state.editor.document(), f.control), {120, 60});
    }
}

int main()
{
    hInst = GetModuleHandleW(nullptr);
    WNDCLASSW klass{};
    klass.hInstance = hInst;
    klass.lpfnWndProc = WndProc;
    klass.lpszClassName = L"CadPointerPreviewTests";
    if (!RegisterClassW(&klass))
        return 1;
    const std::array tests{
        cad::tests::TestCase{"radio controls switch only the view", radioControlsSwitchOnlyTheView},
        cad::tests::TestCase{"renderer dispatches segments Bezier and arcs", rendererDispatchesSegmentsBezierAndArcs},
        cad::tests::TestCase{"renderer preserves widths and falls back to GDI", rendererPreservesWidthsAndFallsBackToGdi},
        cad::tests::TestCase{"thick DIB brush stays within storage", thickDibBrushStaysWithinStorage},
        cad::tests::TestCase{"raster clip bounds the entire brush", rasterClipBoundsTheEntireBrush},
        cad::tests::TestCase{"window paint uses selected algorithm after resize", windowPaintUsesSelectedAlgorithmAfterResize},
        cad::tests::TestCase{"DIB raster clips and changes only expected pixels", dibRasterClipsAndChangesOnlyExpectedPixels},
        cad::tests::TestCase{"DIB raster uses RGB packing and preserves GDI background", dibRasterUsesRgbPackingAndPreservesGdiBackground},
        cad::tests::TestCase{"DIB raster rejects invalid lines and handles buffer lifecycle", dibRasterRejectsInvalidLinesAndHandlesBufferLifecycle},
        cad::tests::TestCase{"arc split command commits segments and selects midpoint", arcSplitCommandCommitsTwoSegmentsAndSelectsMidpoint},
        cad::tests::TestCase{"arc split command refits remaining G1 arc", arcSplitCommandRefitsRemainingG1Arc},
        cad::tests::TestCase{"rejected arc split command displays error and preserves state", rejectedArcSplitCommandShowsErrorAndKeepsState},
        cad::tests::TestCase{"conversion menu matches selected geometry", edgeConversionMenuMatchesSelectedGeometry},
        cad::tests::TestCase{"arc conversion commands round trip through WndProc", arcConversionCommandsRoundTripThroughWindowProcedure},
        cad::tests::TestCase{"rejected arc command explains continuity and preserves state", failedArcCommandShowsReasonAndPreservesDocument},
        cad::tests::TestCase{"arc continuity menu permits G0/G1 and disables C1", arcContinuityMenuDisablesC1OnlyForArcVertices},
        cad::tests::TestCase{"mouse burst uses latest position and presents", burstUsesOnlyLatestPositionAndPresents},
        cad::tests::TestCase{"release before tick uses final position", releaseBeforeTickUsesFinalPosition},
        cad::tests::TestCase{"release after preview flushes newer position", releaseAfterPreviewFlushesNewerPosition},
        cad::tests::TestCase{"cancellation clears pending work and restores snapshot", cancellationDiscardsPendingWorkAndRestoresSnapshot},
        cad::tests::TestCase{"capture loss clears pending work", captureLossDiscardsPendingWork},
        cad::tests::TestCase{"rejected frame allows recovery", rejectionRetainsPreviewAndAllowsRecovery},
        cad::tests::TestCase{"timer failure preserves immediate dragging", timerFailureKeepsImmediateDraggingFunctional}
    };
    unsigned passed = 0;
    for (const auto& test : tests)
    {
        try { test.run(); ++passed; }
        catch (const std::exception& error)
        {
            failTimerAllocation = false;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    UnregisterClassW(klass.lpszClassName, hInst);
    std::cout << passed << '/' << tests.size() << " WinAPI tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
