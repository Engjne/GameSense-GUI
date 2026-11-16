#pragma once
#include "popup_menu.hpp"
#include <memory>

namespace skeet {
struct HotkeyLink {
    HotkeyLink* next = nullptr;
    HotkeyLink* previous = nullptr;
};
#if defined(_M_IX86) || defined(__i386__)
#define SKEET_HOTKEY_CALLBACK __fastcall
#else
#define SKEET_HOTKEY_CALLBACK
#endif
struct RegisteredHotkey;
using HotkeyCallback = void(SKEET_HOTKEY_CALLBACK*)(void*, RegisteredHotkey&);
struct RegisteredHotkey : HotkeyLink {
    void* callback_object = nullptr;
    HotkeyCallback callback = nullptr;
    HotkeyBinding binding{0, HotkeyMode::always};
};
}

namespace skeet::ui {
using skeet::RegisteredHotkey;

class HotkeyWidget : public Widget {
public:
    HotkeyWidget();
    HotkeyWidget(Widget* owner, RegisteredHotkey* setting, bool allow_mode,
                 WidgetIdentities& identities);
    RegisteredHotkey* value = nullptr;
    bool capturing = false;
    bool held = false;
    std::array<wchar_t, 4> key_text{};
    std::unique_ptr<PopupMenu> popup;

    void set_key(std::uint32_t key, std::uint32_t key_data = 0);
    void set_mode(HotkeyMode mode);
    bool hit_test(Point cursor) const override;
    bool mouse(const InputMessage& message, Point cursor) override;
    bool keyboard(const InputMessage& message) override;
    void value_changed() override;
    void config_loaded() override;
    void reset_input() override;
    std::int32_t input_priority() const override;
    void dpi_changed(float scale, const SliderMetrics& metrics) override;
    void draw(RenderQueue& queue, Point origin, std::uint32_t accent) const override;
    static void mode_selected(Widget& owner, PopupMenu& popup, std::uint32_t index);
};

} // namespace skeet::ui
