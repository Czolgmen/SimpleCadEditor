#include "framework.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <optional>
#include <fstream>
#include <sstream>

static std::optional<std::filesystem::path> chosenScenePath;
static bool lastSceneDialogSaving = false;
static std::optional<int> chosenStrokeWidth;
static int initialStrokeWidth = 0;

static bool failTimerAllocation = false;
static unsigned presentRequests = 0;
static unsigned messageBoxRequests = 0;
static unsigned helpShowRequests = 0;
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
static BOOL WINAPI testShowWindow(HWND window, int command)
{
    if (command == SW_SHOWNORMAL) ++helpShowRequests;
    return ::ShowWindow(window, SW_HIDE); // Test actual dialogs without visible windows.
}
static BOOL WINAPI testSetForegroundWindow(HWND)
{
    return TRUE;
}

// Exercise the actual window procedure, including its private timer ID. Inject
// timer-allocation failure and record modal messages without opening dialogs.
// Count presentation requests because
// Windows omits painting for hidden test windows; still call the real WinAPI.
#define SetTimer testSetTimer
#define UpdateWindow testUpdateWindow
#define MessageBoxW testMessageBoxW
#define ShowWindow testShowWindow
#define SetForegroundWindow testSetForegroundWindow
#define askScenePath testAskScenePath
#define askStrokeWidth testAskStrokeWidth
#include "../../Projecr1_GK.cpp"
#undef SetTimer
#undef UpdateWindow
#undef MessageBoxW
#undef ShowWindow
#undef SetForegroundWindow
#undef askScenePath
#undef askStrokeWidth

namespace cad::win32
{
    std::optional<int> testAskStrokeWidth(HINSTANCE, HWND, int current)
    {
        initialStrokeWidth = current;
        return chosenStrokeWidth;
    }

