// Projecr1_GK.cpp : Definiuje punkt wejścia dla aplikacji.
//

#include "framework.h"
#include "Projecr1_GK.h"

#include <platform/win32/AppState.h>
#include <platform/win32/GdiRenderer.h>
#include <cad_core/demo/DemoScene.hpp>
#include <editor/HitTester.h>

#include <windowsx.h>


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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    // TODO: W tym miejscu umieść kod.
    cad::win32::AppState appState{cad::createDemoScene()};

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
        auto* state = reinterpret_cast<cad::win32::AppState*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        if (!state)
            return 0;

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT clientRect{};
        GetClientRect(hWnd, &clientRect);

        const cad::Vec2 worldPosition{
            static_cast<double>(x - 20),
            static_cast<double>(clientRect.bottom - 20 - y)
        };


        state->selection = {};
        state->drag = {};

        state->selection.vertex = cad::editor::hitTestVertex(state->document, worldPosition, 6.0);

        state->drag.vertex.reset();
        

        if (state->selection.vertex)
        {
            const auto* vertex = state->document.findVertex(*state->selection.vertex);

            if (vertex)
            {
                state->drag.vertex = vertex->id;
                state->drag.grabOffset = vertex->position - worldPosition;
                SetCapture(hWnd);
            }
        } else if ((state->selection.edge = cad::editor::hitTestEdge(state->document, worldPosition, 6.0))) 
        {
        } else
        {
            state->selection.polygon =
                cad::editor::hitTestPolygon(state->document, worldPosition);
        
            if (state->selection.polygon)
            {
                state->drag.polygon = state->selection.polygon;
                state->drag.previousWorldPosition = worldPosition;
                SetCapture(hWnd);
            }
        }

        InvalidateRect(hWnd, nullptr, FALSE);

        return 0;
        break;
    }
    case WM_MOUSEMOVE:
    {
        auto* state = reinterpret_cast<cad::win32::AppState*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        if (!state || (!state->drag.vertex && !state->drag.polygon))
            return 0;

        RECT clientRect{};
        if (!GetClientRect(hWnd, &clientRect))
            return 0;

        const cad::Vec2 worldPosition{
            static_cast<double>(GET_X_LPARAM(lParam) - 20),
            static_cast<double>(
                clientRect.bottom - 20 - GET_Y_LPARAM(lParam))
        };

        bool moved = false;

        if (state->drag.vertex)
        {
            const cad::Vec2 newPosition =
                worldPosition + state->drag.grabOffset;
        
            moved = state->document.moveVertex(
                *state->drag.vertex, newPosition);
        }
        else if (state->drag.polygon)
        {
            const cad::Vec2 delta =
                worldPosition - state->drag.previousWorldPosition;
        
            moved = state->document.movePolygon(
                *state->drag.polygon, delta);
            
            if (moved)
                state->drag.previousWorldPosition = worldPosition;
        }

        if (moved)
            InvalidateRect(hWnd, nullptr, FALSE);

        return 0;
    }
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
    {
        auto* state = reinterpret_cast<cad::win32::AppState*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        if (state)
            state->drag = {};

        if (message == WM_LBUTTONUP && GetCapture() == hWnd)
            ReleaseCapture();

        return 0;
    }
    case WM_CONTEXTMENU:
    {
        auto* state = reinterpret_cast<cad::win32::AppState*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
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

            const cad::Vec2 worldPosition{
                static_cast<double>(clientPosition.x - 20),
                static_cast<double>(clientRect.bottom - 20 - clientPosition.y)};

            state->selection = {};
            state->selection.vertex = cad::editor::hitTestVertex(state->document, worldPosition, 6.0);
            if (!state->selection.vertex)
            {
                state->selection.edge = cad::editor::hitTestEdge(state->document, worldPosition, 6.0);
                if (!state->selection.edge)
                    state->selection.polygon = cad::editor::hitTestPolygon(state->document, worldPosition);
            }
        }

        state->drag = {};
        if (GetCapture() == hWnd)
            ReleaseCapture();

        InvalidateRect(hWnd, nullptr, FALSE);
        UpdateWindow(hWnd);

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return 0;

        const bool canSplit = state->selection.edge &&
            state->document.findEdge(*state->selection.edge);
        const UINT flags = MF_STRING | (canSplit ? MF_ENABLED : MF_GRAYED);
        if (!AppendMenuW(menu, flags, IDM_SPLIT_EDGE, L"Podziel krawędź"))
        {
            DestroyMenu(menu);
            return 0;
        }

        const UINT command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            screenPosition.x, screenPosition.y, 0, hWnd, nullptr);
        DestroyMenu(menu);
        if (command != 0)
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return 0;
    }
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Analizuj zaznaczenia menu:
            switch (wmId)
            {
            case IDM_SPLIT_EDGE:
            {
                auto* state = reinterpret_cast<cad::win32::AppState*>(
                    GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (!state || !state->selection.edge)
                    return 0;

                const auto vertexId = state->document.splitEdge(*state->selection.edge);
                if (!vertexId)
                {
                    MessageBoxW(hWnd, L"Nie udało się podzielić wybranej krawędzi.",
                        L"Podział krawędzi", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                state->drag = {};
                if (GetCapture() == hWnd)
                    ReleaseCapture();
                state->selection = {};
                state->selection.vertex = vertexId;
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
                    state->backBuffer.reset();
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
                            cad::win32::drawDocument(targetDc, state->document, clientRect, state->selection.vertex, state->selection.polygon, state->selection.edge);
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
                state->backBuffer.reset();

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
