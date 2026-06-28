#include "menu.h"
#include "util/oled_fonts.h"
#include <stdio.h>

// External declarations of hardware components from Stutter.cpp
extern daisy::GPIO      rot_switch_pins[5];
extern daisy::DaisySeed hw;

// Internal helpers
static const char* GetSubdivString(int pos)
{
    switch(pos)
    {
        case 0: return "1/32";
        case 1: return "1/16";
        case 2: return "1/8 ";
        case 3: return "1/4 ";
        case 4: return "1/2 ";
        default: return "--- ";
    }
}

static void RenderItemRow(StutterDisplay& display,
                          int             y,
                          bool            cursor_here,
                          const char*     label, // exactly 10 chars
                          const char*     value, // exactly 5 chars
                          bool            value_inverted)
{
    char buf[19];
    char c_char = cursor_here ? '>' : ' ';
    snprintf(buf, sizeof(buf), "%c %-10s %5s", c_char, label, value);

    if(value_inverted)
    {
        char left_buf[14];
        snprintf(left_buf, sizeof(left_buf), "%c %-10s ", c_char, label);
        display.SetCursor(0, y);
        // Draw unselected label as white-on-black (true)
        display.WriteString(left_buf, Font_7x10, true);

        display.SetCursor(91, y);
        // Draw selected value box as black-on-white (false)
        display.WriteString(value, Font_7x10, false);
    }
    else
    {
        display.SetCursor(0, y);
        // If cursor_here is true, draw as black-on-white (false)
        // If cursor_here is false, draw as white-on-black (true)
        display.WriteString(buf, Font_7x10, !cursor_here);
    }
}

static void RenderStatusScreen(StutterDisplay&       display,
                               const PedalConfig*    cfg,
                               const StutterRuntime* rt)
{
    char buf[64];

    const char* state_tag = "IDLE";
    if(rt->state == STUTTER_RECORDING)
    {
        state_tag = "REC ";
    }
    else if(rt->state == STUTTER_PLAYING)
    {
        state_tag = "PLAY";
    }
    snprintf(buf, sizeof(buf), "STUTTER      %s", state_tag);
    display.SetCursor(0, 0);
    display.WriteString(buf, Font_7x10, true);

    if(cfg->midi_sync_enabled && !rt->has_clock)
    {
        snprintf(buf, sizeof(buf), "BPM: ---         ");
    }
    else
    {
        int bpm_whole = static_cast<int>(rt->bpm);
        int bpm_frac  = static_cast<int>((rt->bpm - bpm_whole) * 10.0f);
        if(bpm_frac < 0)
            bpm_frac = -bpm_frac;
        snprintf(buf, sizeof(buf), "BPM: %3d.%1d       ", bpm_whole, bpm_frac);
    }
    display.SetCursor(0, 10);
    display.WriteString(buf, Font_7x10, true);

    const char* subdiv_str = GetSubdivString(rt->subdiv_pos);
    const char* sync_mode  = "FREE";
    if(cfg->midi_sync_enabled)
    {
        sync_mode = rt->has_clock ? "SYNC" : "WAIT";
    }
    snprintf(buf, sizeof(buf), "SUBDIV: %-4s %4s", subdiv_str, sync_mode);
    display.SetCursor(0, 20);
    display.WriteString(buf, Font_7x10, true);

    int rate_whole = static_cast<int>(rt->rate);
    int rate_frac  = static_cast<int>((rt->rate - rate_whole) * 100.0f);
    if(rate_frac < 0)
        rate_frac = -rate_frac;
    snprintf(buf, sizeof(buf), "RATE: %d.%02d        ", rate_whole, rate_frac);
    display.SetCursor(0, 30);
    display.WriteString(buf, Font_7x10, true);

    int wet_whole = static_cast<int>(rt->wet);
    int wet_frac  = static_cast<int>((rt->wet - wet_whole) * 100.0f);
    if(wet_frac < 0)
        wet_frac = -wet_frac;
    snprintf(buf, sizeof(buf), "WET:  %d.%02d        ", wet_whole, wet_frac);
    display.SetCursor(0, 40);
    display.WriteString(buf, Font_7x10, true);

    display.SetCursor(0, 50);
    display.WriteString("[>]=MENU          ", Font_7x10, true);
}

