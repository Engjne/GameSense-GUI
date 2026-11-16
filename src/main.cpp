#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include "skeet/core.hpp"
#include "skeet/menu_window.hpp"
#include "skeet/menu/window_lifetime.hpp"
#include "skeet/menu/window_render.hpp"
#include "skeet/menu_tree.hpp"
#include "skeet/widgets.hpp"
#include "skeet/button.hpp"
#include "skeet/combobox.hpp"
#include "skeet/color_picker.hpp"
#include "skeet/multiselect.hpp"
#include "skeet/textbox.hpp"
#include "skeet/hotkey_widget.hpp"
#include "skeet/render/backend_container.hpp"
#include "skeet/render/menu_window_assets.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {

IDirect3D9* g_d3d = nullptr;
IDirect3DDevice9* g_device = nullptr;
D3DPRESENT_PARAMETERS g_d3dpp{};
skeet::ui::MenuWindow* g_window = nullptr;
skeet::ui::RendererContainer* g_container = nullptr;
bool g_device_lost = false;

// Mock configuration storage bound to UI widgets
struct DemoConfig {
    // Rage tab
    uint8_t rage_enabled = 1;
    int32_t rage_min_damage = 35;
    int32_t rage_hitchance = 65;
    int32_t rage_hitbox = 0;
    uint32_t rage_multipoint = 7;
    uint32_t rage_color = 0xffa0e632;
    uint8_t rage_autowall = 1;
    uint8_t rage_silent = 1;
    int32_t rage_fov = 180;
    skeet::RegisteredHotkey rage_hotkey;

    // Anti-Aim tab
    uint8_t aa_enabled = 1;
    int32_t aa_pitch = 1; // Down
    int32_t aa_yaw = 1;   // 180
    int32_t aa_jitter = 30;
    uint8_t aa_freestand = 1;
    uint8_t fakelag_enabled = 1;
    int32_t fakelag_type = 1;
    int32_t fakelag_limit = 14;

    // Legit tab
    uint8_t legit_enabled = 0;
    int32_t legit_speed = 15;
    int32_t legit_fov = 5;
    int32_t legit_recoil = 80;
    uint8_t trigger_enabled = 0;
    int32_t trigger_delay = 30;

    // Visuals tab
    uint8_t esp_box = 1;
    uint32_t esp_box_color = 0xff50c8ff;
    uint8_t esp_name = 1;
    uint8_t esp_health = 1;
    uint8_t esp_weapon = 1;
    uint8_t esp_skeleton = 1;
    uint8_t chams_enabled = 1;
    int32_t chams_material = 0;
    uint32_t chams_visible_color = 0xffe63278;
    uint32_t chams_invisible_color = 0xff7832e6;

    // Misc tab
    uint8_t bhop_enabled = 1;
    uint8_t strafe_enabled = 1;
    uint8_t fastwalk_enabled = 0;
    uint8_t edgejump_enabled = 0;
    uint8_t watermark_enabled = 1;
    uint8_t rankreveal_enabled = 1;

    // Skins tab
    int32_t knife_model = 0;
    int32_t glove_model = 0;
    int32_t skin_stattrak = 1337;
    int32_t skin_wear = 1;
    int32_t skin_seed = 420;

    // Players tab
    int32_t selected_player = 0;
    uint8_t player_resolver = 1;
    uint8_t player_baim = 0;
    int32_t player_pitch = 0;
    int32_t player_yaw = 0;

    // Config tab
    int32_t selected_preset = 0;
    int32_t dpi_scale = 0;
    uint32_t menu_accent = 0xff96e632; // Gamesense signature accent green
} g_cfg;

class SimpleTab final : public skeet::ui::Tab {
public:
    SimpleTab(skeet::Menu& menu, skeet::ui::MenuInput& input, skeet::ui::MenuGeometry& layout,
              std::u16string_view label, skeet::Point& next, skeet::ui::WidgetIdentities& identities)
        : Tab(menu, input, layout, label, next, identities) {}

    std::vector<std::unique_ptr<skeet::ui::Category>> owned_categories;

    skeet::ui::Category& add_category(std::u16string_view title, skeet::ui::GridPosition pos,
                                      skeet::ui::WidgetIdentities& identities) {
        auto cat = std::make_unique<skeet::ui::Category>(this, title, pos, 16, true, identities);
        auto* ptr = cat.get();
        categories.push_back(ptr);
        owned_categories.push_back(std::move(cat));
        return *ptr;
    }
};

