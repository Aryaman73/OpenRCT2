/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/Editor.h>
#include <openrct2/Game.h>
#include <openrct2/Input.h>
#include <openrct2/ParkImporter.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/actions/LoadOrQuitAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/sprites.h>
#include <openrct2/ui/UiContext.h>

// clang-format off
enum {
    WIDX_START_NEW_GAME,
    WIDX_CONTINUE_SAVED_GAME,
    WIDX_MULTIPLAYER,
    WIDX_GAME_TOOLS,
    WIDX_NEW_VERSION,
};

static ScreenRect _filterRect;
static constexpr ScreenSize MenuButtonDims = { 82, 82 };
static constexpr ScreenSize UpdateButtonDims = { MenuButtonDims.width * 4, 28 };

static rct_widget window_title_menu_widgets[] = {
    MakeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WindowWidgetType::ImgBtn, WindowColour::Tertiary,  SPR_MENU_NEW_GAME,       STR_START_NEW_GAME_TIP),
    MakeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WindowWidgetType::ImgBtn, WindowColour::Tertiary,  SPR_MENU_LOAD_GAME,      STR_CONTINUE_SAVED_GAME_TIP),
    MakeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WindowWidgetType::ImgBtn, WindowColour::Tertiary,  SPR_G2_MENU_MULTIPLAYER, STR_SHOW_MULTIPLAYER_TIP),
    MakeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WindowWidgetType::ImgBtn, WindowColour::Tertiary,  SPR_MENU_TOOLBOX,        STR_GAME_TOOLS_TIP),
    MakeWidget({0,                       0}, UpdateButtonDims, WindowWidgetType::Empty,  WindowColour::Secondary, STR_UPDATE_AVAILABLE),
    WIDGETS_END,
};

static void window_title_menu_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_title_menu_mousedown(rct_window *w, rct_widgetindex widgetIndex, rct_widget* widget);
static void window_title_menu_dropdown(rct_window *w, rct_widgetindex widgetIndex, int32_t dropdownIndex);
static void window_title_menu_cursor(rct_window *w, rct_widgetindex widgetIndex, const ScreenCoordsXY& screenCoords, CursorID *cursorId);
static void window_title_menu_invalidate(rct_window *w);
static void window_title_menu_paint(rct_window *w, rct_drawpixelinfo *dpi);

static rct_window_event_list window_title_menu_events([](auto& events)
{
    events.mouse_up = &window_title_menu_mouseup;
    events.mouse_down = &window_title_menu_mousedown;
    events.dropdown = &window_title_menu_dropdown;
    events.cursor = &window_title_menu_cursor;
    events.invalidate = &window_title_menu_invalidate;
    events.paint = &window_title_menu_paint;
});
// clang-format on

/**
 * Creates the window containing the menu buttons on the title screen.
 *  rct2: 0x0066B5C0 (part of 0x0066B3E8)
 */
rct_window* window_title_menu_open()
{
    rct_window* window;

    const uint16_t windowHeight = MenuButtonDims.height + UpdateButtonDims.height;
    window = WindowCreate(
        ScreenCoordsXY(0, context_get_height() - 182), 0, windowHeight, &window_title_menu_events, WC_TITLE_MENU,
        WF_STICK_TO_BACK | WF_TRANSPARENT | WF_NO_BACKGROUND);

    window->widgets = window_title_menu_widgets;
    window->enabled_widgets
        = ((1ULL << WIDX_START_NEW_GAME) | (1ULL << WIDX_CONTINUE_SAVED_GAME) |
#ifndef DISABLE_NETWORK
           (1ULL << WIDX_MULTIPLAYER) |
#endif
           (1ULL << WIDX_GAME_TOOLS));

    rct_widgetindex i = 0;
    int32_t x = 0;
    for (rct_widget* widget = window->widgets; widget != &window->widgets[WIDX_NEW_VERSION]; widget++)
    {
        if (WidgetIsEnabled(window, i))
        {
            widget->left = x;
            widget->right = x + MenuButtonDims.width - 1;

            x += MenuButtonDims.width;
        }
        else
        {
            widget->type = WindowWidgetType::Empty;
        }
        i++;
    }
    window->width = x;
    window->widgets[WIDX_NEW_VERSION].right = window->width;
    window->windowPos.x = (context_get_width() - window->width) / 2;
    window->colours[1] = TRANSLUCENT(COLOUR_LIGHT_ORANGE);

    WindowInitScrollWidgets(window);

    return window;
}

