#pragma once

#include <SweetDream/Gui/VulkanWindow.h>

class MainWindow final : public sd::VulkanWindow
{
public:
    MainWindow() = default;
    ~MainWindow() override;

protected:
    void OnShown() override;
    void OnHidden() override;
    void OnMoved(int x, int y) override;
    void OnResized(int width, int height) override;
    void OnPixelSizeChanged(int width, int height) override;
    void OnMinimized() override;
    void OnMaximized() override;
    void OnRestored() override;
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCloseRequested() override;
    void OnDestroyed() override;

    void OnKeyDown(const SDL_KeyboardEvent& event) override;
    void OnKeyUp(const SDL_KeyboardEvent& event) override;
    void OnTextEditing(const SDL_TextEditingEvent& event) override;
    void OnTextInput(const SDL_TextInputEvent& event) override;

    void OnMouseMove(const SDL_MouseMotionEvent& event) override;
    void OnMouseButtonDown(const SDL_MouseButtonEvent& event) override;
    void OnMouseButtonUp(const SDL_MouseButtonEvent& event) override;
    void OnMouseWheel(const SDL_MouseWheelEvent& event) override;
}; // class MainWindow
