#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace cad::win32
{
    // Creates a hidden modeless dialog, or returns the already open one.
    // windowSlot must outlive the dialog; closing the window clears the slot.
    // The caller shows it and routes messages through handleHelpDialogMessage.
    HWND createHelpDialog(HINSTANCE instance, HWND owner, HWND& windowSlot);
    // Uses the manual embedded as UTF-8 RCDATA, initially showing algorithms.
    HWND createDocumentationDialog(HINSTANCE instance, HWND owner, HWND& windowSlot);
    bool handleHelpDialogMessage(HWND dialog, MSG& message);
}