static void window_title_menu_scenarioselect_callback(const utf8* path)
{
    context_load_park_from_file(path);
    game_load_scripts();
}

static void window_title_menu_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    rct_window* windowToOpen = nullptr;

    switch (widgetIndex)
    {
        case WIDX_START_NEW_GAME:
            windowToOpen = window_find_by_class(WC_SCENARIO_SELECT);
            if (windowToOpen != nullptr)
            {
                window_bring_to_front(windowToOpen);
            }
            else
            {
                window_close_by_class(WC_LOADSAVE);
                window_close_by_class(WC_SERVER_LIST);
                window_scenarioselect_open(window_title_menu_scenarioselect_callback, false);
            }
            break;
        case WIDX_CONTINUE_SAVED_GAME:
            windowToOpen = window_find_by_class(WC_LOADSAVE);
            if (windowToOpen != nullptr)
            {
                window_bring_to_front(windowToOpen);
            }
            else
            {
                window_close_by_class(WC_SCENARIO_SELECT);
                window_close_by_class(WC_SERVER_LIST);
                auto loadOrQuitAction = LoadOrQuitAction(LoadOrQuitModes::OpenSavePrompt);
                GameActions::Execute(&loadOrQuitAction);
            }
            break;
        case WIDX_MULTIPLAYER:
            windowToOpen = window_find_by_class(WC_SERVER_LIST);
            if (windowToOpen != nullptr)
            {
                window_bring_to_front(windowToOpen);
            }
            else
            {
                window_close_by_class(WC_SCENARIO_SELECT);
                window_close_by_class(WC_LOADSAVE);
                context_open_window(WC_SERVER_LIST);
            }
            break;
        case WIDX_NEW_VERSION:
            context_open_window_view(WV_NEW_VERSION_INFO);
            break;
    }
}

static void window_title_menu_mousedown(rct_window* w, rct_widgetindex widgetIndex, rct_widget* widget)
{
    if (widgetIndex == WIDX_GAME_TOOLS)
    {
        gDropdownItemsFormat[0] = STR_SCENARIO_EDITOR;
        gDropdownItemsFormat[1] = STR_CONVERT_SAVED_GAME_TO_SCENARIO;
        gDropdownItemsFormat[2] = STR_ROLLER_COASTER_DESIGNER;
        gDropdownItemsFormat[3] = STR_TRACK_DESIGNS_MANAGER;
        gDropdownItemsFormat[4] = STR_OPEN_USER_CONTENT_FOLDER;
        WindowDropdownShowText(
            { w->windowPos.x + widget->left, w->windowPos.y + widget->top }, widget->height() + 1, TRANSLUCENT(w->colours[0]),
            Dropdown::Flag::StayOpen, 5);
    }
}

static void window_title_menu_dropdown(rct_window* w, rct_widgetindex widgetIndex, int32_t dropdownIndex)
{
    if (widgetIndex == WIDX_GAME_TOOLS)
    {
        switch (dropdownIndex)
        {
            case 0:
                Editor::Load();
                break;
            case 1:
                Editor::ConvertSaveToScenario();
                break;
            case 2:
                Editor::LoadTrackDesigner();
                break;
            case 3:
                Editor::LoadTrackManager();
                break;
            case 4:
            {
                auto context = OpenRCT2::GetContext();
                auto env = context->GetPlatformEnvironment();
                auto uiContext = context->GetUiContext();
                uiContext->OpenFolder(env->GetDirectoryPath(OpenRCT2::DIRBASE::USER));
                break;
            }
        }
    }
}

static void window_title_menu_cursor(
    rct_window* w, rct_widgetindex widgetIndex, const ScreenCoordsXY& screenCoords, CursorID* cursorId)
{
    gTooltipTimeout = 2000;
}

static void window_title_menu_invalidate(rct_window* w)
{
    _filterRect = { w->windowPos.x, w->windowPos.y + UpdateButtonDims.height, w->windowPos.x + w->width - 1,
                    w->windowPos.y + MenuButtonDims.height + UpdateButtonDims.height - 1 };
    if (OpenRCT2::GetContext()->HasNewVersionInfo())
    {
        w->enabled_widgets |= (1ULL << WIDX_NEW_VERSION);
        w->widgets[WIDX_NEW_VERSION].type = WindowWidgetType::Button;
        _filterRect.Point1.y = w->windowPos.y;
    }
}

static void window_title_menu_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    gfx_filter_rect(dpi, _filterRect, FilterPaletteID::Palette51);
    WindowDrawWidgets(w, dpi);
}