void reset_tab_callback(void* raw) {
    static_cast<skeet::ui::Tab*>(raw)->reset_input();
}
void activate_tab_callback(void* raw) {
    static_cast<skeet::ui::Tab*>(raw)->activate();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        if (g_window) {
            if (g_window->input.mouse_flags & 1) {
                g_window->transition_started = g_window->read_ticks(g_window->clock_context);
                g_window->transition_state = 1; // Fade out
            } else {
                g_window->open();               // Fade in
            }
        }
        return 0;
    }

    if (g_window) {
        skeet::ui::WindowMessage wmsg{
            reinterpret_cast<std::uintptr_t>(hwnd),
            { static_cast<std::uint32_t>(msg), static_cast<std::uint32_t>(wParam), static_cast<std::uint32_t>(lParam) }
        };
        if (g_window->dispatch(wmsg)) {
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    try {
        const wchar_t* window_class = L"SkeetStandaloneWindowClass";
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance;
        wc.lpszClassName = window_class;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassExW(&wc);

        const int window_w = 980;
        const int window_h = 720;
        HWND hwnd = CreateWindowExW(
            0, window_class, L"Gamesense / Skeet GUI - Standalone Engine",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            150, 100, window_w, window_h,
            nullptr, nullptr, instance, nullptr
        );

        if (!hwnd) {
            MessageBoxA(nullptr, "Failed to create application window.", "Initialization Error", MB_ICONERROR);
            return 1;
        }

        ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
        g_d3dpp.Windowed = TRUE;
        g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        g_d3dpp.hDeviceWindow = hwnd;
        g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        g_d3dpp.EnableAutoDepthStencil = FALSE;
        g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

        // Modern Windows 10/11 GPU drivers require Direct3DCreate9Ex for HAL devices
        IDirect3D9Ex* d3dEx = nullptr;
        IDirect3DDevice9Ex* devEx = nullptr;
        if (SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dEx))) {
            if (SUCCEEDED(d3dEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                                D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, nullptr, &devEx))) {
                g_device = devEx;
                g_d3d = d3dEx;
            }
        }

        if (!g_device) {
            g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
            if (!g_d3d || FAILED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                                    D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_device))) {
                MessageBoxA(nullptr, "Failed to initialize Direct3D 9 device.", "Direct3D Error", MB_ICONERROR);
                return 1;
            }
        }

        skeet::ui::RendererContainer container(g_device);
        g_container = &container;

        // Configure authentic native font definitions
        static const std::array<skeet::ui::FontDefinition, 7> initial_fonts{{
            {u"Verdana", 9, 2},
            {u"Verdana", 9, 3},
            {u"Calibri", 24, 3},
            {u"Segoe UI Symbol", 12, 2},
            {u"Small Fonts", 7, 4},
            {u"Segoe UI Symbol", 24, 2},
            {u"Malgun Gothic", 10, 2}
        }};
        container.configure_fonts(initial_fonts);

        // Initialize main menu window (660 x 560 native layout)
        skeet::ui::MenuWindow window;
        g_window = &window;
        skeet::ui::WindowCallbackStorage callbacks;
        skeet::ui::initialize_menu_window(window, callbacks, reinterpret_cast<std::uintptr_t>(hwnd), skeet::Point{660, 560});
        window.position = {(window_w - 660) / 2, (window_h - 560) / 2};

        skeet::ui::MainMenuAssets assets;
        skeet::ui::WidgetIdentities identities;
        skeet::Point next_tab_pos{6, 20};

        std::vector<std::unique_ptr<SimpleTab>> owned_tabs;
        std::vector<skeet::MenuTab> nav_tabs;

        auto make_tab = [&](std::u16string_view label) -> SimpleTab& {
            auto tab = std::make_unique<SimpleTab>(window.navigation, window.input, window.geometry, label, next_tab_pos, identities);
            auto* ptr = tab.get();
            ptr->identity = static_cast<std::uint32_t>(window.tabs.size());
            window.tabs.push_back(ptr);
            nav_tabs.push_back(skeet::MenuTab{ptr, reset_tab_callback, activate_tab_callback});
            owned_tabs.push_back(std::move(tab));
            return *ptr;
        };

        // Tab 0: RAGE
        {
            auto& tab = make_tab(u"RAGE");
            auto& left = tab.add_category(u"Aimbot", {0, 12, 0, 24}, identities);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Enabled", &g_cfg.rage_enabled, identities));
            left.elements.push_back(new skeet::ui::HotkeyWidget(&left, &g_cfg.rage_hotkey, true, identities));
            left.elements.push_back(new skeet::ui::Slider(&left, u"Minimum damage", 1, 100, &g_cfg.rage_min_damage, true, 0, 1.0f, identities));
            left.elements.push_back(new skeet::ui::Slider(&left, u"Hit chance", 0, 100, &g_cfg.rage_hitchance, true, 0, 1.0f, identities));

            auto* hb_combo = new skeet::ui::ComboBox(&left, u"Target hitbox", &g_cfg.rage_hitbox, false, identities);
            hb_combo->set_options({{u"Head", 0}, {u"Chest", 1}, {u"Pelvis", 2}, {u"All", 3}});
            left.elements.push_back(hb_combo);

            auto* mp_multi = new skeet::ui::MultiSelect(&left, u"Multipoint", &g_cfg.rage_multipoint, identities, false);
            mp_multi->add_option("Head", 1);
            mp_multi->add_option("Upper chest", 2);
            mp_multi->add_option("Lower chest", 4);
            mp_multi->add_option("Pelvis", 8);
            left.elements.push_back(mp_multi);

            left.elements.push_back(new skeet::ui::ColorPicker(&left, &g_cfg.rage_color, identities));

            auto& right = tab.add_category(u"Accuracy", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Automatic penetration", &g_cfg.rage_autowall, identities));
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Silent aim", &g_cfg.rage_silent, identities));
            right.elements.push_back(new skeet::ui::Slider(&right, u"Maximum FOV", 1, 180, &g_cfg.rage_fov, true, 0, 1.0f, identities));
        }

        // Tab 1: AA (Anti-Aim)
        {
            auto& tab = make_tab(u"AA");
            auto& left = tab.add_category(u"Anti-aim", {0, 12, 0, 24}, identities);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Enabled", &g_cfg.aa_enabled, identities));
            auto* pitch_combo = new skeet::ui::ComboBox(&left, u"Pitch", &g_cfg.aa_pitch, false, identities);
            pitch_combo->set_options({{u"Off", 0}, {u"Default", 1}, {u"Down", 2}, {u"Up", 3}});
            left.elements.push_back(pitch_combo);

            auto* yaw_combo = new skeet::ui::ComboBox(&left, u"Yaw", &g_cfg.aa_yaw, false, identities);
            yaw_combo->set_options({{u"Off", 0}, {u"180", 1}, {u"Spin", 2}, {u"Jitter", 3}});
            left.elements.push_back(yaw_combo);

            left.elements.push_back(new skeet::ui::Slider(&left, u"Yaw jitter", -180, 180, &g_cfg.aa_jitter, true, 0, 1.0f, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Freestanding", &g_cfg.aa_freestand, identities));

            auto& right = tab.add_category(u"Fake lag", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Enabled", &g_cfg.fakelag_enabled, identities));
            auto* lag_combo = new skeet::ui::ComboBox(&right, u"Type", &g_cfg.fakelag_type, false, identities);
            lag_combo->set_options({{u"Factor", 0}, {u"Adaptive", 1}, {u"Maximum", 2}});
            right.elements.push_back(lag_combo);
            right.elements.push_back(new skeet::ui::Slider(&right, u"Limit", 1, 16, &g_cfg.fakelag_limit, true, 0, 1.0f, identities));
        }

        // Tab 2: LEGIT
        {
            auto& tab = make_tab(u"LEGIT");
            auto& left = tab.add_category(u"Aimbot", {0, 12, 0, 24}, identities);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Enabled", &g_cfg.legit_enabled, identities));
            left.elements.push_back(new skeet::ui::Slider(&left, u"Speed", 1, 100, &g_cfg.legit_speed, true, 0, 1.0f, identities));
            left.elements.push_back(new skeet::ui::Slider(&left, u"FOV", 1, 30, &g_cfg.legit_fov, true, 0, 1.0f, identities));
            left.elements.push_back(new skeet::ui::Slider(&left, u"Recoil compensation", 0, 100, &g_cfg.legit_recoil, true, 0, 1.0f, identities));

            auto& right = tab.add_category(u"Triggerbot", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Enabled", &g_cfg.trigger_enabled, identities));
            right.elements.push_back(new skeet::ui::Slider(&right, u"Reaction time (ms)", 0, 300, &g_cfg.trigger_delay, true, 0, 1.0f, identities));
        }

        // Tab 3: VISUALS
        {
            auto& tab = make_tab(u"VISUALS");
            auto& left = tab.add_category(u"Player ESP", {0, 12, 0, 24}, identities);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Bounding box", &g_cfg.esp_box, identities));
            left.elements.push_back(new skeet::ui::ColorPicker(&left, &g_cfg.esp_box_color, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Player name", &g_cfg.esp_name, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Health bar", &g_cfg.esp_health, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Weapon icon", &g_cfg.esp_weapon, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Skeleton", &g_cfg.esp_skeleton, identities));

            auto& right = tab.add_category(u"Colored models", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Enabled", &g_cfg.chams_enabled, identities));
            auto* mat_combo = new skeet::ui::ComboBox(&right, u"Material", &g_cfg.chams_material, false, identities);
            mat_combo->set_options({{u"Visible", 0}, {u"Invisible", 1}, {u"Flat", 2}, {u"Metallic", 3}, {u"Glow", 4}});
            right.elements.push_back(mat_combo);
            right.elements.push_back(new skeet::ui::ColorPicker(&right, &g_cfg.chams_visible_color, identities));
            right.elements.push_back(new skeet::ui::ColorPicker(&right, &g_cfg.chams_invisible_color, identities));
        }

        // Tab 4: MISC
        {
            auto& tab = make_tab(u"MISC");
            auto& left = tab.add_category(u"Movement", {0, 12, 0, 24}, identities);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Bunny hop", &g_cfg.bhop_enabled, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Air strafe", &g_cfg.strafe_enabled, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Fast walk", &g_cfg.fastwalk_enabled, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Edge jump", &g_cfg.edgejump_enabled, identities));

            auto& right = tab.add_category(u"Settings", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Watermark", &g_cfg.watermark_enabled, identities));
            right.elements.push_back(new skeet::ui::Checkbox(&right, u"Rank reveal", &g_cfg.rankreveal_enabled, identities));
            right.elements.push_back(new skeet::ui::Textbox(&right, identities));
        }

        // Tab 5: SKINS
        {
            auto& tab = make_tab(u"SKINS");
            auto& left = tab.add_category(u"Model changer", {0, 12, 0, 24}, identities);
            auto* knife_combo = new skeet::ui::ComboBox(&left, u"Knife model", &g_cfg.knife_model, false, identities);
            knife_combo->set_options({{u"Default", 0}, {u"Karambit", 1}, {u"Butterfly", 2}, {u"M9 Bayonet", 3}, {u"Talon", 4}, {u"Skeleton", 5}});
            left.elements.push_back(knife_combo);

            auto* glove_combo = new skeet::ui::ComboBox(&left, u"Glove model", &g_cfg.glove_model, false, identities);
            glove_combo->set_options({{u"Default", 0}, {u"Sport", 1}, {u"Specialist", 2}, {u"Moto", 3}, {u"Driver", 4}, {u"Hand Wraps", 5}});
            left.elements.push_back(glove_combo);

            auto& right = tab.add_category(u"Overrides", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Slider(&right, u"StatTrak", 0, 9999, &g_cfg.skin_stattrak, true, 0, 1.0f, identities));
            right.elements.push_back(new skeet::ui::Slider(&right, u"Wear", 0, 100, &g_cfg.skin_wear, true, 0, 1.0f, identities));
            right.elements.push_back(new skeet::ui::Slider(&right, u"Seed", 1, 1000, &g_cfg.skin_seed, true, 0, 1.0f, identities));
        }

        // Tab 6: PLAYERS
        {
            auto& tab = make_tab(u"PLAYERS");
            auto& left = tab.add_category(u"Player list", {0, 12, 0, 24}, identities);
            auto* ply_combo = new skeet::ui::ComboBox(&left, u"Selected player", &g_cfg.selected_player, false, identities);
            ply_combo->set_options({{u"Player 1 (bot_1)", 0}, {u"Player 2 (bot_2)", 1}, {u"Player 3 (bot_3)", 2}, {u"Player 4 (bot_4)", 3}});
            left.elements.push_back(ply_combo);
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Override resolver", &g_cfg.player_resolver, identities));
            left.elements.push_back(new skeet::ui::Checkbox(&left, u"Force body aim", &g_cfg.player_baim, identities));

            auto& right = tab.add_category(u"Adjustments", {12, 12, 0, 24}, identities);
            right.elements.push_back(new skeet::ui::Slider(&right, u"Pitch correction", -89, 89, &g_cfg.player_pitch, true, 0, 1.0f, identities));
            right.elements.push_back(new skeet::ui::Slider(&right, u"Yaw offset", -180, 180, &g_cfg.player_yaw, true, 0, 1.0f, identities));
        }

        // Tab 7: CONFIG
        {
            auto& tab = make_tab(u"CONFIG");
            auto& left = tab.add_category(u"Presets", {0, 12, 0, 24}, identities);
            auto* preset_combo = new skeet::ui::ComboBox(&left, u"Preset", &g_cfg.selected_preset, false, identities);
            preset_combo->set_options({{u"Default", 0}, {u"HvH Aggressive", 1}, {u"HvH Scout", 2}, {u"Legit MM", 3}});
            left.elements.push_back(preset_combo);

            skeet::ui::WidgetCallback dummy_action{nullptr, [](void*, skeet::ui::Widget&){}};
            left.elements.push_back(new skeet::ui::Button(&left, u"Load preset", {158, 24}, dummy_action, identities));
            left.elements.push_back(new skeet::ui::Button(&left, u"Save preset", {158, 24}, dummy_action, identities));

            auto& right = tab.add_category(u"GUI Settings", {12, 12, 0, 24}, identities);
            auto* dpi_combo = new skeet::ui::ComboBox(&right, u"DPI scale", &g_cfg.dpi_scale, false, identities);
            dpi_combo->set_options({{u"100%", 0}, {u"125%", 1}, {u"150%", 2}, {u"175%", 3}, {u"200%", 4}});
            right.elements.push_back(dpi_combo);
            right.elements.push_back(new skeet::ui::ColorPicker(&right, &g_cfg.menu_accent, identities));
        }

        // Link navigation tabs and load tab icons
        window.navigation.tabs = nav_tabs;
        window.navigation.active_tab = 0;
        skeet::ui::initialize_main_menu_assets(window, container.backend(), assets);

        for (auto* t : window.tabs) {
            t->layout_categories();
            t->reflow_categories();
        }

        // Trigger opening fade transition
        window.open();

        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);

        // Application Render & Message Loop
        MSG msg{};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                continue;
            }

            if (g_device_lost) {
                HRESULT hr = g_device->TestCooperativeLevel();
                if (hr == D3DERR_DEVICELOST) {
                    Sleep(10);
                    continue;
                }
                if (hr == D3DERR_DEVICENOTRESET) {
                    container.release_context();
                    if (FAILED(g_device->Reset(&g_d3dpp))) {
                        Sleep(10);
                        continue;
                    }
                    container.configure_fonts(initial_fonts);
                    skeet::ui::initialize_main_menu_assets(window, container.backend(), assets);
                    g_device_lost = false;
                }
            }

            container.backend().screen_width = window_w;
            container.backend().screen_height = window_h;

            g_device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(20, 20, 22), 1.0f, 0);

            if (SUCCEEDED(g_device->BeginScene())) {
                auto* queue = container.begin();
                if (queue) {
                    window.update_transition(*queue);
                    window.draw(*queue, g_cfg.menu_accent);
                    container.publish();
                }
                container.draw();
                g_device->EndScene();
            }

            HRESULT hr = g_device->Present(nullptr, nullptr, nullptr, nullptr);
            if (hr == D3DERR_DEVICELOST) {
                g_device_lost = true;
            }

            Sleep(1);
        }

        callbacks.destroy(window);

        if (g_device) {
            g_device->Release();
            g_device = nullptr;
        }
        if (g_d3d) {
            g_d3d->Release();
            g_d3d = nullptr;
        }

        UnregisterClassW(window_class, instance);
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error Starting Standalone Menu", MB_ICONERROR);
        return 1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error starting standalone menu", "Error", MB_ICONERROR);
        return 1;
    }
}
