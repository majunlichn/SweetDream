#pragma once

#include <SweetDream/Platform/EventHandler.h>

#include <SDL3/SDL_video.h>

#include <cstdint>
#include <span>
#include <vector>

struct SDL_Surface;

namespace sd
{

class Window : public EventHandler
{
public:
    Window() = default;
    ~Window() override;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] bool Create(const char* title, int width, int height, SDL_WindowFlags flags = 0);
    // Destroy API presentation resources (such as Vulkan swapchains and surfaces)
    // before destroying their native window.
    void Destroy();

    [[nodiscard]] bool IsCreated() const noexcept { return m_handle != nullptr; }
    [[nodiscard]] bool IsCloseRequested() const noexcept { return m_closeRequested; }
    void CancelCloseRequest() noexcept { m_closeRequested = false; }

    [[nodiscard]] SDL_Window* GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] SDL_WindowID GetID() const noexcept { return m_id; }

    [[nodiscard]] float GetPixelDensity() const;
    [[nodiscard]] float GetDisplayScale() const;
    bool SetFullscreenMode(const SDL_DisplayMode* mode);
    bool SetFullscreenMode(const SDL_DisplayMode& mode);
    bool SetBorderlessFullscreenDesktopMode();
    [[nodiscard]] const SDL_DisplayMode* GetFullscreenMode() const;
    [[nodiscard]] std::vector<uint8_t> GetICCProfile() const;
    [[nodiscard]] SDL_PixelFormat GetPixelFormat() const;

    [[nodiscard]] SDL_WindowFlags GetFlags() const;
    bool SetTitle(const char* title);
    [[nodiscard]] const char* GetTitle() const;
    bool SetIcon(SDL_Surface* icon);
    bool SetPosition(int x, int y);
    bool GetPosition(int* x, int* y) const;
    bool SetSize(int width, int height);
    bool Resize(int width, int height) { return SetSize(width, height); }
    bool GetSize(int* width, int* height) const;
    bool GetSafeArea(SDL_Rect* rect) const;
    bool SetAspectRatio(float minimumAspect, float maximumAspect);
    bool GetAspectRatio(float* minimumAspect, float* maximumAspect) const;
    bool GetBordersSize(int* top, int* left, int* bottom, int* right) const;
    bool GetSizeInPixels(int* width, int* height) const;
    bool SetMinimumSize(int width, int height);
    bool GetMinimumSize(int* width, int* height) const;
    bool SetMaximumSize(int width, int height);
    bool GetMaximumSize(int* width, int* height) const;
    bool SetBordered(bool bordered);
    bool SetResizable(bool resizable);
    bool SetAlwaysOnTop(bool onTop);

    bool Show();
    bool Hide();
    bool Raise();
    bool Maximize();
    bool Minimize();
    bool Restore();
    bool SetFullscreen(bool fullscreen);
    bool Sync();

    [[nodiscard]] bool HasSurface() const;
    [[nodiscard]] SDL_Surface* GetSurface() const;
    bool SetSurfaceVSync(int vsync);
    bool GetSurfaceVSync(int* vsync) const;
    bool UpdateSurface();
    bool UpdateSurfaceRects(std::span<const SDL_Rect> rects);
    bool DestroySurface();

    bool SetKeyboardGrab(bool grabbed);
    bool SetMouseGrab(bool grabbed);
    [[nodiscard]] bool GetKeyboardGrab() const;
    [[nodiscard]] bool GetMouseGrab() const;

    bool SetMouseRect(const SDL_Rect* rect);
    bool SetMouseRect(const SDL_Rect& rect);
    [[nodiscard]] const SDL_Rect* GetMouseRect() const;

    bool SetOpacity(float opacity);
    [[nodiscard]] float GetOpacity() const;
    bool SetFocusable(bool focusable);

    bool ShowSystemMenu(int x, int y);
    bool SetShape(SDL_Surface* shape);
    bool Flash(SDL_FlashOperation operation);

protected:
    bool OnEvent(const SDL_Event& event) override;

    virtual void OnWindowEvent(const SDL_WindowEvent& event);
    virtual void OnShown() {}
    virtual void OnHidden() {}
    virtual void OnExposed() {}
    virtual void OnMoved(int x, int y) {}
    virtual void OnResized(int width, int height) {}
    virtual void OnPixelSizeChanged(int width, int height) {}
    virtual void OnMinimized() {}
    virtual void OnMaximized() {}
    virtual void OnRestored() {}
    virtual void OnMouseEnter() {}
    virtual void OnMouseLeave() {}
    virtual void OnFocusGained() {}
    virtual void OnFocusLost() {}
    virtual void OnCloseRequested() { m_closeRequested = true; }
    virtual void OnHitTest() {}
    virtual void OnIccProfileChanged() {}
    virtual void OnDisplayChanged() {}
    virtual void OnDisplayScaleChanged() {}
    virtual void OnSafeAreaChanged() {}
    virtual void OnOccluded() {}
    virtual void OnEnterFullscreen() {}
    virtual void OnLeaveFullscreen() {}
    virtual void OnHdrStateChanged() {}
    virtual void OnDestroyed() {}

    virtual void OnKeyDown(const SDL_KeyboardEvent& event) {}
    virtual void OnKeyUp(const SDL_KeyboardEvent& event) {}
    virtual void OnTextEditing(const SDL_TextEditingEvent& event) {}
    virtual void OnTextInput(const SDL_TextInputEvent& event) {}

    virtual void OnMouseMove(const SDL_MouseMotionEvent& event) {}
    virtual void OnMouseButtonDown(const SDL_MouseButtonEvent& event) {}
    virtual void OnMouseButtonUp(const SDL_MouseButtonEvent& event) {}
    virtual void OnMouseWheel(const SDL_MouseWheelEvent& event) {}

    virtual void OnUserEvent(const SDL_UserEvent& event) {}

private:
    SDL_Window* m_handle = nullptr;
    SDL_WindowID m_id = 0;
    bool m_closeRequested = false;
}; // class Window

} // namespace sd