static void RenderBrowseScreen(StutterDisplay&    display,
                               const MenuContext* ctx,
                               const PedalConfig* cfg)
{
    display.SetCursor(0, 0);
    display.WriteString("-- SETTINGS --    ", Font_7x10, true);

    RenderItemRow(display,
                  10,
                  ctx->cursor == MENU_ITEM_MIDI_SYNC,
                  "MIDI SYNC ",
                  cfg->midi_sync_enabled ? "   ON" : "  OFF",
                  false);
    RenderItemRow(display,
                  20,
                  ctx->cursor == MENU_ITEM_QUANTIZE_TRIGGER,
                  "QUANTIZE  ",
                  cfg->quantize_trigger ? "   ON" : "  OFF",
                  false);
    RenderItemRow(display,
                  30,
                  ctx->cursor == MENU_ITEM_DEBUG,
                  "DEBUG     ",
                  "ENTER",
                  false);

    display.SetCursor(0, 40);
    display.WriteString("                  ", Font_7x10, true);
    display.SetCursor(0, 50);
    display.WriteString("[>]=SEL [H]=BACK  ", Font_7x10, true);
}

static void RenderEditScreen(StutterDisplay&    display,
                             const MenuContext* ctx,
                             const PedalConfig* cfg)
{
    display.SetCursor(0, 0);
    display.WriteString("-- EDITING --     ", Font_7x10, true);

    RenderItemRow(display,
                  10,
                  ctx->cursor == MENU_ITEM_MIDI_SYNC,
                  "MIDI SYNC ",
                  cfg->midi_sync_enabled ? "   ON" : "  OFF",
                  ctx->cursor == MENU_ITEM_MIDI_SYNC);
    RenderItemRow(display,
                  20,
                  ctx->cursor == MENU_ITEM_QUANTIZE_TRIGGER,
                  "QUANTIZE  ",
                  cfg->quantize_trigger ? "   ON" : "  OFF",
                  ctx->cursor == MENU_ITEM_QUANTIZE_TRIGGER);
    RenderItemRow(display,
                  30,
                  ctx->cursor == MENU_ITEM_DEBUG,
                  "DEBUG     ",
                  "ENTER",
                  ctx->cursor == MENU_ITEM_DEBUG);

    display.SetCursor(0, 40);
    display.WriteString("                  ", Font_7x10, true);
    display.SetCursor(0, 50);
    display.WriteString("[>]=OK [H]=CNCL   ", Font_7x10, true);
}

static void RenderDebugScreen(StutterDisplay&       display,
                              const MenuContext*    ctx,
                              const PedalConfig*    cfg,
                              const StutterRuntime* rt)
{
    char buf[64];

    display.SetCursor(0, 0);
    display.WriteString("-- DEBUG --       ", Font_7x10, true);

    // Pot is inverted in software so we display the inverted value
    float wet_dry_val = 1.0f - hw.adc.GetFloat(0); // 0 is ADC_WET_DRY_POT
    int   pot_whole   = static_cast<int>(wet_dry_val);
    int   pot_frac    = static_cast<int>((wet_dry_val - pot_whole) * 100.0f);
    if(pot_frac < 0)
        pot_frac = -pot_frac;
    snprintf(buf, sizeof(buf), "POT (A0): %d.%02d    ", pot_whole, pot_frac);
    display.SetCursor(0, 10);
    display.WriteString(buf, Font_7x10, true);

    snprintf(buf,
             sizeof(buf),
             "MENU A:%d B:%d SW:%d ",
             menu_pin_a.Read(),
             menu_pin_b.Read(),
             menu_pin_sw.Read());
    display.SetCursor(0, 20);
    display.WriteString(buf, Font_7x10, true);

    snprintf(buf,
             sizeof(buf),
             "RATE A:%d B:%d SW:%d ",
             rate_pin_a.Read(),
             rate_pin_b.Read(),
             rate_pin_sw.Read());
    display.SetCursor(0, 30);
    display.WriteString(buf, Font_7x10, true);

    int rot_pos = -1;
    for(int i = 0; i < 5; i++)
    {
        if(!rot_switch_pins[i].Read())
        {
            rot_pos = i;
            break;
        }
    }
    snprintf(buf, sizeof(buf), "ROTARY: %d         ", rot_pos);
    display.SetCursor(0, 40);
    display.WriteString(buf, Font_7x10, true);

    display.SetCursor(0, 50);
    display.WriteString("[>]=BACK          ", Font_7x10, true);
}

