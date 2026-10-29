// Projecr1_GK.cpp : Definiuje punkt wejścia dla aplikacji.
//

#include "framework.h"
#include "Projecr1_GK.h"

#include <platform/win32/AppState.h>
#include <platform/win32/GdiRenderer.h>
#include <platform/win32/LengthDialog.h>
#include <platform/win32/HelpDialog.h>
#include <platform/win32/SceneFiles.h>
#include <cad_core/demo/DemoScene.hpp>
#include <editor/HitTester.h>

#include <windowsx.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <variant>


#define MAX_LOADSTRING 100

// Zmienne globalne:
HINSTANCE hInst;                                // bieżące wystąpienie
WCHAR szTitle[MAX_LOADSTRING];                  // Tekst paska tytułu
WCHAR szWindowClass[MAX_LOADSTRING];            // nazwa klasy okna głównego

// Przekaż dalej deklaracje funkcji dołączone w tym module kodu:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, cad::win32::AppState&);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

namespace
{
    // These IDs are handled directly after TrackPopupMenu, before WM_COMMAND's 16-bit packing.
    constexpr UINT parallelRemovalCommandBase = 0x10000;
    constexpr UINT_PTR pointerFrameTimer = 1;
    constexpr UINT pointerFrameIntervalMs = 1;
    constexpr int algorithmBarHeight = 40;

    void checkAlgorithmControls(HWND window, cad::render::LineAlgorithm algorithm)
    {
        const int chosen = algorithm == cad::render::LineAlgorithm::Wu ? IDC_LINE_WU :
            algorithm == cad::render::LineAlgorithm::Bresenham ? IDC_LINE_BRESENHAM : IDC_LINE_GDI;
        for (const int id : {IDC_LINE_GDI, IDC_LINE_BRESENHAM, IDC_LINE_WU})
            CheckDlgButton(window, id, id == chosen ? BST_CHECKED : BST_UNCHECKED);
    }

