#include <SweetDream/Gui/Window.h>

#include <SweetDream/Core/IO/Logging.h>

#include <SDL3/SDL.h>

#include <cassert>
#include <limits>

namespace sd
{

Window::~Window()
{
    Destroy();
}

bool Window::Create(const char* title, int width, int height, SDL_WindowFlags flags)
{
    assert(title != nullptr);
    if (width <= 0 || height <= 0)
    {
        SD_LOG(err, "Window::Create requires a positive window size");
        return false;
    }

    if (m_handle != nullptr)
    {
        SD_LOG(err, "Window::Create called for an existing window");
        return false;
    }

    m_handle = SDL_CreateWindow(title, width, height, flags);
    if (m_handle == nullptr)
    {
        SD_LOG(err, "SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    m_id = SDL_GetWindowID(m_handle);
    if (m_id == 0)
    {
        SD_LOG(err, "SDL_GetWindowID failed: {}", SDL_GetError());
        SDL_DestroyWindow(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_closeRequested = false;
    return true;
}

void Window::Destroy()
{
    if (m_handle == nullptr)
    {
        return;
    }

    SDL_Window* handle = m_handle;
    m_handle = nullptr;
    m_id = 0;
    m_closeRequested = false;
    SDL_DestroyWindow(handle);
    OnDestroyed();
}

float Window::GetPixelDensity() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowPixelDensity(m_handle);
}

float Window::GetDisplayScale() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowDisplayScale(m_handle);
}

bool Window::SetFullscreenMode(const SDL_DisplayMode* mode)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowFullscreenMode(m_handle, mode);
}

bool Window::SetFullscreenMode(const SDL_DisplayMode& mode)
{
    assert(m_handle != nullptr);
    return SetFullscreenMode(&mode);
}

bool Window::SetBorderlessFullscreenDesktopMode()
{
    return SetFullscreenMode(nullptr);
}

const SDL_DisplayMode* Window::GetFullscreenMode() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowFullscreenMode(m_handle);
}

std::vector<uint8_t> Window::GetICCProfile() const
{
    assert(m_handle != nullptr);
    size_t size = 0;
    void* profile = SDL_GetWindowICCProfile(m_handle, &size);
    if (profile == nullptr)
    {
        return {};
    }

    try
    {
        const auto* bytes = static_cast<const uint8_t*>(profile);
        std::vector<uint8_t> result(bytes, bytes + size);
        SDL_free(profile);
        return result;
    }
    catch (...)
    {
        SDL_free(profile);
        throw;
    }
}

SDL_PixelFormat Window::GetPixelFormat() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowPixelFormat(m_handle);
}

SDL_WindowFlags Window::GetFlags() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowFlags(m_handle);
}

bool Window::SetTitle(const char* title)
{
    assert(m_handle != nullptr);
    assert(title != nullptr);
    return SDL_SetWindowTitle(m_handle, title);
}

const char* Window::GetTitle() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowTitle(m_handle);
}

bool Window::SetIcon(SDL_Surface* icon)
{
    assert(m_handle != nullptr);
    assert(icon != nullptr);
    return SDL_SetWindowIcon(m_handle, icon);
}

bool Window::SetPosition(int x, int y)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowPosition(m_handle, x, y);
}

bool Window::GetPosition(int* x, int* y) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowPosition(m_handle, x, y);
}

bool Window::SetSize(int width, int height)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowSize(m_handle, width, height);
}

bool Window::GetSize(int* width, int* height) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowSize(m_handle, width, height);
}

bool Window::GetSafeArea(SDL_Rect* rect) const
{
    assert(m_handle != nullptr);
    assert(rect != nullptr);
    return SDL_GetWindowSafeArea(m_handle, rect);
}

bool Window::SetAspectRatio(float minimumAspect, float maximumAspect)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowAspectRatio(m_handle, minimumAspect, maximumAspect);
}

bool Window::GetAspectRatio(float* minimumAspect, float* maximumAspect) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowAspectRatio(m_handle, minimumAspect, maximumAspect);
}

bool Window::GetBordersSize(int* top, int* left, int* bottom, int* right) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowBordersSize(m_handle, top, left, bottom, right);
}

bool Window::GetSizeInPixels(int* width, int* height) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowSizeInPixels(m_handle, width, height);
}

bool Window::SetMinimumSize(int width, int height)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowMinimumSize(m_handle, width, height);
}

bool Window::GetMinimumSize(int* width, int* height) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowMinimumSize(m_handle, width, height);
}

bool Window::SetMaximumSize(int width, int height)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowMaximumSize(m_handle, width, height);
}

bool Window::GetMaximumSize(int* width, int* height) const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowMaximumSize(m_handle, width, height);
}

bool Window::SetBordered(bool bordered)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowBordered(m_handle, bordered);
}

bool Window::SetResizable(bool resizable)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowResizable(m_handle, resizable);
}

bool Window::SetAlwaysOnTop(bool onTop)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowAlwaysOnTop(m_handle, onTop);
}

bool Window::Show()
{
    assert(m_handle != nullptr);
    return SDL_ShowWindow(m_handle);
}

bool Window::Hide()
{
    assert(m_handle != nullptr);
    return SDL_HideWindow(m_handle);
}

bool Window::Raise()
{
    assert(m_handle != nullptr);
    return SDL_RaiseWindow(m_handle);
}

bool Window::Maximize()
{
    assert(m_handle != nullptr);
    return SDL_MaximizeWindow(m_handle);
}

bool Window::Minimize()
{
    assert(m_handle != nullptr);
    return SDL_MinimizeWindow(m_handle);
}

bool Window::Restore()
{
    assert(m_handle != nullptr);
    return SDL_RestoreWindow(m_handle);
}

