#include <eacp/Sprites/Sprites.h>

#include "../../PureDOOM.h"

using namespace eacp;

namespace
{
constexpr auto doomWidth = 320;
constexpr auto doomHeight = 200;

// DOOM's 320x200 frame was designed for 4:3 CRTs, whose non-square pixels
// stretched it 1.2x vertically; 320x240 is the intended display shape.
constexpr auto displayWidth = 320.0f;
constexpr auto displayHeight = 240.0f;

// eacp has no display-metrics API yet (see the gap log), so the initial size
// is a guess that fits laptop screens; the window resizes from there.
constexpr auto windowScale = 3;

constexpr auto mouseSpeed = 4.0f;

doom_key_t toDoomKey(const Graphics::KeyEvent& event)
{
    using namespace Graphics;

    switch (event.keyCode)
    {
        case KeyCode::Tab:
            return DOOM_KEY_TAB;
        case KeyCode::Return:
            return DOOM_KEY_ENTER;
        case KeyCode::Escape:
            return DOOM_KEY_ESCAPE;
        case KeyCode::Space:
            return DOOM_KEY_SPACE;
        case KeyCode::Delete:
            return DOOM_KEY_BACKSPACE;
        case KeyCode::LeftArrow:
            return DOOM_KEY_LEFT_ARROW;
        case KeyCode::RightArrow:
            return DOOM_KEY_RIGHT_ARROW;
        case KeyCode::UpArrow:
            return DOOM_KEY_UP_ARROW;
        case KeyCode::DownArrow:
            return DOOM_KEY_DOWN_ARROW;
        case KeyCode::F1:
            return DOOM_KEY_F1;
        case KeyCode::F2:
            return DOOM_KEY_F2;
        case KeyCode::F3:
            return DOOM_KEY_F3;
        case KeyCode::F4:
            return DOOM_KEY_F4;
        case KeyCode::F5:
            return DOOM_KEY_F5;
        case KeyCode::F6:
            return DOOM_KEY_F6;
        case KeyCode::F7:
            return DOOM_KEY_F7;
        case KeyCode::F8:
            return DOOM_KEY_F8;
        case KeyCode::F9:
            return DOOM_KEY_F9;
        case KeyCode::F10:
            return DOOM_KEY_F10;
        case KeyCode::F11:
            return DOOM_KEY_F11;
        case KeyCode::F12:
            return DOOM_KEY_F12;
        default:
            break;
    }

    // DOOM's codes for letters, digits and punctuation are their ASCII
    // values, so everything else maps through the typed character. This also
    // covers keys eacp's KeyCode table has no constant for yet (see the gap
    // log in CLAUDE.md).
    const auto& characters = event.charactersIgnoringModifiers;

    if (characters.size() == 1)
    {
        auto character = characters.front();

        if (character >= 'A' && character <= 'Z')
            character = (char) (character - 'A' + 'a');

        if (character >= ' ' && character <= '~')
            return (doom_key_t) character;
    }

    return DOOM_KEY_UNKNOWN;
}

doom_button_t toDoomButton(Graphics::MouseButton button)
{
    switch (button)
    {
        case Graphics::MouseButton::Right:
            return DOOM_RIGHT_BUTTON;
        case Graphics::MouseButton::Middle:
            return DOOM_MIDDLE_BUTTON;
        default:
            return DOOM_LEFT_BUTTON;
    }
}

Graphics::WindowOptions windowOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = (int) displayWidth * windowScale;
    options.height = (int) displayHeight * windowScale;
    options.title = "Pure DOOM (eacp)";
    options.minWidth = (int) displayWidth;
    options.minHeight = (int) displayHeight;
    return options;
}

// The sprite space is fixed at 320x240 and stretches to the window, so a
// window that isn't 4:3 would distort the frame. This returns the sub-rect
// of that space which maps back to a centered 4:3 area of the window.
Graphics::Rect letterboxedDisplayRect(const Graphics::Rect& bounds)
{
    constexpr auto contentAspect = displayWidth / displayHeight;

    if (bounds.w <= 0.0f || bounds.h <= 0.0f)
        return {0.0f, 0.0f, displayWidth, displayHeight};

    auto viewAspect = bounds.w / bounds.h;

    if (viewAspect > contentAspect)
    {
        auto width = displayWidth * contentAspect / viewAspect;
        return {(displayWidth - width) / 2.0f, 0.0f, width, displayHeight};
    }

    auto height = displayHeight * viewAspect / contentAspect;
    return {0.0f, (displayHeight - height) / 2.0f, displayWidth, height};
}