    bool createAlgorithmControls(HWND window, cad::render::LineAlgorithm algorithm)
    {
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
        HWND gdi = CreateWindowExW(0, L"BUTTON", L"GDI", style | WS_GROUP,
            65, 8, 70, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LINE_GDI)), hInst, nullptr);
        HWND bresenham = CreateWindowExW(0, L"BUTTON", L"Bresenham", style,
            145, 8, 125, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LINE_BRESENHAM)), hInst, nullptr);
        HWND wu = CreateWindowExW(0, L"BUTTON", L"Wu (AA)", style,
            275, 8, 95, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LINE_WU)), hInst, nullptr);
        HWND documentation = CreateWindowExW(0, L"BUTTON", L"Dokumentacja", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_PUSHBUTTON,
            390, 8, 130, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DOCUMENTATION)), hInst, nullptr);
        if (!gdi || !bresenham || !wu || !documentation)
            return false;
        for (const HWND control : {gdi, bresenham, wu, documentation})
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        checkAlgorithmControls(window, algorithm);
        return true;
    }

    void drawAlgorithmBar(HDC dc, const RECT& clientRect)
    {
        const int saved = SaveDC(dc);
        if (!saved) return;
        RECT bar = clientRect;
        bar.bottom = (std::min)(bar.bottom, bar.top + algorithmBarHeight);
        FillRect(dc, &bar, GetSysColorBrush(COLOR_BTNFACE));
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        RECT label{10, 8, 60, 32};
        DrawTextW(dc, L"Linie:", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        RestoreDC(dc, saved);
    }

    void drawConsistencyWarning(HDC dc, const RECT& clientRect)
    {
        const int saved = SaveDC(dc);
        if (!saved)
            return;
        RECT banner{clientRect.left + 8, (std::max)(clientRect.top + 8, clientRect.bottom - 72),
            clientRect.right - 8, clientRect.bottom - 8};
        SetDCBrushColor(dc, RGB(255, 240, 205));
        FillRect(dc, &banner, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        InflateRect(&banner, -10, -8);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(130, 55, 0));
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc,
            L"Nie udało się uzyskać spójnego stanu. Zachowano ostatni poprawny stan. "
            L"Spróbuj innego położenia.",
            -1, &banner, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        RestoreDC(dc, saved);
    }

    bool appendParallelRemovalMenu(HMENU menu, const cad::editor::EditorSession& editor,
        std::vector<cad::EdgeId>& targets)
    {
        const auto selected = editor.selection().edge;
        if (!selected)
            return AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Usuń równoległość") != FALSE;

        HMENU submenu = CreatePopupMenu();
        if (!submenu)
            return false;
        std::size_t number = 0;
        for (const auto& pair : editor.document().getParallelConstraints())
        {
            ++number;
            if (pair.first != *selected && pair.second != *selected)
                continue;
            const UINT command = parallelRemovalCommandBase + static_cast<UINT>(targets.size());
            targets.push_back(pair.first == *selected ? pair.second : pair.first);
            const auto label = L"P" + std::to_wstring(number);
            if (!AppendMenuW(submenu, MF_STRING, command, label.c_str()))
            {
                DestroyMenu(submenu);
                return false;
            }
        }
        if (targets.empty())
        {
            DestroyMenu(submenu);
            return AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Usuń równoległość") != FALSE;
        }
        if (!AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), L"Usuń równoległość"))
        {
            DestroyMenu(submenu);
            return false;
        }
        return true; // The parent menu now owns the submenu.
    }

    bool appendStrokeWidthMenu(HMENU menu, const cad::editor::EditorSession& editor)
    {
        return AppendMenuW(menu, MF_STRING | (editor.selectedEdgeStrokeWidth() ? MF_ENABLED : MF_GRAYED),
            IDM_SET_EDGE_WIDTH, L"Grubość krawędzi...") != FALSE;
    }

    bool appendContinuityMenu(HMENU menu, const cad::editor::EditorSession& editor)
    {
        const auto continuity = editor.selectedVertexContinuity();
        if (!continuity)
            return AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Ciągłość wierzchołka") != FALSE;

        bool hasArc = false;
        const auto selectedVertex = *editor.selection().vertex;
        for (const auto& [polygonId, polygon] : editor.document().getPolygons())
            for (const auto id : polygon.edges)
                if (const auto* edge = editor.document().findEdge(id);
                    edge && (edge->start == selectedVertex || edge->end == selectedVertex) &&
                    std::holds_alternative<cad::CircularArc>(edge->geometry))
                    hasArc = true;

        UINT checkedCommand;
        switch (*continuity)
        {
        case cad::Continuity::G0: checkedCommand = IDM_CONTINUITY_G0; break;
        case cad::Continuity::G1: checkedCommand = IDM_CONTINUITY_G1; break;
        case cad::Continuity::C1: checkedCommand = IDM_CONTINUITY_C1; break;
        default: return false;
        }

        HMENU submenu = CreatePopupMenu();
        if (!submenu)
            return false;
        if (!AppendMenuW(submenu, MF_STRING, IDM_CONTINUITY_G0, L"G0 - wspólny punkt") ||
            !AppendMenuW(submenu, MF_STRING, IDM_CONTINUITY_G1, L"G1 - zgodny kierunek stycznych") ||
            !AppendMenuW(submenu, MF_STRING | (hasArc ? MF_GRAYED : 0), IDM_CONTINUITY_C1, L"C1 - zgodne pochodne") ||
            !CheckMenuRadioItem(submenu, IDM_CONTINUITY_G0, IDM_CONTINUITY_C1, checkedCommand, MF_BYCOMMAND) ||
            !AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), L"Ciągłość wierzchołka"))
        {
            DestroyMenu(submenu);
            return false;
        }
        return true; // The parent menu owns the submenu after a successful append.
    }

    bool appendEdgeConversionMenu(HMENU menu, const cad::editor::EditorSession& editor)
    {
        const auto selected = editor.selection().edge;
        const auto* edge = selected ? editor.document().findEdge(*selected) : nullptr;
        const bool segment = edge && std::holds_alternative<cad::LineSegment>(edge->geometry);
        const bool bezier = edge && std::holds_alternative<cad::CubicBezier>(edge->geometry);
        const bool arc = edge && std::holds_alternative<cad::CircularArc>(edge->geometry);
        return AppendMenuW(menu, MF_STRING | (segment || arc ? MF_ENABLED : MF_GRAYED),
                   IDM_CONVERT_TO_BEZIER, L"Zamień na Béziera") &&
            AppendMenuW(menu, MF_STRING | (segment || bezier ? MF_ENABLED : MF_GRAYED),
                   IDM_CONVERT_TO_ARC, L"Zamień na łuk okręgu") &&
            AppendMenuW(menu, MF_STRING | (bezier || arc ? MF_ENABLED : MF_GRAYED),
                   IDM_CONVERT_TO_SEGMENT, L"Zamień na odcinek");
    }

    cad::win32::AppState* getAppState(HWND window)
    {
        return reinterpret_cast<cad::win32::AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    cad::Vec2 clientToWorld(const POINT& point, const RECT& clientRect)
    {
        return {static_cast<double>(point.x - clientRect.left - 20),
                static_cast<double>(clientRect.bottom - 20 - point.y)};
    }

    std::optional<cad::Vec2> mouseWorldPosition(HWND window, LPARAM position)
    {
        RECT clientRect{};
        if (!GetClientRect(window, &clientRect))
            return std::nullopt;
        return clientToWorld(POINT{GET_X_LPARAM(position), GET_Y_LPARAM(position)}, clientRect);
    }

    void stopPointerUpdates(HWND window)
    {
        KillTimer(window, pointerFrameTimer);
        if (auto* state = getAppState(window))
        {
            state->pendingPointerPosition.reset();
            state->pointerTimerActive = false;
        }
    }

    void updatePointerPreview(HWND window, cad::win32::AppState& state, const cad::Vec2& position)
    {
        const bool accepted = state.editor.pointerMove(position);
        const bool warningChanged = state.showConsistencyWarning != !accepted;
        state.showConsistencyWarning = !accepted;
        if (accepted || warningChanged)
            InvalidateRect(window, nullptr, FALSE);
    }

    void releaseMouseCapture(HWND window)
    {
        stopPointerUpdates(window);
        if (GetCapture() == window)
            ReleaseCapture();
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    // TODO: W tym miejscu umieść kod.
    cad::win32::AppState appState{cad::editor::EditorSession{cad::createDemoScene()}};

    // Inicjuj ciągi globalne
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PROJECR1GK, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Wykonaj inicjowanie aplikacji:
    if (!InitInstance (hInstance, nCmdShow, appState))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PROJECR1GK));

    MSG msg;

    // Główna pętla komunikatów:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (cad::win32::handleHelpDialogMessage(appState.helpWindow, msg) ||
            cad::win32::handleHelpDialogMessage(appState.documentationWindow, msg))
            continue;
        const HWND root = GetAncestor(msg.hwnd, GA_ROOT);
        const bool fromToolbarControl = msg.hwnd != root &&
            (GetDlgCtrlID(msg.hwnd) == IDC_LINE_GDI || GetDlgCtrlID(msg.hwnd) == IDC_LINE_BRESENHAM ||
                GetDlgCtrlID(msg.hwnd) == IDC_DOCUMENTATION);
        if (fromToolbarControl && msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_ESCAPE || msg.wParam == VK_DELETE || msg.wParam == VK_F1))
        {
            SendMessageW(root, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        if (!TranslateAccelerator(root, hAccelTable, &msg) &&
            !(fromToolbarControl && IsDialogMessageW(root, &msg)))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNKCJA: MyRegisterClass()
//
//  PRZEZNACZENIE: Rejestruje klasę okna.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PROJECR1GK));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = nullptr;
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_PROJECR1GK);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNKCJA: InitInstance(HINSTANCE, int)
//
//   PRZEZNACZENIE: Zapisuje dojście wystąpienia i tworzy okno główne
//
//   KOMENTARZE:
//
//        W tej funkcji dojście wystąpienia jest zapisywane w zmiennej globalnej i
//        jest tworzone i wyświetlane okno główne programu.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, cad::win32::AppState& appState)
{
   hInst = hInstance; // Przechowuj dojście wystąpienia w naszej zmiennej globalnej

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, &appState);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNKCJA: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PRZEZNACZENIE: Przetwarza komunikaty dla okna głównego.
//
//  WM_COMMAND  - przetwarzaj menu aplikacji
//  WM_PAINT    - Maluj okno główne
//  WM_DESTROY  - opublikuj komunikat o wyjściu i wróć
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        auto* state = getAppState(hWnd);
        if (!state || !createAlgorithmControls(hWnd, state->lineAlgorithm))
            return -1;
        // Parent repaints must not paint over the native radio controls.
        SetWindowLongPtrW(hWnd, GWL_STYLE, GetWindowLongPtrW(hWnd, GWL_STYLE) | WS_CLIPCHILDREN);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (GET_Y_LPARAM(lParam) < algorithmBarHeight)
            return 0;
        stopPointerUpdates(hWnd);
        auto* state = getAppState(hWnd);
        const auto position = mouseWorldPosition(hWnd, lParam);
        if (!state || !position)
            return 0;
        SetFocus(hWnd);
        state->showConsistencyWarning = false;
        if (state->editor.pendingParallelEdge())
        {
            const auto second = cad::editor::hitTestEdge(state->editor.document(), *position, 6.0);
            if (second && !state->editor.completeParallelSelection(*second))
            {
                state->editor.cancelParallelSelection();
                MessageBoxW(hWnd,
                    L"Nie udało się uzyskać spójnego stanu dla tej operacji. Para może już istnieć albo ograniczenia nie pozwalają dopasować geometrii. "
                    L"Dokument nie został zmieniony. Możesz ponowić wybór z menu krawędzi.",
                    L"Równoległość", MB_OK | MB_ICONWARNING);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (state->editor.pointerDown(*position))
        {
            SetCapture(hWnd);
            if (GetCapture() != hWnd)
                state->editor.cancelInteraction();
            else
                state->pointerTimerActive = SetTimer(hWnd, pointerFrameTimer, pointerFrameIntervalMs, nullptr) != 0;
        }
        else
        {
            releaseMouseCapture(hWnd);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        auto* state = getAppState(hWnd);
        if (!state || !state->editor.isDragging())
            return 0;
        const auto position = mouseWorldPosition(hWnd, lParam);
        if (position)
        {
            // Keep only the latest sample. Solving every mouse message can starve
            // low-priority WM_PAINT even when each individual solve is fast.
            if (state->pointerTimerActive)
                state->pendingPointerPosition = *position;
            else
            {
                // Keep dragging functional if the system cannot allocate a timer.
                updatePointerPreview(hWnd, *state, *position);
                UpdateWindow(hWnd);
            }
        }
        return 0;
    }
    case WM_TIMER:
    {
        if (wParam != pointerFrameTimer)
            return DefWindowProcW(hWnd, message, wParam, lParam);
        auto* state = getAppState(hWnd);
        if (!state || !state->pointerTimerActive)
            return 0; // KillTimer can leave an already queued WM_TIMER behind.
        if (!state->editor.isDragging())
        {
            stopPointerUpdates(hWnd);
            return 0;
        }
        if (const auto position = state->pendingPointerPosition)
        {
            state->pendingPointerPosition.reset();
            updatePointerPreview(hWnd, *state, *position);
            // Present this preview before processing the next burst of mouse input.
            UpdateWindow(hWnd);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        auto* state = getAppState(hWnd);
        if (state)
        {
            const auto releasedPosition = mouseWorldPosition(hWnd, lParam);
            const auto finalPosition = releasedPosition ? releasedPosition : state->pendingPointerPosition;
            stopPointerUpdates(hWnd);
            if (state->editor.isDragging())
            {
                // Always solve the final cursor sample, even before the first tick.
                if (finalPosition)
                    updatePointerPreview(hWnd, *state, *finalPosition);
            }
            // Finish before ReleaseCapture sends WM_CAPTURECHANGED.
            state->editor.pointerUp();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        releaseMouseCapture(hWnd);
        return 0;
    }
    case WM_CAPTURECHANGED:
    {
        stopPointerUpdates(hWnd);
        auto* state = getAppState(hWnd);
        if (state && state->editor.cancelInteraction())
        {
            state->showConsistencyWarning = false;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    {
        if (message == WM_KILLFOCUS && IsChild(hWnd, reinterpret_cast<HWND>(wParam)))
            return 0;
        auto* state = getAppState(hWnd);
        if (state)
        {
            const bool cancelled = state->editor.cancelInteraction();
            const bool wasShowing = state->showConsistencyWarning;
            state->showConsistencyWarning = false;
            if (cancelled || wasShowing)
                InvalidateRect(hWnd, nullptr, FALSE);
        }
        releaseMouseCapture(hWnd);
        return 0;
    }
    case WM_HELP:
        SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDM_HELP, 0), 0);
        return TRUE;
    case WM_KEYDOWN:
        if (wParam == VK_F1)
        {
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDM_HELP, 0), 0);
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            if (auto* state = getAppState(hWnd))
            {
                state->editor.escape();
                state->showConsistencyWarning = false;
            }
            releaseMouseCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_DELETE)
        {
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDM_DELETE_VERTEX, 0), 0);
            return 0;
        }
        return DefWindowProcW(hWnd, message, wParam, lParam);
    case WM_CONTEXTMENU:
    {
        auto* state = getAppState(hWnd);
        RECT clientRect{};
        if (!state || !GetClientRect(hWnd, &clientRect))
            return 0;

        POINT screenPosition{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool fromKeyboard = screenPosition.x == -1 && screenPosition.y == -1;
        if (fromKeyboard)
        {
            // Shift+F10 uses the current selection and opens the menu in the client area.
            screenPosition = {
                clientRect.left + (clientRect.right - clientRect.left) / 2,
                clientRect.top + (clientRect.bottom - clientRect.top) / 2};
            if (!ClientToScreen(hWnd, &screenPosition))
                return 0;
        }
        else
        {
            POINT clientPosition = screenPosition;
            if (!ScreenToClient(hWnd, &clientPosition) || !PtInRect(&clientRect, clientPosition))
                return DefWindowProcW(hWnd, message, wParam, lParam);
            if (clientPosition.y < algorithmBarHeight)
                return 0;

            state->editor.selectAt(clientToWorld(clientPosition, clientRect));
        }

        state->editor.pointerUp();
        state->editor.cancelParallelSelection();
        state->showConsistencyWarning = false;
        releaseMouseCapture(hWnd);

        InvalidateRect(hWnd, nullptr, FALSE);
        UpdateWindow(hWnd);

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return 0;

        const bool canSplit = state->editor.canSplitSelectedEdge();
        const UINT flags = MF_STRING | (canSplit ? MF_ENABLED : MF_GRAYED);
        const UINT deleteFlags = MF_STRING | (state->editor.canDeleteSelectedVertex() ? MF_ENABLED : MF_GRAYED);
        const UINT lengthFlags = MF_STRING | (state->editor.selectedEdgeLength() ? MF_ENABLED : MF_GRAYED);
        const UINT removeLengthFlags = MF_STRING | (state->editor.selectedFixedLength() ? MF_ENABLED : MF_GRAYED);
        const auto edgeLength = state->editor.selectedEdgeLength();
        const UINT parallelFlags = MF_STRING | (edgeLength && *edgeLength > 0.0 ? MF_ENABLED : MF_GRAYED);
        const auto menuEdge = state->editor.selection().edge;
        std::vector<cad::EdgeId> parallelRemovalTargets;
        if (!AppendMenuW(menu, flags, IDM_SPLIT_EDGE, L"Podziel krawędź") ||
            !appendStrokeWidthMenu(menu, state->editor) ||
            !AppendMenuW(menu, deleteFlags, IDM_DELETE_VERTEX, L"Usuń wierzchołek\tDelete") ||
            !appendContinuityMenu(menu, state->editor) ||
            !AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) ||
            !AppendMenuW(menu, lengthFlags, IDM_SET_FIXED_LENGTH,
                state->editor.selectedFixedLength() ? L"Zmień stałą długość..." : L"Ustaw stałą długość...") ||
            !AppendMenuW(menu, removeLengthFlags, IDM_REMOVE_FIXED_LENGTH, L"Usuń ograniczenie długości") ||
            !AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) ||
            !AppendMenuW(menu, parallelFlags, IDM_ADD_PARALLEL, L"Ustaw równoległość z...") ||
            !appendParallelRemovalMenu(menu, state->editor, parallelRemovalTargets) ||
            !AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) ||
            !appendEdgeConversionMenu(menu, state->editor))
        {
            DestroyMenu(menu);
            return 0;
        }

        const UINT command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            screenPosition.x, screenPosition.y, 0, hWnd, nullptr);
        DestroyMenu(menu);
        if (command >= parallelRemovalCommandBase && command - parallelRemovalCommandBase < parallelRemovalTargets.size())
        {
            const cad::EdgeId other = parallelRemovalTargets[command - parallelRemovalCommandBase];
            if (state->editor.selection().edge == menuEdge && state->editor.removeSelectedEdgeParallel(other))
            {
                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (command != 0)
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return 0;
    }
    case WM_COMMAND:
        {
            const int controlId = LOWORD(wParam);
            if (controlId == IDM_HELP || controlId == IDM_DOCUMENTATION || controlId == IDC_DOCUMENTATION)
            {
                if (controlId == IDC_DOCUMENTATION && (HIWORD(wParam) != BN_CLICKED ||
                    !lParam || reinterpret_cast<HWND>(lParam) != GetDlgItem(hWnd, IDC_DOCUMENTATION)))
                    return 0;
                if (auto* state = getAppState(hWnd))
                {
                    const bool documentation = controlId != IDM_HELP;
                    const HWND help = documentation
                        ? cad::win32::createDocumentationDialog(hInst, hWnd, state->documentationWindow)
                        : cad::win32::createHelpDialog(hInst, hWnd, state->helpWindow);
                    if (!help)
                    {
                        MessageBoxW(hWnd, documentation ? L"Nie udało się otworzyć dokumentacji." : L"Nie udało się otworzyć pomocy.",
                            documentation ? L"Dokumentacja" : L"Pomoc", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    if (state->editor.cancelInteraction())
                    {
                        state->showConsistencyWarning = false;
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                    releaseMouseCapture(hWnd);
                    ShowWindow(help, SW_SHOWNORMAL);
                    SetForegroundWindow(help);
                }
                return 0;
            }
            if (controlId == IDC_LINE_GDI || controlId == IDC_LINE_BRESENHAM || controlId == IDC_LINE_WU)
            {
                if (HIWORD(wParam) == BN_CLICKED && lParam &&
                    reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, controlId))
                    if (auto* state = getAppState(hWnd))
                    {
                        state->lineAlgorithm = controlId == IDC_LINE_WU ? cad::render::LineAlgorithm::Wu :
                            controlId == IDC_LINE_GDI ? cad::render::LineAlgorithm::Gdi : cad::render::LineAlgorithm::Bresenham;
                        checkAlgorithmControls(hWnd, state->lineAlgorithm);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                return 0;
            }
            if (auto* state = getAppState(hWnd); state && state->showConsistencyWarning)
            {
                state->showConsistencyWarning = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            int wmId = LOWORD(wParam);
            // Analizuj zaznaczenia menu:
            switch (wmId)
            {
            case IDM_CONTINUITY_G0:
            case IDM_CONTINUITY_G1:
            case IDM_CONTINUITY_C1:
            {
                auto* state = getAppState(hWnd);
                if (!state || !state->editor.selectedVertexContinuity())
                    return 0;

                const cad::Continuity continuity = wmId == IDM_CONTINUITY_G0 ? cad::Continuity::G0
                    : wmId == IDM_CONTINUITY_G1 ? cad::Continuity::G1 : cad::Continuity::C1;
                if (!state->editor.setSelectedVertexContinuity(continuity))
                {
                    MessageBoxW(hWnd,
                        L"Nie udało się uzyskać spójnego stanu dla wybranej ciągłości. "
                        L"Dokument i poprzednie ustawienie nie zostały zmienione.",
                        L"Ciągłość wierzchołka", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_CONVERT_TO_BEZIER:
            case IDM_CONVERT_TO_ARC:
            case IDM_CONVERT_TO_SEGMENT:
            {
                auto* state = getAppState(hWnd);
                if (!state || !state->editor.selection().edge)
                    return 0;
                const bool converted = wmId == IDM_CONVERT_TO_BEZIER
                    ? state->editor.convertSelectedEdgeToBezier()
                    : wmId == IDM_CONVERT_TO_ARC ? state->editor.convertSelectedEdgeToArc()
                    : state->editor.convertSelectedEdgeToSegment();
                if (!converted)
                {
                    MessageBoxW(hWnd, wmId == IDM_CONVERT_TO_ARC
                        ? L"Nie udało się zmienić krawędzi na łuk. Łuk wymaga różnych końców i ciągłości G0/G1, "
                          L"z G1 najwyżej na jednym końcu. Sprawdź te ustawienia oraz ograniczenia sąsiadów. "
                          L"Dokument nie został zmieniony."
                        : L"Nie udało się zmienić typu krawędzi. Dokument nie został zmieniony.",
                        L"Konwersja krawędzi", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_ADD_PARALLEL:
            {
                auto* state = getAppState(hWnd);
                if (!state)
                    return 0;
                state->editor.pointerUp();
                releaseMouseCapture(hWnd);
                if (state->editor.beginParallelSelection())
                    InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_SET_EDGE_WIDTH:
            {
                auto* state = getAppState(hWnd);
                if (!state) return 0;
                const auto current = state->editor.selectedEdgeStrokeWidth();
                if (!current) return 0;
                stopPointerUpdates(hWnd);
                state->editor.pointerUp();
                releaseMouseCapture(hWnd);
                const auto width = cad::win32::askStrokeWidth(hInst, hWnd, *current);
                if (!width) return 0;
                if (!state->editor.setSelectedEdgeStrokeWidth(*width))
                    MessageBoxW(hWnd, L"Nie udało się zmienić grubości krawędzi.", L"Grubość krawędzi", MB_OK | MB_ICONWARNING);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_SET_FIXED_LENGTH:
            {
                auto* state = getAppState(hWnd);
                if (!state)
                    return 0;
                const auto currentLength = state->editor.selectedEdgeLength();
                if (!currentLength)
                    return 0;
                const auto length = cad::win32::askFixedLength(hInst, hWnd, *currentLength);
                if (!length)
                    return 0;
                if (!state->editor.setSelectedEdgeLength(*length))
                {
                    MessageBoxW(hWnd, L"Nie udało się uzyskać spójnego stanu dla podanej długości. Dokument nie został zmieniony.",
                        L"Stała długość", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_REMOVE_FIXED_LENGTH:
            {
                auto* state = getAppState(hWnd);
                if (state && state->editor.removeSelectedEdgeLength())
                {
                    releaseMouseCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            case IDM_DELETE_VERTEX:
            {
                auto* state = getAppState(hWnd);
                if (!state || !state->editor.canDeleteSelectedVertex())
                    return 0;

                if (!state->editor.deleteSelectedVertex())
                {
                    MessageBoxW(hWnd, L"Nie udało się usunąć wybranego wierzchołka.",
                        L"Usuwanie wierzchołka", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_SPLIT_EDGE:
            {
                auto* state = getAppState(hWnd);
                if (!state || !state->editor.canSplitSelectedEdge())
                    return 0;

                if (!state->editor.splitSelectedEdge())
                {
                    MessageBoxW(hWnd, L"Nie udało się podzielić wybranej krawędzi.",
                        L"Podział krawędzi", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            case IDM_SCENE_SAVE_AS:
            case IDM_SCENE_LOAD:
            {
                auto* state = getAppState(hWnd);
                if (!state) return 0;
                // End the gesture before a modal dialog takes focus/capture. Keep
                // the last accepted preview; do not serialize pending mouse input.
                stopPointerUpdates(hWnd);
                state->editor.pointerUp();
                state->editor.cancelParallelSelection();
                releaseMouseCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                const bool saving = wmId == IDM_SCENE_SAVE_AS;
                const auto path = cad::win32::askScenePath(hWnd, saving);
                if (!path) return 0;
                if (saving)
                {
                    std::wstring error;
                    if (!cad::win32::saveSceneFile(*path, state->editor.document(), error))
                        MessageBoxW(hWnd, error.c_str(), L"Nie udało się zapisać sceny", MB_OK | MB_ICONERROR);
                }
                else
                {
                    auto loaded = cad::win32::loadSceneFile(*path);
                    if (!loaded.document)
                    {
                        const auto errorText = loaded.error + L"\n\nBieżąca scena nie została zmieniona.";
                        MessageBoxW(hWnd, errorText.c_str(), L"Nie udało się załadować sceny", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    state->editor = cad::editor::EditorSession(std::move(*loaded.document));
                    state->showConsistencyWarning = false;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_NCCREATE:
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);

                auto* state = static_cast<cad::win32::AppState*>(create->lpCreateParams);

                if (!state)
                    return FALSE;

                SetWindowLongPtrW(
                    hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state)
                );

                return DefWindowProcW(hWnd, message, wParam, lParam);
            }
    case WM_SIZE:
        {
            auto* state = reinterpret_cast<cad::win32::AppState*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA));

            if (wParam == SIZE_MINIMIZED)
            {
                if (state)
                {
                    state->editor.cancelInteraction();
                    state->showConsistencyWarning = false;
                    releaseMouseCapture(hWnd);
                    state->backBuffer.reset();
                }
            }
            else
            {
                // The next paint recreates the buffer if the client size changed.
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
    case WM_ERASEBKGND:
        // WM_PAINT supplies the complete background together with the geometry.
        return 1;
    case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hWnd, &ps);

            auto* state = reinterpret_cast<cad::win32::AppState*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA)
            );

            RECT clientRect{};
            if (dc && GetClientRect(hWnd, &clientRect))
            {
                const int width = clientRect.right - clientRect.left;
                const int height = clientRect.bottom - clientRect.top;
                if (width > 0 && height > 0)
                {
                    const auto drawFrame = [&](HDC targetDc, cad::win32::BackBuffer* buffer = nullptr)
                    {
                        FillRect(targetDc, &clientRect, GetSysColorBrush(COLOR_WINDOW));
                        if (state)
                        {
                            RECT canvasRect = clientRect;
                            canvasRect.top = (std::min)(canvasRect.bottom, canvasRect.top + algorithmBarHeight);
                            const auto& selection = state->editor.selection();
                            cad::win32::drawDocument(targetDc, state->editor.document(), canvasRect,
                                selection.vertex, selection.polygon, selection.edge, state->editor.pendingParallelEdge(),
                                selection.controlPoint, state->lineAlgorithm, buffer);
                            if (state->showConsistencyWarning)
                                drawConsistencyWarning(targetDc, canvasRect);
                        }
                        drawAlgorithmBar(targetDc, clientRect);
                    };

                    if (state && state->backBuffer.resize(dc, width, height))
                    {
                        drawFrame(state->backBuffer.dc(), &state->backBuffer);
                        if (!state->backBuffer.present(dc))
                            drawFrame(dc);
                    }
                    else
                    {
                        // Keep painting functional if allocating a buffer fails.
                        drawFrame(dc);
                    }
                }
            }

            EndPaint(hWnd, &ps);
            return 0;
        }
    case WM_DESTROY:
        stopPointerUpdates(hWnd);
        PostQuitMessage(0);
        break;
    case WM_NCDESTROY:
        {
            stopPointerUpdates(hWnd);
            auto* state = reinterpret_cast<cad::win32::AppState*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (state)
            {
                if (state->helpWindow)
                    DestroyWindow(state->helpWindow);
                if (state->documentationWindow)
                    DestroyWindow(state->documentationWindow);
                state->editor.cancelInteraction();
                state->backBuffer.reset();
            }

            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Procedura obsługi komunikatów dla okna informacji o programie.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