bool Window::SetFullscreen(bool fullscreen)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowFullscreen(m_handle, fullscreen);
}

bool Window::Sync()
{
    assert(m_handle != nullptr);
    return SDL_SyncWindow(m_handle);
}

bool Window::HasSurface() const
{
    assert(m_handle != nullptr);
    return SDL_WindowHasSurface(m_handle);
}

SDL_Surface* Window::GetSurface() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowSurface(m_handle);
}

bool Window::SetSurfaceVSync(int vsync)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowSurfaceVSync(m_handle, vsync);
}

bool Window::GetSurfaceVSync(int* vsync) const
{
    assert(m_handle != nullptr);
    assert(vsync != nullptr);
    return SDL_GetWindowSurfaceVSync(m_handle, vsync);
}

bool Window::UpdateSurface()
{
    assert(m_handle != nullptr);
    return SDL_UpdateWindowSurface(m_handle);
}

bool Window::UpdateSurfaceRects(std::span<const SDL_Rect> rects)
{
    assert(m_handle != nullptr);
    if (rects.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    return SDL_UpdateWindowSurfaceRects(m_handle, rects.data(), static_cast<int>(rects.size()));
}

bool Window::DestroySurface()
{
    assert(m_handle != nullptr);
    return SDL_DestroyWindowSurface(m_handle);
}

bool Window::SetKeyboardGrab(bool grabbed)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowKeyboardGrab(m_handle, grabbed);
}

bool Window::SetMouseGrab(bool grabbed)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowMouseGrab(m_handle, grabbed);
}

bool Window::GetKeyboardGrab() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowKeyboardGrab(m_handle);
}

bool Window::GetMouseGrab() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowMouseGrab(m_handle);
}

bool Window::SetMouseRect(const SDL_Rect* rect)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowMouseRect(m_handle, rect);
}

bool Window::SetMouseRect(const SDL_Rect& rect)
{
    assert(m_handle != nullptr);
    return SetMouseRect(&rect);
}

const SDL_Rect* Window::GetMouseRect() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowMouseRect(m_handle);
}

bool Window::SetOpacity(float opacity)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowOpacity(m_handle, opacity);
}

float Window::GetOpacity() const
{
    assert(m_handle != nullptr);
    return SDL_GetWindowOpacity(m_handle);
}

bool Window::SetFocusable(bool focusable)
{
    assert(m_handle != nullptr);
    return SDL_SetWindowFocusable(m_handle, focusable);
}

bool Window::ShowSystemMenu(int x, int y)
{
    assert(m_handle != nullptr);
    return SDL_ShowWindowSystemMenu(m_handle, x, y);
}

bool Window::SetShape(SDL_Surface* shape)
{
    assert(m_handle != nullptr);
    assert(shape != nullptr);
    return SDL_SetWindowShape(m_handle, shape);
}

bool Window::Flash(SDL_FlashOperation operation)
{
    assert(m_handle != nullptr);
    return SDL_FlashWindow(m_handle, operation);
}

bool Window::OnEvent(const SDL_Event& event)
{
    if (m_handle == nullptr || m_id == 0)
    {
        return false;
    }

    const bool isWindowEvent =
        event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST;
    if ((isWindowEvent && event.window.windowID != m_id) ||
        (!isWindowEvent && SDL_GetWindowFromEvent(&event) != m_handle))
    {
        return false;
    }

    if (isWindowEvent)
    {
        OnWindowEvent(event.window);
    }
    else
    {
        switch (event.type)
        {
        case SDL_EVENT_KEY_DOWN:
            OnKeyDown(event.key);
            break;
        case SDL_EVENT_KEY_UP:
            OnKeyUp(event.key);
            break;
        case SDL_EVENT_TEXT_EDITING:
            OnTextEditing(event.edit);
            break;
        case SDL_EVENT_TEXT_INPUT:
            OnTextInput(event.text);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            OnMouseMove(event.motion);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            OnMouseButtonDown(event.button);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            OnMouseButtonUp(event.button);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            OnMouseWheel(event.wheel);
            break;
        default:
            if (event.type >= SDL_EVENT_USER)
            {
                OnUserEvent(event.user);
            }
            break;
        }
    }

    return false;
}

void Window::OnWindowEvent(const SDL_WindowEvent& event)
{
    switch (event.type)
    {
    case SDL_EVENT_WINDOW_SHOWN:
        OnShown();
        break;
    case SDL_EVENT_WINDOW_HIDDEN:
        OnHidden();
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
        OnExposed();
        break;
    case SDL_EVENT_WINDOW_MOVED:
        OnMoved(event.data1, event.data2);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        OnResized(event.data1, event.data2);
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        OnPixelSizeChanged(event.data1, event.data2);
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        OnMinimized();
        break;
    case SDL_EVENT_WINDOW_MAXIMIZED:
        OnMaximized();
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        OnRestored();
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        OnMouseEnter();
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        OnMouseLeave();
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        OnFocusGained();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        OnFocusLost();
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        OnCloseRequested();
        break;
    case SDL_EVENT_WINDOW_HIT_TEST:
        OnHitTest();
        break;
    case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
        OnIccProfileChanged();
        break;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        OnDisplayChanged();
        break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        OnDisplayScaleChanged();
        break;
    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
        OnSafeAreaChanged();
        break;
    case SDL_EVENT_WINDOW_OCCLUDED:
        OnOccluded();
        break;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        OnEnterFullscreen();
        break;
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        OnLeaveFullscreen();
        break;
    case SDL_EVENT_WINDOW_DESTROYED:
        m_handle = nullptr;
        m_id = 0;
        m_closeRequested = false;
        OnDestroyed();
        break;
    case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
        OnHdrStateChanged();
        break;
    default:
        break;
    }
}

} // namespace sd