GPU::Texture makeFramebufferTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = doomWidth;
    descriptor.height = doomHeight;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}
} // namespace

struct DoomView final : GPU::GPUView
{
    DoomView()
    {
        setSampleCount(1);
        sprites.emplace(Graphics::Point {displayWidth, displayHeight},
                        sampleCount());
        setHandlesMouseEvents(true);
        setGrabsFocusOnMouseDown(true);
        setContinuous(true);
    }

    void update(Threads::FrameTime) override
    {
        if (window != nullptr)
        {
            if (window->isCommandPressed())
                window->setMouseLocked(false);

            syncModifierKeys(window->getModifiers());
        }

        doom_update();
    }

    void render(GPU::Frame& frame) override
    {
        framebuffer.update(doom_get_framebuffer(4));

        auto pass = frame.beginPass({Graphics::Color::black()});
        sprites->begin(pass);
        sprites->drawTexture(framebuffer, letterboxedDisplayRect(getLocalBounds()));
    }

    // DOOM binds Ctrl/Shift/Alt as ordinary keys (fire/run/strafe), but eacp
    // reports them only as modifier state, never as key events. Diff the
    // polled state once per frame into synthetic down/up events.
    void syncModifierKeys(const Graphics::ModifierKeys& current)
    {
        syncModifierKey(current.shift, modifiers.shift, DOOM_KEY_SHIFT);
        syncModifierKey(current.control, modifiers.control, DOOM_KEY_CTRL);
        syncModifierKey(current.alt, modifiers.alt, DOOM_KEY_ALT);
        modifiers = current;
    }

    static void syncModifierKey(bool pressed, bool wasPressed, doom_key_t key)
    {
        if (pressed == wasPressed)
            return;

        if (pressed)
            doom_key_down(key);
        else
            doom_key_up(key);
    }

    void keyDown(const Graphics::KeyEvent& event) override
    {
        if (event.isRepeat)
            return;

        if (auto key = toDoomKey(event); key != DOOM_KEY_UNKNOWN)
            doom_key_down(key);
    }

    void keyUp(const Graphics::KeyEvent& event) override
    {
        if (auto key = toDoomKey(event); key != DOOM_KEY_UNKNOWN)
            doom_key_up(key);
    }

    void mouseDown(const Graphics::MouseEvent& event) override
    {
        if (window == nullptr)
            return;

        if (!window->isMouseLocked())
        {
            window->setMouseLocked(true);
            return;
        }

        doom_button_down(toDoomButton(event.button));
    }

    void mouseUp(const Graphics::MouseEvent& event) override
    {
        if (window != nullptr && window->isMouseLocked())
            doom_button_up(toDoomButton(event.button));
    }

    void mouseMoved(const Graphics::MouseEvent& event) override { aim(event); }

    void mouseDragged(const Graphics::MouseEvent& event) override { aim(event); }

    void aim(const Graphics::MouseEvent& event)
    {
        if (window != nullptr && window->isMouseLocked())
            doom_mouse_move((int) (event.delta.x * mouseSpeed),
                            (int) (event.delta.y * mouseSpeed));
    }

    std::optional<Sprites::SpriteRenderer> sprites;
    GPU::Texture framebuffer = makeFramebufferTexture();
    Graphics::Window* window = nullptr;
    Graphics::ModifierKeys modifiers;
};

struct DoomApp
{
    DoomApp()
    {
        window.setContentView(view);
        view.window = &window;
        view.focus();
    }

    DoomView view;
    Graphics::Window window {windowOptions()};
};

int main(int argc, char** argv)
{
    // PureDOOM locates WAD files via DOOMWADDIR (defaulting to the current
    // directory), so point it at the repository's shareware WAD unless the
    // user already chose a directory.
    if (!getEnv("DOOMWADDIR"))
        setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    doom_set_default_int("key_up", DOOM_KEY_W);
    doom_set_default_int("key_down", DOOM_KEY_S);
    doom_set_default_int("key_strafeleft", DOOM_KEY_A);
    doom_set_default_int("key_straferight", DOOM_KEY_D);
    doom_set_default_int("key_use", DOOM_KEY_E);
    doom_set_default_int("mouse_move", 0);

    doom_init(argc, argv, DOOM_FLAG_MENU_DARKEN_BG);

    return Apps::run<DoomApp>();
}