// ── Public API Implementation ──────────────────────────────────────────────────

void MenuInit(MenuContext* ctx)
{
    ctx->state         = MENU_STATE_STATUS;
    ctx->cursor        = 0;
    ctx->idle_timer_ms = 0;
    ctx->dirty         = false;
    ctx->needs_redraw  = true;
}

void MenuTick(MenuContext* ctx, PedalConfig* cfg, uint32_t elapsed_ms)
{
    if(ctx->state == MENU_STATE_BROWSE)
    {
        ctx->idle_timer_ms += elapsed_ms;
        if(ctx->idle_timer_ms >= 5000)
        {
            ctx->state        = MENU_STATE_STATUS;
            ctx->needs_redraw = true;
        }
    }
}

void MenuHandleRotate(MenuContext* ctx, PedalConfig* cfg, int delta)
{
    ctx->idle_timer_ms = 0;
    if(ctx->state == MENU_STATE_BROWSE)
    {
        ctx->cursor += delta;
        if(ctx->cursor < 0)
        {
            ctx->cursor = 0;
        }
        else if(ctx->cursor >= MENU_ITEM_COUNT)
        {
            ctx->cursor = MENU_ITEM_COUNT - 1;
        }
        ctx->needs_redraw = true;
    }
    else if(ctx->state == MENU_STATE_EDIT)
    {
        if(ctx->cursor == MENU_ITEM_MIDI_SYNC)
        {
            cfg->midi_sync_enabled = !cfg->midi_sync_enabled;
        }
        else if(ctx->cursor == MENU_ITEM_QUANTIZE_TRIGGER)
        {
            cfg->quantize_trigger = !cfg->quantize_trigger;
        }
        ctx->needs_redraw = true;
    }
}

void MenuHandleShortPress(MenuContext* ctx, PedalConfig* cfg)
{
    ctx->idle_timer_ms = 0;
    switch(ctx->state)
    {
        case MENU_STATE_STATUS:
            ctx->state        = MENU_STATE_BROWSE;
            ctx->needs_redraw = true;
            break;
        case MENU_STATE_BROWSE:
            if(ctx->cursor == MENU_ITEM_DEBUG)
            {
                ctx->state = MENU_STATE_DEBUG;
            }
            else
            {
                ctx->edit_shadow = *cfg;
                ctx->state       = MENU_STATE_EDIT;
            }
            ctx->needs_redraw = true;
            break;
        case MENU_STATE_EDIT:
            ctx->state        = MENU_STATE_BROWSE;
            ctx->dirty        = true;
            ctx->needs_redraw = true;
            break;
        case MENU_STATE_DEBUG:
            ctx->state        = MENU_STATE_BROWSE;
            ctx->needs_redraw = true;
            break;
    }
}

void MenuHandleLongPress(MenuContext* ctx, PedalConfig* cfg)
{
    ctx->idle_timer_ms = 0;
    switch(ctx->state)
    {
        case MENU_STATE_STATUS: break;
        case MENU_STATE_BROWSE:
            ctx->state        = MENU_STATE_STATUS;
            ctx->needs_redraw = true;
            break;
        case MENU_STATE_EDIT:
            *cfg              = ctx->edit_shadow;
            ctx->state        = MENU_STATE_BROWSE;
            ctx->needs_redraw = true;
            break;
        case MENU_STATE_DEBUG:
            ctx->state        = MENU_STATE_BROWSE;
            ctx->needs_redraw = true;
            break;
    }
}

void MenuRender(StutterDisplay&       display,
                const MenuContext*    ctx,
                const PedalConfig*    cfg,
                const StutterRuntime* rt)
{
    display.Fill(false);

    switch(ctx->state)
    {
        case MENU_STATE_STATUS: RenderStatusScreen(display, cfg, rt); break;
        case MENU_STATE_BROWSE: RenderBrowseScreen(display, ctx, cfg); break;
        case MENU_STATE_EDIT: RenderEditScreen(display, ctx, cfg); break;
        case MENU_STATE_DEBUG: RenderDebugScreen(display, ctx, cfg, rt); break;
    }

    display.Update();
}