    std::optional<std::filesystem::path> testAskScenePath(HWND, bool save)
    {
        lastSceneDialogSaving = save;
        return chosenScenePath;
    }
}
#include "../CurveTestSupport.h"
#include <cad_core/model/EdgeSampling.h>
#include <cad_core/model/ContinuityJoins.h>
#include <cad_core/constraints/ContinuityConstraint.h>
#include <platform/win32/DibRenderer.h>
#include <cad_core/io/SceneIO.h>

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

    struct SceneDiskFixture
    {
        std::filesystem::path directory;
        std::filesystem::path file;
        SceneDiskFixture()
        {
            static unsigned serial = 0;
            directory = std::filesystem::temp_directory_path() /
                (L"CadSceneTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial));
            require(std::filesystem::create_directory(directory), "Could not create isolated scene test directory");
            file = directory / L"scena-żółć.cadscene";
            chosenScenePath = file;
        }
        ~SceneDiskFixture()
        {
            chosenScenePath.reset();
            std::error_code error;
            // Remove only files created in this fixture's unique directory, without recursion.
            for (const auto& entry : std::filesystem::directory_iterator(directory, error))
            {
                SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
                std::filesystem::remove(entry.path(), error);
            }
            std::filesystem::remove(directory, error);
        }
    };

    std::string savedText(const Document& document)
    {
        std::ostringstream output;
        std::string error;
        require(io::writeScene(output, document, error), error);
        return output.str();
    }

    void sceneCommandsRoundTripUnicodeFileAndResetInteraction()
    {
        Fixture f(render::LineAlgorithm::Bresenham);
        SceneDiskFixture disk;
        f.state.editor = editor::EditorSession(createDemoScene());
        const auto original = savedText(f.state.editor.document());
        const auto beforeMessages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_SAVE_AS, 0);
        require(lastSceneDialogSaving && std::filesystem::exists(disk.file), "Save As did not create Unicode path");
        auto file = win32::loadSceneFile(disk.file);
        require(file.document.has_value(), "Saved file could not be read");
        require(savedText(*file.document) == original, "Saved command lost data");
        f.state.editor = editor::EditorSession{};
        f.state.showConsistencyWarning = true;
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_LOAD, 0);
        require(!lastSceneDialogSaving && savedText(f.state.editor.document()) == original, "Load command did not restore scene");
        require(!f.state.editor.selection().vertex && !f.state.editor.selection().edge &&
            !f.state.editor.selection().polygon && !f.state.editor.selection().controlPoint &&
            !f.state.editor.pendingParallelEdge() && !f.state.showConsistencyWarning, "Loaded scene retained stale state");
        require(f.state.lineAlgorithm == render::LineAlgorithm::Bresenham, "Load changed view preference");
        f.requireStopped();
        require(messageBoxRequests == beforeMessages, "Successful file operation showed an error");
    }

    void cancelledSceneDialogsPreserveDocument()
    {
        Fixture f;
        chosenScenePath.reset();
        const auto before = savedText(f.state.editor.document());
        const auto messages = messageBoxRequests;
        for (const auto command : {IDM_SCENE_SAVE_AS, IDM_SCENE_LOAD})
        {
            SendMessageW(f.window, WM_COMMAND, command, 0);
            require(savedText(f.state.editor.document()) == before, "Cancel changed document");
        }
        require(messageBoxRequests == messages, "Cancellation displayed an error");
    }

    void failedSceneLoadPreservesDocumentAndSelection()
    {
        Fixture f;
        SceneDiskFixture disk;
        require(f.state.editor.selectAt({80,40}), "Could not select control");
        const auto before = savedText(f.state.editor.document());
        { std::ofstream output(disk.file); output << "not a CAD scene"; }
        const auto messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_LOAD, 0);
        require(messageBoxRequests == messages + 1, "Invalid load did not report error");
        require(savedText(f.state.editor.document()) == before && f.state.editor.selection().controlPoint == f.control,
            "Invalid load replaced document or selection");
        require(lastMessageBoxText.find(L"nie została zmieniona") != std::wstring::npos, "Load error did not explain preservation");
        std::filesystem::remove(disk.file);
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_LOAD, 0);
        require(messageBoxRequests == messages + 2 && savedText(f.state.editor.document()) == before,
            "Missing file was not handled");
    }

    void saveEndsGestureAndDiscardsQueuedMovement()
    {
        Fixture f;
        SceneDiskFixture disk;
        f.begin();
        f.move({100,50});
        f.tick();
        const auto accepted = savedText(f.state.editor.document());
        f.move({150,80});
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_SAVE_AS, 0);
        f.requireStopped();
        require(GetCapture() != f.window, "File dialog command retained capture");
        f.tick();
        const auto loaded = win32::loadSceneFile(disk.file);
        require(loaded.document && savedText(*loaded.document) == accepted && savedText(f.state.editor.document()) == accepted,
            "Save did not keep last accepted preview or consumed queued movement");
    }

    void failedOverwritePreservesFileAndCleansTemporary()
    {
        SceneDiskFixture disk;
        const auto document = createDemoScene();
        std::wstring error;
        require(win32::saveSceneFile(disk.file, document, error), "Could not prepare saved scene");
        const auto before = std::filesystem::file_size(disk.file);
        require(SetFileAttributesW(disk.file.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE, "Could not set read-only attribute");
        const bool saved = win32::saveSceneFile(disk.file, Document{}, error);
        SetFileAttributesW(disk.file.c_str(), FILE_ATTRIBUTE_NORMAL);
        require(!saved && !error.empty(), "Read-only destination was overwritten");
        const auto loaded = win32::loadSceneFile(disk.file);
        require(loaded.document && savedText(*loaded.document) == savedText(document), "Failed overwrite corrupted old file");
        require(std::filesystem::file_size(disk.file) == before, "Failed overwrite changed file size");
        std::size_t files = 0;
        for (const auto& entry : std::filesystem::directory_iterator(disk.directory)) { (void)entry; ++files; }
        require(files == 1, "Failed save left a temporary file");
        require(win32::saveSceneFile(disk.file, Document{}, error) && error.empty(), "Valid overwrite failed");
        require(win32::loadSceneFile(disk.file).document->getPolygons().empty(), "Valid overwrite retained old data");
    }

    void saveFailureReportsErrorWithoutChangingScene()
    {
        Fixture f;
        SceneDiskFixture disk;
        chosenScenePath = disk.directory / L"missing-directory" / L"scene.cadscene";
        const auto before = savedText(f.state.editor.document());
        const auto messages = messageBoxRequests;
        SendMessageW(f.window, WM_COMMAND, IDM_SCENE_SAVE_AS, 0);
        require(messageBoxRequests == messages + 1 && savedText(f.state.editor.document()) == before,
            "Save failure did not preserve scene and report error");
    }

    void fileMenuAndAcceleratorsAreEmbedded()
    {
        HMENU menu = LoadMenuW(hInst, MAKEINTRESOURCEW(IDC_PROJECR1GK));
        require(menu != nullptr, "Missing application menu");
        const auto saveState = GetMenuState(menu, IDM_SCENE_SAVE_AS, MF_BYCOMMAND);
        const auto loadState = GetMenuState(menu, IDM_SCENE_LOAD, MF_BYCOMMAND);
        DestroyMenu(menu);
        require(saveState != static_cast<UINT>(-1) && loadState != static_cast<UINT>(-1), "Missing file menu entries");
        HACCEL accelerators = LoadAcceleratorsW(hInst, MAKEINTRESOURCEW(IDC_PROJECR1GK));
        require(accelerators != nullptr, "Missing accelerators");
        std::vector<ACCEL> entries(CopyAcceleratorTableW(accelerators, nullptr, 0));
        CopyAcceleratorTableW(accelerators, entries.data(), static_cast<int>(entries.size()));
        bool save = false, load = false;
        for (const auto& entry : entries)
        {
            save |= entry.cmd == IDM_SCENE_SAVE_AS && entry.key == 'S' && (entry.fVirt & (FVIRTKEY|FCONTROL|FSHIFT)) == (FVIRTKEY|FCONTROL|FSHIFT);
            load |= entry.cmd == IDM_SCENE_LOAD && entry.key == 'O' && (entry.fVirt & (FVIRTKEY|FCONTROL)) == (FVIRTKEY|FCONTROL);
        }
        require(save && load, "Missing Ctrl+O or Ctrl+Shift+S");
    }

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
        const HWND wu = GetDlgItem(f.window, IDC_LINE_WU);
        require(gdi && bresenham && wu &&
            (GetWindowLongPtrW(gdi, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON &&
            (GetWindowLongPtrW(bresenham, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON &&
            (GetWindowLongPtrW(wu, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON,
            "Native algorithm radio controls missing");
        require(SendMessageW(gdi, BM_GETCHECK, 0, 0) == BST_CHECKED &&
            SendMessageW(bresenham, BM_GETCHECK, 0, 0) == BST_UNCHECKED, "Wrong default radio selection");
        const auto edge = f.state.editor.document().getPolygons().begin()->second.edges[1];
        require(f.state.editor.selectAt(edgeMiddle(f.state.editor.document(), edge), 0) &&
            f.state.editor.beginParallelSelection(), "View switch selection fixture failed");
        const Document before = f.state.editor.document();
        f.state.showConsistencyWarning = true;
        SendMessageW(f.window, WM_KILLFOCUS, reinterpret_cast<WPARAM>(bresenham), 0);
        for (const HWND control : {bresenham, wu, wu, gdi})
        {
            SendMessageW(control, BM_CLICK, 0, 0);
            const auto expected = control == wu ? render::LineAlgorithm::Wu :
                control == bresenham ? render::LineAlgorithm::Bresenham : render::LineAlgorithm::Gdi;
            require(f.state.lineAlgorithm == expected,
                "Radio did not change selected algorithm");
            for (const HWND radio : {gdi,bresenham,wu})
                require(SendMessageW(radio, BM_GETCHECK, 0, 0) == (radio == control ? BST_CHECKED : BST_UNCHECKED),
                    "Radio group lost exclusive check");
            requireSameCurveDocument(f.state.editor.document(), before);
            require(f.state.editor.selection().edge == edge && f.state.editor.pendingParallelEdge() == edge &&
                f.state.showConsistencyWarning, "View switch changed editing state");
        }
        Fixture initiallyBresenham{render::LineAlgorithm::Bresenham};
        require(SendMessageW(GetDlgItem(initiallyBresenham.window, IDC_LINE_BRESENHAM), BM_GETCHECK, 0, 0) == BST_CHECKED,
            "Window creation ignored initial renderer mode");
        Fixture initiallyWu{render::LineAlgorithm::Wu};
        require(SendMessageW(GetDlgItem(initiallyWu.window, IDC_LINE_WU), BM_GETCHECK, 0, 0) == BST_CHECKED,
            "Window creation ignored initial Wu mode");
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
        for (const auto algorithm : {render::LineAlgorithm::Gdi, render::LineAlgorithm::Bresenham, render::LineAlgorithm::Wu})
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

    void edgeWidthCommandChangesOnlySelectedEdgeAndHandlesCancel()
    {
        Fixture f;
        const auto polygon = f.state.editor.document().getPolygons().begin()->second;
        const auto edge = polygon.edges[1];
        require(f.state.editor.selectAt(edgeMiddle(f.state.editor.document(),edge),0), "Width selection failed");
        const Document before = f.state.editor.document();
        chosenStrokeWidth = 9;
        const auto messages = messageBoxRequests;
        SendMessageW(f.window,WM_COMMAND,IDM_SET_EDGE_WIDTH,0);
        require(initialStrokeWidth == 2 && f.state.editor.document().findEdge(edge)->strokeWidth == 9,
            "Width command did not use current value or update selected edge");
        for (const auto other : polygon.edges)
            require(f.state.editor.document().findEdge(other)->strokeWidth == (other == edge ? 9 : 2), "Width command changed another edge");
        requireSameCurveDocument(f.state.editor.document(),before);
        chosenStrokeWidth.reset();
        SendMessageW(f.window,WM_COMMAND,IDM_SET_EDGE_WIDTH,0);
        require(initialStrokeWidth == 9 && f.state.editor.document().findEdge(edge)->strokeWidth == 9 && messageBoxRequests == messages,
            "Cancelled width dialog changed state or reported error");
        chosenStrokeWidth = 33;
        SendMessageW(f.window,WM_COMMAND,IDM_SET_EDGE_WIDTH,0);
        require(f.state.editor.document().findEdge(edge)->strokeWidth == 9 && messageBoxRequests == messages+1,
            "Invalid width was accepted or not reported");
        chosenStrokeWidth.reset();
    }

    void widthMenuSupportsAllGeometriesAndDialogResourceExists()
    {
        require(FindResourceW(hInst,MAKEINTRESOURCEW(IDD_EDGE_WIDTH),RT_DIALOG) != nullptr, "Missing width dialog resource");
        for (int kind = 0; kind < 3; ++kind)
        {
            Document document;
            const std::array points{Vec2{0,0},Vec2{100,0},Vec2{100,100},Vec2{0,100}};
            const auto polygon = *document.findPolygon(document.addPolygon(points));
            if (kind == 1) require(document.convertEdgeToBezier(polygon.edges[0]), "Width menu Bezier failed");
            if (kind == 2) require(document.convertEdgeToArc(polygon.edges[0]), "Width menu arc failed");
            const auto* edge = document.findEdge(polygon.edges[0]);
            const auto samples = sampleEdge(edge->geometry,document.findVertex(edge->start)->position,document.findVertex(edge->end)->position);
            editor::EditorSession editor(document);
            const Vec2 click = kind == 0 ? Vec2{50,0} : samples[samples.size()/2];
            require(editor.selectAt(click,0) && editor.selection().edge == edge->id, "Width menu edge selection failed");
            HMENU menu = CreatePopupMenu();
            const bool added = appendStrokeWidthMenu(menu,editor);
            const auto state = GetMenuState(menu,IDM_SET_EDGE_WIDTH,MF_BYCOMMAND);
            DestroyMenu(menu);
            require(added && state != static_cast<UINT>(-1) && !(state & (MF_GRAYED|MF_DISABLED)), "Width disabled for a geometry kind");
        }
        HMENU menu = CreatePopupMenu();
        editor::EditorSession empty;
        const bool added = appendStrokeWidthMenu(menu,empty);
        const auto state = GetMenuState(menu,IDM_SET_EDGE_WIDTH,MF_BYCOMMAND);
        DestroyMenu(menu);
        require(added && (state & MF_GRAYED), "Width menu enabled without edge selection");
    }

    void perEdgeWidthsRenderInAllModesAndHighlightRelatively()
    {
        RenderFixture f;
        const auto edge = f.polygon.edges[2];
        require(f.document.setEdgeStrokeWidth(edge,10), "Renderer width setup failed");
        for (const auto mode : {render::LineAlgorithm::Gdi,render::LineAlgorithm::Bresenham,render::LineAlgorithm::Wu})
        {
            f.draw(mode);
            require(f.pixel(150,36) == 0 && f.pixel(150,40) == 0 && f.pixel(150,32) == 0x00FFFFFF,
                "Configured width not used in a renderer");
            require(f.pixel(60,110) == 0 && f.pixel(64,110) == 0x00FFFFFF,
                "Per-edge width changed a neighbouring edge");
            f.draw(mode,edge);
            require(f.pixel(150,35) == 0x000078D7 && f.pixel(150,40) == 0x000078D7,
                "Highlight did not add two pixels to the configured width");
            require(f.document.findEdge(edge)->strokeWidth == 10, "Rendering modified persistent width");
        }
    }

    void wuBlendsWithExistingDibColor()
    {
        DibFixture f;
        win32::WuScratch scratch;
        std::fill_n(f.buffer.pixels(),48,0x00204060);
        const std::array points{Vec2{0,2.25},Vec2{7,2.25}};
        win32::drawWuPolyline(f.buffer,points,0x00A0C0E0,scratch);
        require(f.buffer.pixels()[2*8+3] == 0x0080A0C0 && f.buffer.pixels()[3*8+3] == 0x00406080,
            "Wu did not blend RGB against actual background with .75/.25 coverage");
        require(f.buffer.pixels()[3] == 0x00204060, "Wu changed pixels outside footprint");
        require(GetPixel(f.buffer.dc(),3,2) == RGB(128,160,192), "Wu swapped DIB color channels");
        GdiFlush();
    }

    void wuPolylineJoinsMatchUnsplitHorizontal()
    {
        DibFixture f;
        win32::WuScratch scratch;
        const std::array straight{Vec2{0,2.25},Vec2{7,2.25}};
        win32::drawWuPolyline(f.buffer,straight,0,scratch,2);
        const std::vector<std::uint32_t> reference(f.buffer.pixels(),f.buffer.pixels()+48);
        std::fill_n(f.buffer.pixels(),48,DibFixture::background);
        std::vector<Vec2> sampled;
        for (int i = 0; i <= 56; ++i) sampled.push_back({i*0.125,2.25});
        sampled.insert(sampled.begin()+20,sampled[20]);
        win32::drawWuPolyline(f.buffer,sampled,0,scratch,2);
        require(std::equal(reference.begin(),reference.end(),f.buffer.pixels()),
            "Polyline seams changed brightness or duplicate vertices made dots");
    }

    void wuThickStrokesBlendAndStayInsideClip()
    {
        DibFixture f;
        win32::WuScratch scratch;
        const RECT clip{2,1,6,5};
        const std::array points{Vec2{-1e12,2.25},Vec2{1e12,2.25}};
        win32::drawWuPolyline(f.buffer,points,0x000078D7,scratch,4,&clip);
        for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            if (x < 2 || x >= 6 || y < 1 || y >= 5)
                require(f.buffer.pixels()[y*8+x] == DibFixture::background, "Wu stroke escaped clip rectangle");
        require(f.buffer.pixels()[2*8+3] == 0x000078D7, "Thick Wu stroke has no opaque selected core");
        const auto border = f.buffer.pixels()[4*8+3];
        require(border != DibFixture::background && border != 0x000078D7, "Thick Wu boundary was not antialiased");
    }

    void wuRejectsBadInputAndHandlesBufferResize()
    {
        DibFixture f;
        win32::WuScratch scratch;
        const std::array points{Vec2{0,1},Vec2{7,4}};
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const std::array bad{Vec2{0,1},Vec2{3,2},Vec2{nan,4}};
        const std::array invisible{Vec2{-100,-100},Vec2{-50,-50}};
        for (const int width : {0,-1,(std::numeric_limits<int>::max)()})
            win32::drawWuPolyline(f.buffer,points,0,scratch,width);
        win32::drawWuPolyline(f.buffer,bad,0,scratch);
        win32::drawWuPolyline(f.buffer,invisible,0,scratch);
        const RECT empty{3,3,3,5};
        win32::drawWuPolyline(f.buffer,points,0,scratch,4,&empty);
        require(std::all_of(f.buffer.pixels(),f.buffer.pixels()+48,
            [](std::uint32_t p) { return p == DibFixture::background; }), "Rejected Wu line changed pixels");
        win32::drawWuPolyline(f.buffer,points,0,scratch);
        f.buffer.reset();
        win32::drawWuPolyline(f.buffer,points,0,scratch);
        const HDC reference = GetDC(nullptr);
        const bool resized = f.buffer.resize(reference,1,1);
        ReleaseDC(nullptr,reference);
        require(resized, "Could not create single-pixel Wu buffer");
        f.buffer.pixels()[0] = DibFixture::background;
        const std::array dot{Vec2{0,0},Vec2{0,0}};
        win32::drawWuPolyline(f.buffer,dot,0x00123456,scratch);
        require(f.buffer.pixels()[0] == 0x00123456, "Zero-length Wu dot or scratch resize failed");
    }

    void wuRendererPreservesSelectionAndFallsBackToGdi()
    {
        RenderFixture f;
        const auto edge = f.polygon.edges[2];
        f.draw(render::LineAlgorithm::Wu);
        require(f.pixel(150,40) == 0 && f.pixel(150,39) != 0 && f.pixel(150,39) != 0x00FFFFFF &&
            f.pixel(150,38) == 0x00FFFFFF, "Normal Wu width has no AA boundary");
        f.draw(render::LineAlgorithm::Wu,edge);
        require(f.pixel(150,40) == 0x000078D7 && f.pixel(150,39) == 0x000078D7 &&
            f.pixel(150,38) != 0x000078D7 && f.pixel(150,38) != 0x00FFFFFF, "Selected Wu width/color wrong");
        f.draw(render::LineAlgorithm::Wu,edge,edge);
        require(f.pixel(150,40) == 0x00DC7800, "Wu lost pending parallel color");
        f.draw(render::LineAlgorithm::Gdi,edge);
        const std::vector<std::uint32_t> reference(f.buffer.pixels(),f.buffer.pixels()+320*240);
        f.draw(render::LineAlgorithm::Wu,edge,{},false);
        require(std::equal(reference.begin(),reference.end(),f.buffer.pixels()), "Wu without DIB did not fall back to GDI");
    }

    void wuWindowPaintKeepsModeAndAaAfterResize()
    {
        Fixture f;
        Document document;
        const std::array points{Vec2{40,40},Vec2{220,40},Vec2{220,180},Vec2{40,180}};
        const auto polygon = *document.findPolygon(document.addPolygon(points));
        f.state.editor = editor::EditorSession(document);
        require(f.state.editor.selectAt({130,40},0) && f.state.editor.selection().edge == polygon.edges[0], "Wu paint selection failed");
        SendMessageW(GetDlgItem(f.window,IDC_LINE_WU),BM_CLICK,0,0);
        for (const bool resize : {false,true})
        {
            if (resize) require(SetWindowPos(f.window,nullptr,0,0,410,310,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE), "Wu resize failed");
            SendMessageW(f.window,WM_PAINT,0,0);
            GdiFlush();
            auto& buffer = f.state.backBuffer;
            const int row = buffer.height()-60;
            const auto core = buffer.pixels()[row*buffer.width()+150] & 0x00FFFFFF;
            const auto fringe = buffer.pixels()[(row-2)*buffer.width()+150] & 0x00FFFFFF;
            require(core == 0x000078D7 && fringe != core && fringe != 0x00FFFFFF, "WM_PAINT missed Wu coverage");
            require(f.state.lineAlgorithm == render::LineAlgorithm::Wu, "Resize reset Wu mode");
        }
        requireSameCurveDocument(f.state.editor.document(),document);
    }

    void wuToolbarClipBoundsAntialiasedFringe()
    {
        RenderFixture f;
        const RECT canvas{0,40,320,240};
        FillRect(f.buffer.dc(),&f.rect,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        win32::drawDocument(f.buffer.dc(),f.document,canvas,{},{},f.polygon.edges[2],{},{},render::LineAlgorithm::Wu,&f.buffer);
        GdiFlush();
        require(f.pixel(150,38) == 0x00FFFFFF && f.pixel(150,39) == 0x00FFFFFF && f.pixel(150,40) == 0x000078D7,
            "Wu AA fringe overwrote toolbar or clipped away visible stroke");
    }

    void wuWideCircleHasUniformThicknessAcrossOctants()
    {
        RenderFixture f;
        win32::WuScratch scratch;
        std::vector<Vec2> circle;
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i <= 256; ++i)
        {
            const double angle = 2*pi*i/256;
            circle.push_back({160+70*std::cos(angle),120+70*std::sin(angle)});
        }
        FillRect(f.buffer.dc(),&f.rect,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        GdiFlush();
        win32::drawWuPolyline(f.buffer,circle,0,scratch,16);
        for (int y = 40; y <= 200; ++y)
        for (int x = 80; x <= 240; ++x)
        {
            const double distance = std::abs(std::hypot(x-160.0,y-120.0)-70.0);
            const double alpha = std::clamp(8.5-distance,0.0,1.0);
            const int reference = static_cast<int>(std::lround(255*(1-alpha)));
            const int actual = static_cast<int>(f.pixel(x,y)&255u);
            require(std::abs(actual-reference) <= 3, "Wide circular stroke changes thickness or has octant seams");
        }
    }

    void wuScratchDoesNotRepaintPreviousEdges()
    {
        DibFixture f;
        win32::WuScratch scratch;
        const std::array first{Vec2{0,1.25},Vec2{7,1.25}}, second{Vec2{0,4.25},Vec2{7,4.25}};
        win32::drawWuPolyline(f.buffer,first,0x000078D7,scratch);
        const std::vector<std::uint32_t> before(f.buffer.pixels(),f.buffer.pixels()+48);
        win32::drawWuPolyline(f.buffer,second,0x00DC7800,scratch);
        for (int i = 0; i < 3*8; ++i) require(f.buffer.pixels()[i] == before[i], "Reused Wu mask repainted previous edge");
        require(f.buffer.pixels()[4*8+3] != DibFixture::background && f.buffer.pixels()[4*8+3] != 0x00DC7800,
            "New Wu stroke lost its independent coverage");
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

    std::wstring dialogText(HWND dialog)
    {
        const HWND edit = GetDlgItem(dialog, IDC_HELP_TEXT);
        const int size = GetWindowTextLengthW(edit);
        std::wstring text(static_cast<std::size_t>(size) + 1, L'\0');
        const int written = GetWindowTextW(edit, text.data(), size + 1);
        text.resize(static_cast<std::size_t>(written));
        return text;
    }

    void documentationButtonAndMenuOpenTheSameWindow()
    {
        Fixture f;
        const HWND button = GetDlgItem(f.window, IDC_DOCUMENTATION);
        require(button && (GetWindowLongPtrW(button, GWL_STYLE) & BS_TYPEMASK) == BS_PUSHBUTTON,
            "Documentation toolbar button missing");
        require(f.state.editor.selectAt({80, 40}, 0), "Documentation selection fixture failed");
        const Document before = f.state.editor.document();
        f.state.lineAlgorithm = render::LineAlgorithm::Bresenham;
        SendMessageW(button, BM_CLICK, 0, 0);
        const HWND documentation = f.state.documentationWindow;
        require(documentation && IsWindow(documentation) && !f.state.helpWindow,
            "Documentation button opened the wrong window");
        const auto text = dialogText(documentation);
        require(text.find(L"Bresenham") != std::wstring::npos && text.find(L"Cohena") != std::wstring::npos,
            "Documentation did not initially show drawing algorithms");
        SendDlgItemMessageW(documentation, IDC_HELP_TOPIC, CB_SETCURSEL, 7, 0);
        SendMessageW(documentation, WM_COMMAND, MAKEWPARAM(IDC_HELP_TOPIC, CBN_SELCHANGE), 0);
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_DOCUMENTATION, 0), 0);
        require(f.state.documentationWindow == documentation &&
            SendDlgItemMessageW(documentation, IDC_HELP_TOPIC, CB_GETCURSEL, 0, 0) == 7,
            "Documentation menu duplicated window or reset chapter");
        requireSameCurveDocument(f.state.editor.document(), before);
        require(f.state.editor.selection().controlPoint == f.control &&
            f.state.lineAlgorithm == render::LineAlgorithm::Bresenham, "Documentation changed editor state");
        const HMENU menu = LoadMenuW(hInst, MAKEINTRESOURCEW(IDC_PROJECR1GK));
        wchar_t label[128]{};
        require(menu != nullptr, "Documentation menu resource missing");
        const int length = GetMenuStringW(menu, IDM_DOCUMENTATION, label, static_cast<int>(std::size(label)), MF_BYCOMMAND);
        DestroyMenu(menu);
        require(length > 0 && std::wstring(label).find(L"Dokumentacja") != std::wstring::npos,
            "Documentation menu item missing");
    }

    void embeddedDocumentationHasFormattedAlgorithmsModelAndSolver()
    {
        Fixture f;
        const HWND dialog = win32::createDocumentationDialog(hInst, f.window, f.state.documentationWindow);
        require(dialog && !IsWindowVisible(dialog), "Embedded documentation could not be created hidden");
        const LRESULT count = SendDlgItemMessageW(dialog, IDC_HELP_TOPIC, CB_GETCOUNT, 0, 0);
        require(count == 10, "Manual chapters were lost or table of contents became a duplicate chapter");
        for (LRESULT chapter = 0; chapter < count; ++chapter)
        {
            SendDlgItemMessageW(dialog, IDC_HELP_TOPIC, CB_SETCURSEL, static_cast<WPARAM>(chapter), 0);
            SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_HELP_TOPIC, CBN_SELCHANGE), 0);
            const auto text = dialogText(dialog);
            require(text.size() > 100 && text.find(L"```") == std::wstring::npos &&
                text.find(L"**") == std::wstring::npos && text.find(L"](../") == std::wstring::npos,
                "Embedded chapter is empty or still contains Markdown display markup");
            if (chapter == 5)
                require(text.find(L"VertexId") != std::wstring::npos && text.find(L"Document") != std::wstring::npos,
                    "Model chapter did not load");
            if (chapter == 6)
                require(text.find(L"Δ1") != std::wstring::npos && text.find(L"Bresenham") != std::wstring::npos &&
                    text.find(L"Cohena") != std::wstring::npos && text.find(L"point += Δ1") != std::wstring::npos,
                    "Algorithm chapter lost Unicode equations or code blocks");
            if (chapter == 7)
                require(text.find(L"J*Jᵀ") != std::wstring::npos && text.find(L"nextafter") != std::wstring::npos &&
                    text.find(L"maxIterations") != std::wstring::npos, "Solver chapter lost technical content");
        }
    }

    void documentationAndHelpHaveIndependentLifetimes()
    {
        Fixture f;
        SendMessageW(f.window, WM_KEYDOWN, VK_F1, 0);
        SendMessageW(GetDlgItem(f.window, IDC_DOCUMENTATION), BM_CLICK, 0, 0);
        const HWND help = f.state.helpWindow, documentation = f.state.documentationWindow;
        require(help && documentation && help != documentation, "Help and documentation must be separate windows");
        MSG escape{};
        escape.hwnd = GetDlgItem(documentation, IDC_HELP_TEXT);
        escape.message = WM_KEYDOWN;
        escape.wParam = VK_ESCAPE;
        require(!win32::handleHelpDialogMessage(help, escape) &&
            win32::handleHelpDialogMessage(documentation, escape), "Escape was routed to the wrong reference window");
        require(!f.state.documentationWindow && f.state.helpWindow == help && IsWindow(help),
            "Closing documentation also closed help or retained stale handle");
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_DOCUMENTATION, 0), 0);
        const HWND reopened = f.state.documentationWindow;
        require(reopened && IsWindow(reopened), "Documentation did not reopen");
        DestroyWindow(f.window);
        f.window = nullptr;
        require(!f.state.helpWindow && !f.state.documentationWindow && !IsWindow(help) && !IsWindow(reopened),
            "Owner destruction did not close both reference windows");
    }

    void helpTopicsAreReadableAndResizeWithTheDialog()
    {
        Fixture f;
        const HWND help = win32::createHelpDialog(hInst, f.window, f.state.helpWindow);
        require(help && f.state.helpWindow == help && !IsWindowVisible(help), "Help creation did not return hidden owned dialog");
        require(GetWindow(help, GW_OWNER) == f.window, "Help dialog lost its owner");
        const HWND edit = GetDlgItem(help, IDC_HELP_TEXT);
        require(edit && (GetWindowLongPtrW(edit, GWL_STYLE) & ES_READONLY) &&
            (GetWindowLongPtrW(edit, GWL_STYLE) & WS_VSCROLL), "Help text must be read-only and scrollable");
        const std::array markers{L"LPM", L"RÓWNOLEGŁOŚĆ", L"C1", L"G0", L"SCENA"};
        require(SendDlgItemMessageW(help, IDC_HELP_TOPIC, CB_GETCOUNT, 0, 0) == static_cast<LRESULT>(markers.size()), "Help topics missing");
        for (std::size_t i = 0; i < markers.size(); ++i)
        {
            SendDlgItemMessageW(help, IDC_HELP_TOPIC, CB_SETCURSEL, i, 0);
            SendMessageW(help, WM_COMMAND, MAKEWPARAM(IDC_HELP_TOPIC, CBN_SELCHANGE),
                reinterpret_cast<LPARAM>(GetDlgItem(help, IDC_HELP_TOPIC)));
            wchar_t text[4096]{};
            require(GetDlgItemTextW(help, IDC_HELP_TEXT, text, static_cast<int>(std::size(text))) > 100 &&
                std::wstring(text).find(markers[i]) != std::wstring::npos, "Help topic text missing or garbled");
        }
        for (const POINT size : {POINT{420, 340}, POINT{800, 600}})
        {
            require(SetWindowPos(help, nullptr, 0, 0, size.x, size.y, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE),
                "Help resize failed");
            RECT text{}, close{}, client{};
            require(GetWindowRect(edit, &text) && GetWindowRect(GetDlgItem(help, IDCANCEL), &close) &&
                GetClientRect(help, &client), "Help layout unavailable");
            MapWindowPoints(nullptr, help, reinterpret_cast<POINT*>(&text), 2);
            MapWindowPoints(nullptr, help, reinterpret_cast<POINT*>(&close), 2);
            require(text.left >= 0 && text.right <= client.right && text.bottom > text.top &&
                text.bottom < close.top && close.bottom <= client.bottom, "Help controls overlap after resize");
        }
        MSG escape{};
        escape.hwnd = edit;
        escape.message = WM_KEYDOWN;
        escape.wParam = VK_ESCAPE;
        MSG canvasEscape = escape;
        canvasEscape.hwnd = f.window;
        require(!win32::handleHelpDialogMessage(help, canvasEscape) && IsWindow(help),
            "Help consumed Escape belonging to the canvas");
        const bool handled = win32::handleHelpDialogMessage(help, escape);
        require(handled && !f.state.helpWindow && !IsWindow(help),
            "Escape did not close modeless help: handled=" + std::to_string(handled) +
            ", alive=" + std::to_string(IsWindow(help)) + ", slot=" + std::to_string(f.state.helpWindow != nullptr));
    }

    void helpEntryPointsReuseWindowAndPreserveDocument()
    {
        Fixture f;
        require(f.state.editor.selectAt({80, 40}, 0), "Help selection fixture failed");
        const Document before = f.state.editor.document();
        const auto selection = f.state.editor.selection().controlPoint;
        f.state.lineAlgorithm = render::LineAlgorithm::Bresenham;
        const unsigned shown = helpShowRequests;
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_HELP, 0), 0);
        const HWND help = f.state.helpWindow;
        require(help && IsWindow(help), "Help menu command did not open dialog");
        SendDlgItemMessageW(help, IDC_HELP_TOPIC, CB_SETCURSEL, 2, 0);
        SendMessageW(help, WM_COMMAND, MAKEWPARAM(IDC_HELP_TOPIC, CBN_SELCHANGE), 0);
        SendMessageW(f.window, WM_KEYDOWN, VK_F1, 0);
        SendMessageW(f.window, WM_HELP, 0, 0);
        require(f.state.helpWindow == help && helpShowRequests == shown + 3 &&
            SendDlgItemMessageW(help, IDC_HELP_TOPIC, CB_GETCURSEL, 0, 0) == 2,
            "F1/help request duplicated window or reset selected topic");
        requireSameCurveDocument(f.state.editor.document(), before);
        require(f.state.editor.selection().controlPoint == selection &&
            f.state.lineAlgorithm == render::LineAlgorithm::Bresenham, "Help changed document view or selection");
        SendMessageW(help, WM_CLOSE, 0, 0);
        require(!f.state.helpWindow && !IsWindow(help), "Help close retained stale handle");
        SendMessageW(f.window, WM_KEYDOWN, VK_F1, 0);
        require(f.state.helpWindow && IsWindow(f.state.helpWindow), "Help could not reopen");
        const HWND reopened = f.state.helpWindow;
        DestroyWindow(f.window);
        f.window = nullptr;
        require(!IsWindow(reopened) && !f.state.helpWindow, "Owner destruction left help window or stale slot");
    }

    void helpCancelsPendingGestureAndParallelSelection()
    {
        Fixture f;
        const Document before = f.state.editor.document();
        f.begin();
        f.move({100, 50});
        f.tick();
        f.move({120, 60});
        SendMessageW(f.window, WM_KEYDOWN, VK_F1, 0);
        f.requireStopped();
        requireSameCurveDocument(f.state.editor.document(), before);
        const auto edge = f.state.editor.document().getPolygons().begin()->second.edges[1];
        require(f.state.editor.selectAt(edgeMiddle(before, edge), 0) && f.state.editor.beginParallelSelection(),
            "Help parallel cancellation fixture failed");
        SendMessageW(f.window, WM_COMMAND, MAKEWPARAM(IDM_HELP, 0), 0);
        require(!f.state.editor.pendingParallelEdge(), "Help retained half-completed parallel selection");
        requireSameCurveDocument(f.state.editor.document(), before);
    }

    void helpMenuAndF1AcceleratorAreEmbeddedResources()
    {
        const HMENU menu = LoadMenuW(hInst, MAKEINTRESOURCEW(IDC_PROJECR1GK));
        require(menu != nullptr, "Application menu resource unavailable");
        wchar_t label[128]{};
        const int length = GetMenuStringW(menu, IDM_HELP, label, static_cast<int>(std::size(label)), MF_BYCOMMAND);
        DestroyMenu(menu);
        require(length > 0 && std::wstring(label).find(L"F1") != std::wstring::npos, "Help menu entry missing");
        const HACCEL table = LoadAcceleratorsW(hInst, MAKEINTRESOURCEW(IDC_PROJECR1GK));
        require(table != nullptr, "Accelerator resource unavailable");
        const int count = CopyAcceleratorTableW(table, nullptr, 0);
        require(count > 0, "Empty accelerator table");
        std::vector<ACCEL> accelerators(static_cast<std::size_t>(count));
        require(CopyAcceleratorTableW(table, accelerators.data(), count) == count, "Could not inspect accelerators");
        require(std::any_of(accelerators.begin(), accelerators.end(), [](const ACCEL& item)
        {
            return item.cmd == IDM_HELP && item.key == VK_F1 && (item.fVirt & FVIRTKEY) != 0;
        }), "F1 accelerator is not bound to Help");
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
        cad::tests::TestCase{"edge width command selection cancellation and errors",edgeWidthCommandChangesOnlySelectedEdgeAndHandlesCancel},
        cad::tests::TestCase{"width menu supports curves and has dialog resource",widthMenuSupportsAllGeometriesAndDialogResourceExists},
        cad::tests::TestCase{"individual widths render in all modes",perEdgeWidthsRenderInAllModesAndHighlightRelatively},
        cad::tests::TestCase{"Wu blends with existing DIB RGB",wuBlendsWithExistingDibColor},
        cad::tests::TestCase{"Wu polyline joins match an unsplit line",wuPolylineJoinsMatchUnsplitHorizontal},
        cad::tests::TestCase{"Wu thick strokes blend and stay clipped",wuThickStrokesBlendAndStayInsideClip},
        cad::tests::TestCase{"Wu rejects bad input and handles resize",wuRejectsBadInputAndHandlesBufferResize},
        cad::tests::TestCase{"Wu selection color widths and GDI fallback",wuRendererPreservesSelectionAndFallsBackToGdi},
        cad::tests::TestCase{"Wu paint retains AA after resize",wuWindowPaintKeepsModeAndAaAfterResize},
        cad::tests::TestCase{"Wu AA fringe respects toolbar clip",wuToolbarClipBoundsAntialiasedFringe},
        cad::tests::TestCase{"Wu scratch does not repaint previous edges",wuScratchDoesNotRepaintPreviousEdges},
        cad::tests::TestCase{"wide Wu circle stays uniform across octants",wuWideCircleHasUniformThicknessAcrossOctants},
        cad::tests::TestCase{"scene commands round trip Unicode file and reset state", sceneCommandsRoundTripUnicodeFileAndResetInteraction},
        cad::tests::TestCase{"cancelled scene dialogs preserve document", cancelledSceneDialogsPreserveDocument},
        cad::tests::TestCase{"failed scene load preserves document and selection", failedSceneLoadPreservesDocumentAndSelection},
        cad::tests::TestCase{"save ends gesture and discards queued movement", saveEndsGestureAndDiscardsQueuedMovement},
        cad::tests::TestCase{"failed overwrite preserves file and cleans temporary", failedOverwritePreservesFileAndCleansTemporary},
        cad::tests::TestCase{"save failure reports error and preserves scene", saveFailureReportsErrorWithoutChangingScene},
        cad::tests::TestCase{"file menu and accelerators are embedded", fileMenuAndAcceleratorsAreEmbedded},
        cad::tests::TestCase{"documentation button and menu open the same window", documentationButtonAndMenuOpenTheSameWindow},
        cad::tests::TestCase{"embedded documentation has formatted algorithms model and solver", embeddedDocumentationHasFormattedAlgorithmsModelAndSolver},
        cad::tests::TestCase{"documentation and help have independent lifetimes", documentationAndHelpHaveIndependentLifetimes},
        cad::tests::TestCase{"help topics are readable and resize with dialog", helpTopicsAreReadableAndResizeWithTheDialog},
        cad::tests::TestCase{"help entry points reuse window and preserve document", helpEntryPointsReuseWindowAndPreserveDocument},
        cad::tests::TestCase{"help cancels pending gesture and parallel selection", helpCancelsPendingGestureAndParallelSelection},
        cad::tests::TestCase{"help menu and F1 accelerator are embedded resources", helpMenuAndF1AcceleratorAreEmbeddedResources},
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
