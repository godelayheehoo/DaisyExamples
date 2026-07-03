#define MENU_DEBUG


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
        // Round to nearest integer BPM to minimize draw calls on jitter
        int bpm_rounded = static_cast<int>(rt->bpm + 0.5f);
        snprintf(buf, sizeof(buf), "BPM: %3d           ", bpm_rounded);
    }
    display.SetCursor(0, 10);
    display.WriteString(buf, Font_7x10, true);

    const char* subdiv_str = GetSubdivString(rt->subdiv_pos);
    const char* sync_mode  = "FREE";
    if(cfg->midi_sync_enabled)
    {
        if(rt->has_clock)
        {
            sync_mode = rt->midi_play_seen ? "SYNC" : "CLK ";
        }
        else
        {
            sync_mode = "WAIT";
        }
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
    display.WriteString("[>]=SEL [B]=BACK  ", Font_7x10, true);
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
    display.WriteString("[>]=OK [B]=CNCL   ", Font_7x10, true);
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
    snprintf(buf,
             sizeof(buf),
             "POT:%d.%02d MID:%-5lu",
             pot_whole,
             pot_frac,
             rt->midi_event_count % 100000);
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
    snprintf(buf,
             sizeof(buf),
             "ROT:%d BAK:%d CON:%d ",
             rot_pos,
             menu_pin_bak.Read(),
             menu_pin_con.Read());
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


void MenuHandleBackPress(MenuContext* ctx, PedalConfig* cfg)
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
    // Determine if we actually need to redraw
    bool should_redraw = ctx->needs_redraw;

    if(ctx->state == MENU_STATE_STATUS)
    {
        static StutterState last_state       = STUTTER_IDLE;
        static float        last_rate        = -1.0f;
        static float        last_target_rate = -1.0f;
        static float        last_wet         = -1.0f;
        static int          last_bpm_rounded = -1;
        static bool         last_has_clock   = false;
        static int          last_subdiv      = -1;
        static bool         last_play_seen   = false;

        float rate_diff = rt->rate - last_rate;
        if(rate_diff < 0)
            rate_diff = -rate_diff;
        float target_rate_diff = rt->target_rate - last_target_rate;
        if(target_rate_diff < 0)
            target_rate_diff = -target_rate_diff;
        float wet_diff = rt->wet - last_wet;
        if(wet_diff < 0)
            wet_diff = -wet_diff;
        int cur_bpm_rounded = static_cast<int>(rt->bpm + 0.5f);

        if(rt->state != last_state || rate_diff > 0.01f
           || target_rate_diff > 0.01f || wet_diff > 0.01f
           || cur_bpm_rounded != last_bpm_rounded
           || rt->has_clock != last_has_clock || rt->subdiv_pos != last_subdiv
           || rt->midi_play_seen != last_play_seen || ctx->needs_redraw)
        {
            should_redraw    = true;
            last_state       = rt->state;
            last_rate        = rt->rate;
            last_target_rate = rt->target_rate;
            last_wet         = rt->wet;
            last_bpm_rounded = cur_bpm_rounded;
            last_has_clock   = rt->has_clock;
            last_subdiv      = rt->subdiv_pos;
            last_play_seen   = rt->midi_play_seen;
        }
        else
        {
            should_redraw = false;
        }
    }
    else if(ctx->state == MENU_STATE_DEBUG)
    {
        static float    last_wet_dry = -1.0f;
        static int      last_menu_a = -1, last_menu_b = -1, last_menu_sw = -1;
        static int      last_rate_a = -1, last_rate_b = -1, last_rate_sw = -1;
        static int      last_rot = -1;
        static int      last_bak = -1, last_con = -1;
        static uint32_t last_midi_count = 0;

        float    wet_dry_val = 1.0f - hw.adc.GetFloat(0);
        int      menu_a      = menu_pin_a.Read();
        int      menu_b      = menu_pin_b.Read();
        int      menu_sw     = menu_pin_sw.Read();
        int      rate_a      = rate_pin_a.Read();
        int      rate_b      = rate_pin_b.Read();
        int      rate_sw     = rate_pin_sw.Read();
        int      bak_val     = menu_pin_bak.Read();
        int      con_val     = menu_pin_con.Read();
        uint32_t midi_count  = rt->midi_event_count;

        int rot_pos = -1;
        for(int i = 0; i < 5; i++)
        {
            if(!rot_switch_pins[i].Read())
            {
                rot_pos = i;
                break;
            }
        }

        float diff = wet_dry_val - last_wet_dry;
        if(diff < 0)
            diff = -diff;

        if(diff > 0.01f || menu_a != last_menu_a || menu_b != last_menu_b
           || menu_sw != last_menu_sw || rate_a != last_rate_a
           || rate_b != last_rate_b || rate_sw != last_rate_sw
           || rot_pos != last_rot || bak_val != last_bak || con_val != last_con
           || midi_count != last_midi_count || ctx->needs_redraw)
        {
            should_redraw   = true;
            last_wet_dry    = wet_dry_val;
            last_menu_a     = menu_a;
            last_menu_b     = menu_b;
            last_menu_sw    = menu_sw;
            last_rate_a     = rate_a;
            last_rate_b     = rate_b;
            last_rate_sw    = rate_sw;
            last_rot        = rot_pos;
            last_bak        = bak_val;
            last_con        = con_val;
            last_midi_count = midi_count;
        }
        else
        {
            should_redraw = false;
        }
    }

    if(!should_redraw)
    {
        return;
    }

    // --- OLED RATE LIMITING ---
    // Cap OLED updates to prevent I2C controller lockup on SSD1309.
    // We allow a slightly faster cadence for menus (10Hz) to keep scrolling responsive,
    // and a slower cadence (5Hz) for the Status screen.
    uint32_t oled_period = (ctx->state == MENU_STATE_STATUS) ? 200 : 100;

    static uint32_t last_oled = 0;
    if(daisy::System::GetNow() - last_oled < oled_period)
    {
        // Skip drawing if it hasn't been long enough.
        // We don't reset needs_redraw so it will try again next tick.
        return;
    }
    last_oled = daisy::System::GetNow();
    // ------------------------------------------------------

    display.Fill(false);

    switch(ctx->state)
    {
        case MENU_STATE_STATUS: RenderStatusScreen(display, cfg, rt); break;
        case MENU_STATE_BROWSE: RenderBrowseScreen(display, ctx, cfg); break;
        case MENU_STATE_EDIT: RenderEditScreen(display, ctx, cfg); break;
        case MENU_STATE_DEBUG: RenderDebugScreen(display, ctx, cfg, rt); break;
    }

#ifdef MENU_DEBUG
    uint32_t start_ms = daisy::System::GetNow();

    hw.PrintLine("OLED Update Start: %d ms", start_ms);
#endif
    display.Update();
#ifdef MENU_DEBUG
    uint32_t end_ms = daisy::System::GetNow();
    hw.PrintLine(
        "OLED Update Stop: %d ms (took %d ms)", end_ms, end_ms - start_ms);
#endif

    // Reset needs_redraw flag (cast away const safely)
    const_cast<MenuContext*>(ctx)->needs_redraw = false;
}
