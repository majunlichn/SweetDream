#include "MainWindow.h"

#include <SweetDream/Core/IO/Logging.h>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cstdint>

namespace
{

const char* GetMouseButtonName(uint8_t button) noexcept
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return "Left";
    case SDL_BUTTON_MIDDLE:
        return "Middle";
    case SDL_BUTTON_RIGHT:
        return "Right";
    case SDL_BUTTON_X1:
        return "X1";
    case SDL_BUTTON_X2:
        return "X2";
    default:
        return "Unknown";
    }
}

} // namespace

MainWindow::~MainWindow() = default;

void MainWindow::OnShown()
{
    SD_LOG(debug, "MainWindow::OnShown");
}

void MainWindow::OnHidden()
{
    SD_LOG(debug, "MainWindow::OnHidden");
}

void MainWindow::OnMoved(int x, int y)
{
    SD_LOG(debug, "MainWindow::OnMoved: position=({}, {})", x, y);
}

void MainWindow::OnResized(int width, int height)
{
    SD_LOG(debug, "MainWindow::OnResized: size={}x{}", width, height);
}

void MainWindow::OnPixelSizeChanged(int width, int height)
{
    SD_LOG(debug, "MainWindow::OnPixelSizeChanged: size={}x{}", width, height);
}

void MainWindow::OnMinimized()
{
    SD_LOG(debug, "MainWindow::OnMinimized");
}

void MainWindow::OnMaximized()
{
    SD_LOG(debug, "MainWindow::OnMaximized");
}

void MainWindow::OnRestored()
{
    SD_LOG(debug, "MainWindow::OnRestored");
}

void MainWindow::OnFocusGained()
{
    SD_LOG(debug, "MainWindow::OnFocusGained");
}

void MainWindow::OnFocusLost()
{
    SD_LOG(debug, "MainWindow::OnFocusLost");
}

void MainWindow::OnCloseRequested()
{
    SD_LOG(debug, "MainWindow::OnCloseRequested");
    Window::OnCloseRequested();
}

void MainWindow::OnDestroyed()
{
    SD_LOG(debug, "MainWindow::OnDestroyed");
    Window::OnDestroyed();
}

void MainWindow::OnKeyDown(const SDL_KeyboardEvent& event)
{
    SD_LOG(debug, "MainWindow::OnKeyDown: key={}, repeat={}", SDL_GetKeyName(event.key),
           event.repeat);
}

void MainWindow::OnKeyUp(const SDL_KeyboardEvent& event)
{
    SD_LOG(debug, "MainWindow::OnKeyUp: key={}, repeat={}", SDL_GetKeyName(event.key),
           event.repeat);
}

void MainWindow::OnTextEditing(const SDL_TextEditingEvent& event)
{
    SD_LOG(debug, "MainWindow::OnTextEditing: text='{}', start={}, length={}", event.text,
           event.start, event.length);
}

void MainWindow::OnTextInput(const SDL_TextInputEvent& event)
{
    SD_LOG(debug, "MainWindow::OnTextInput: text='{}'", event.text);
}

void MainWindow::OnMouseMove(const SDL_MouseMotionEvent& event)
{
    SD_LOG(debug, "MainWindow::OnMouseMove: position=({:4.0f}, {:4.0f}), delta=({:4.0f}, {:4.0f})",
           event.x, event.y, event.xrel, event.yrel);
}

void MainWindow::OnMouseButtonDown(const SDL_MouseButtonEvent& event)
{
    SD_LOG(debug, "MainWindow::OnMouseButtonDown: button={}",
           GetMouseButtonName(event.button));
}

void MainWindow::OnMouseButtonUp(const SDL_MouseButtonEvent& event)
{
    SD_LOG(debug, "MainWindow::OnMouseButtonUp: button={}", GetMouseButtonName(event.button));
}

void MainWindow::OnMouseWheel(const SDL_MouseWheelEvent& event)
{
    SD_LOG(debug, "MainWindow::OnMouseWheel: delta=({:2.0f}, {:2.0f})", event.x, event.y);
}
