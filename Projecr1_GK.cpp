// Projecr1_GK.cpp : Definiuje punkt wejścia dla aplikacji.
//

#include "framework.h"
#include "Projecr1_GK.h"

#include <platform/win32/AppState.h>
#include <platform/win32/GdiRenderer.h>
#include <platform/win32/LengthDialog.h>
#include <cad_core/demo/DemoScene.hpp>
#include <editor/HitTester.h>

#include <windowsx.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>


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

    void releaseMouseCapture(HWND window)
    {
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
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
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
    case WM_LBUTTONDOWN:
    {
        auto* state = getAppState(hWnd);
        const auto position = mouseWorldPosition(hWnd, lParam);
        if (!state || !position)
            return 0;
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
            const bool accepted = state->editor.pointerMove(*position);
            const bool warningChanged = state->showConsistencyWarning != !accepted;
            state->showConsistencyWarning = !accepted;
            if (accepted || warningChanged)
                InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        auto* state = getAppState(hWnd);
        if (state)
        {
            if (state->editor.isDragging())
            {
                if (const auto position = mouseWorldPosition(hWnd, lParam))
                    state->showConsistencyWarning = !state->editor.pointerMove(*position);
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
    case WM_KEYDOWN:
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
            !AppendMenuW(menu, deleteFlags, IDM_DELETE_VERTEX, L"Usuń wierzchołek\tDelete") ||
            !AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) ||
            !AppendMenuW(menu, lengthFlags, IDM_SET_FIXED_LENGTH,
                state->editor.selectedFixedLength() ? L"Zmień stałą długość..." : L"Ustaw stałą długość...") ||
            !AppendMenuW(menu, removeLengthFlags, IDM_REMOVE_FIXED_LENGTH, L"Usuń ograniczenie długości") ||
            !AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) ||
            !AppendMenuW(menu, parallelFlags, IDM_ADD_PARALLEL, L"Ustaw równoległość z...") ||
            !appendParallelRemovalMenu(menu, state->editor, parallelRemovalTargets))
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
            if (auto* state = getAppState(hWnd); state && state->showConsistencyWarning)
            {
                state->showConsistencyWarning = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            int wmId = LOWORD(wParam);
            // Analizuj zaznaczenia menu:
            switch (wmId)
            {
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
                    const auto drawFrame = [&](HDC targetDc)
                    {
                        FillRect(targetDc, &clientRect, GetSysColorBrush(COLOR_WINDOW));
                        if (state)
                        {
                            const auto& selection = state->editor.selection();
                            cad::win32::drawDocument(targetDc, state->editor.document(), clientRect,
                                selection.vertex, selection.polygon, selection.edge, state->editor.pendingParallelEdge());
                            if (state->showConsistencyWarning)
                                drawConsistencyWarning(targetDc, clientRect);
                        }
                    };

                    if (state && state->backBuffer.resize(dc, width, height))
                    {
                        drawFrame(state->backBuffer.dc());
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
        PostQuitMessage(0);
        break;
    case WM_NCDESTROY:
        {
            auto* state = reinterpret_cast<cad::win32::AppState*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (state)
            {
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
