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

namespace
{
    using namespace cad;
    using namespace cad::tests;
    struct Fixture
    {
        win32::AppState state;
        ControlPointRef control;
        HWND window = nullptr;

        Fixture()
        {
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
