import os
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, KeepTogether, PageBreak, HRFlowable
)
from reportlab.pdfgen import canvas

# ----------------------------------------------------
# 1. GENERATE DIAGRAMS WITH MATPLOTLIB
# ----------------------------------------------------
def create_front_panel_diagram():
    fig, ax = plt.subplots(figsize=(7, 4.5), dpi=300)
    ax.set_facecolor('#1E1E2E')
    fig.patch.set_facecolor('#1E1E2E')

    # Main pedal box contour
    box = patches.FancyBboxPatch((0.5, 0.5), 9, 5.5, boxstyle="round,pad=0.3",
                                fc='#2D2B42', ec='#7F56D9', lw=3)
    ax.add_patch(box)

    # Title on pedal
    ax.text(5, 5.5, "STUTTER UNIT", color='#06B6D4', fontsize=16, fontweight='bold', ha='center')
    ax.text(5, 5.1, "DAISY SEED AUDIO ENGINE", color='#A5F3FC', fontsize=8, ha='center', fontstyle='italic')

    # 1. Rate Encoder
    c1 = patches.Circle((2, 3.8), 0.7, fc='#3B3759', ec='#7F56D9', lw=2)
    ax.add_patch(c1)
    ax.text(2, 3.8, "RATE\n[HOLD: REC]", color='white', fontsize=8, fontweight='bold', ha='center', va='center')
    ax.text(2, 2.8, "Rate / Stutter Encoder", color='#D1D5DB', fontsize=8, ha='center')

    # 2. OLED Screen
    oled = patches.Rectangle((3.8, 3.2), 2.4, 1.3, fc='#000000', ec='#06B6D4', lw=2)
    ax.add_patch(oled)
    ax.text(5, 4.1, "STATUS: PLAY", color='#38BDF8', fontsize=7, fontweight='bold', ha='center')
    ax.text(5, 3.7, "RATE: 1.00x  [1/8]", color='#F43F5E', fontsize=7, ha='center')
    ax.text(5, 3.4, "CLK: 120.0 BPM", color='#10B981', fontsize=7, ha='center')
    ax.text(5, 2.8, "OLED Screen (128x64)", color='#D1D5DB', fontsize=8, ha='center')

    # 3. Menu Encoder
    c2 = patches.Circle((8, 3.8), 0.7, fc='#3B3759', ec='#7F56D9', lw=2)
    ax.add_patch(c2)
    ax.text(8, 3.8, "MENU\n[PRESS]", color='white', fontsize=8, fontweight='bold', ha='center', va='center')
    ax.text(8, 2.8, "Menu Encoder", color='#D1D5DB', fontsize=8, ha='center')

    # Bottom Row Controls
    # 4. Rotary Switch
    c3 = patches.Circle((2, 1.6), 0.6, fc='#3B3759', ec='#F59E0B', lw=2)
    ax.add_patch(c3)
    ax.text(2, 1.6, "1/32..1/2", color='white', fontsize=7, fontweight='bold', ha='center', va='center')
    ax.text(2, 0.8, "Subdivision Switch", color='#D1D5DB', fontsize=8, ha='center')

    # 5. Wet/Dry Pot
    c4 = patches.Circle((4, 1.6), 0.6, fc='#3B3759', ec='#10B981', lw=2)
    ax.add_patch(c4)
    ax.text(4, 1.6, "WET/DRY", color='white', fontsize=7, fontweight='bold', ha='center', va='center')
    ax.text(4, 0.8, "Mix Potentiometer", color='#D1D5DB', fontsize=8, ha='center')

    # 6. CON Button & BAK Button
    btn1 = patches.Circle((6, 1.8), 0.3, fc='#10B981', ec='white', lw=1.5)
    btn2 = patches.Circle((6, 1.2), 0.3, fc='#EF4444', ec='white', lw=1.5)
    ax.add_patch(btn1)
    ax.add_patch(btn2)
    ax.text(6.5, 1.8, "CON (Confirm)", color='#D1D5DB', fontsize=8, va='center')
    ax.text(6.5, 1.2, "BAK (Back)", color='#D1D5DB', fontsize=8, va='center')

    # 7. Stutter LED
    led = patches.Circle((8, 1.6), 0.3, fc='#F43F5E', ec='#FB7185', lw=2)
    ax.add_patch(led)
    ax.text(8, 0.8, "Stutter LED", color='#D1D5DB', fontsize=8, ha='center')

    ax.set_xlim(0, 10)
    ax.set_ylim(0, 6)
    ax.axis('off')

    plt.tight_layout()
    img_path = 'diagram_front_panel.png'
    plt.savefig(img_path, bbox_inches='tight', facecolor=fig.get_facecolor(), edgecolor='none')
    plt.close()
    return img_path


def create_signal_flow_diagram():
    fig, ax = plt.subplots(figsize=(7, 2.2), dpi=300)
    ax.set_facecolor('#F8FAFC')
    fig.patch.set_facecolor('#F8FAFC')

    # Blocks
    box_style = "round,pad=0.2"
    
    # In
    ax.add_patch(patches.FancyBboxPatch((0.2, 0.6), 1.2, 0.8, boxstyle=box_style, fc='#E2E8F0', ec='#64748B', lw=1.5))
    ax.text(0.8, 1.0, "Stereo In\n(Line Level)", color='#1E293B', fontsize=8, fontweight='bold', ha='center', va='center')

    # Arrow 1
    ax.annotate('', xy=(2.0, 1.0), xytext=(1.4, 1.0), arrowprops=dict(arrowstyle="->", lw=1.5, color='#475569'))

    # Buffer Rec
    ax.add_patch(patches.FancyBboxPatch((2.0, 0.6), 1.8, 0.8, boxstyle=box_style, fc='#DDD6FE', ec='#7C3AED', lw=1.5))
    ax.text(2.9, 1.0, "SDRAM Buffer\n(Capture Loop)", color='#4C1D95', fontsize=8, fontweight='bold', ha='center', va='center')

    # Arrow 2
    ax.annotate('', xy=(4.4, 1.0), xytext=(3.8, 1.0), arrowprops=dict(arrowstyle="->", lw=1.5, color='#475569'))

    # Pitch / Stutter Engine
    ax.add_patch(patches.FancyBboxPatch((4.4, 0.6), 1.8, 0.8, boxstyle=box_style, fc='#CFFAFE', ec='#0891B2', lw=1.5))
    ax.text(5.3, 1.0, "DSP Resampler\n(Rate: 0.25x-4x)", color='#164E63', fontsize=8, fontweight='bold', ha='center', va='center')

    # Arrow 3
    ax.annotate('', xy=(6.8, 1.0), xytext=(6.2, 1.0), arrowprops=dict(arrowstyle="->", lw=1.5, color='#475569'))

    # Wet/Dry Mixer
    ax.add_patch(patches.FancyBboxPatch((6.8, 0.6), 1.6, 0.8, boxstyle=box_style, fc='#D1FAE5', ec='#059669', lw=1.5))
    ax.text(7.6, 1.0, "Analog Mix\n(Wet / Dry)", color='#065F46', fontsize=8, fontweight='bold', ha='center', va='center')

    # Arrow 4
    ax.annotate('', xy=(9.0, 1.0), xytext=(8.4, 1.0), arrowprops=dict(arrowstyle="->", lw=1.5, color='#475569'))

    # Out
    ax.add_patch(patches.FancyBboxPatch((9.0, 0.6), 1.2, 0.8, boxstyle=box_style, fc='#E2E8F0', ec='#64748B', lw=1.5))
    ax.text(9.6, 1.0, "Stereo Out\n(To Amp/Mixer)", color='#1E293B', fontsize=8, fontweight='bold', ha='center', va='center')

    ax.set_xlim(0, 10.4)
    ax.set_ylim(0.2, 1.8)
    ax.axis('off')

    plt.tight_layout()
    img_path = 'diagram_signal_flow.png'
    plt.savefig(img_path, bbox_inches='tight', facecolor=fig.get_facecolor(), edgecolor='none')
    plt.close()
    return img_path

# Generate PNGs
img_panel = create_front_panel_diagram()
img_flow = create_signal_flow_diagram()

# ----------------------------------------------------
# 2. REPORTLAB NUMBERED CANVAS FOR HEADERS & FOOTERS
# ----------------------------------------------------
class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super(NumberedCanvas, self).__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_header_footer(num_pages)
            canvas.Canvas.showPage(self)
        canvas.Canvas.save(self)

    def draw_header_footer(self, page_count):
        self.saveState()
        self.setFont("Helvetica-Bold", 8)
        self.setFillColor(colors.HexColor("#64748B"))
        
        # Suppress header on page 1 (Cover / Intro top)
        if self._pageNumber > 1:
            self.drawString(54, 750, "STUTTER UNIT — USER MANUAL & OPERATION GUIDE")
            self.setStrokeColor(colors.HexColor("#E2E8F0"))
            self.setLineWidth(0.75)
            self.line(54, 742, 558, 742)

        # Footer on all pages
        self.setFont("Helvetica", 8)
        self.drawString(54, 36, "Stutter Unit • Daisy Seed Stereo DSP Engine")
        page_str = f"Page {self._pageNumber} of {page_count}"
        self.drawRightString(558, 36, page_str)
        self.setStrokeColor(colors.HexColor("#E2E8F0"))
        self.setLineWidth(0.75)
        self.line(54, 48, 558, 48)

        self.restoreState()

# ----------------------------------------------------
# 3. BUILD PDF DOCUMENT
# ----------------------------------------------------
def build_pdf(filename="Stutter_Unit_User_Manual.pdf"):
    doc = SimpleDocTemplate(
        filename,
        pagesize=letter,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=54
    )

    styles = getSampleStyleSheet()

    # Custom Palette
    PRIMARY = colors.HexColor("#1E1E2E")
    PURPLE_ACCENT = colors.HexColor("#6D28D9")
    CYAN_ACCENT = colors.HexColor("#0284C7")
    DARK_TEXT = colors.HexColor("#1F2937")
    MUTED_TEXT = colors.HexColor("#4B5563")

    # Typography styles
    style_cover_title = ParagraphStyle(
        'CoverTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=24,
        leading=28,
        textColor=PURPLE_ACCENT,
        alignment=0,
        spaceAfter=2
    )

    style_cover_subtitle = ParagraphStyle(
        'CoverSubtitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=11,
        leading=15,
        textColor=CYAN_ACCENT,
        alignment=0,
        spaceAfter=10
    )

    style_h1 = ParagraphStyle(
        'Heading1_Custom',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=13.5,
        leading=17,
        textColor=PURPLE_ACCENT,
        spaceBefore=12,
        spaceAfter=6,
        keepWithNext=True
    )

    style_h2 = ParagraphStyle(
        'Heading2_Custom',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=10.5,
        leading=14,
        textColor=DARK_TEXT,
        spaceBefore=8,
        spaceAfter=4,
        keepWithNext=True
    )

    style_body = ParagraphStyle(
        'Body_Custom',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9,
        leading=13,
        textColor=DARK_TEXT,
        spaceAfter=6
    )

    style_table_header = ParagraphStyle(
        'TableHeader_Custom',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=9,
        leading=13,
        textColor=colors.white
    )

    style_bullet = ParagraphStyle(
        'Bullet_Custom',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9,
        leading=13,
        textColor=DARK_TEXT,
        leftIndent=10,
        spaceAfter=3
    )

    style_callout = ParagraphStyle(
        'CalloutText',
        parent=styles['Normal'],
        fontName='Helvetica-Oblique',
        fontSize=8.5,
        leading=12,
        textColor=colors.HexColor("#334155")
    )

    story = []

    # ====================================================
    # PAGE 1: TITLE, WELCOME, & AUDIO ARCHITECTURE
    # ====================================================
    story.append(Paragraph("STUTTER UNIT", style_cover_title))
    story.append(Paragraph("User Operation Manual & Performance Guide • Daisy Seed Stereo DSP Engine", style_cover_subtitle))
    story.append(HRFlowable(width="100%", thickness=1.5, color=PURPLE_ACCENT, spaceBefore=0, spaceAfter=8))

    story.append(Paragraph("1. Welcome & Main Purpose", style_h1))
    welcome_text = (
        "Welcome to the <b>Stutter Unit</b>! Whether you are performing live electronic sets, capturing glitchy rhythm breaks, "
        "or crafting ambient pitch-shifted textures, the Stutter Unit is designed to be your instant, expressive performance companion.<br/><br/>"
        "<b>What does the Stutter Unit do?</b><br/>"
        "At its core, the Stutter Unit is an <i>on-demand stereo audio buffer capture and micro-looping pedal</i>. "
        "While you play audio into the unit, it continuously monitors your incoming signal and tempo. The moment you press and hold "
        "the <b>Rate Encoder</b>, the pedal instantly grabs a clean stereo snippet of your performance (sliced cleanly to a musical subdivision "
        "like 1/16 or 1/8 note) and loops it indefinitely in real time.<br/><br/>"
        "While the loop is captured, turning the Rate Encoder sweeps the playback speed and pitch from sub-bass tape drops (0.25x) up to "
        "hyper-speed metallic glitches (4.0x). Releasing the encoder immediately drops you back into your live dry audio seamlessly! "
        "It gives you the intuitive control of hardware loopers like the Synthstrom Deluge with the power of high-fidelity stereo DSP."
    )
    story.append(Paragraph(welcome_text, style_body))

    callout_data = [[
        Paragraph("<b>Tip for Performers:</b> Use the Stutter Unit as a build-up riser during song transitions! Lock to MIDI clock, hit the stutter on a snare roll, and sweep the rate knob upward to create instant high-energy pitch risers before dropping back into the main beat.", style_callout)
    ]]
    callout_table = Table(callout_data, colWidths=[504])
    callout_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor("#F0F9FF")),
        ('BORDER', (0, 0), (-1, -1), 1, colors.HexColor("#BAE6FD")),
        ('PADDING', (0, 0), (-1, -1), 6),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
    ]))
    story.append(callout_table)
    story.append(Spacer(1, 10))

    story.append(Paragraph("2. Signal Flow & Audio Architecture", style_h1))
    story.append(Paragraph("Understanding how audio moves through the Daisy Seed audio engine:", style_body))
    story.append(Image(img_flow, width=6.5*inch, height=1.36*inch))
    story.append(Spacer(1, 4))
    story.append(Paragraph("Audio flows continuously through low-noise line input buffers into Daisy Seed SDRAM memory. "
                           "When triggered, the DSP engine freezes the active subdivision window and plays it back through "
                           "an ultra-smooth resampler before sending it to the analog Wet/Dry blend circuit.", style_body))

    story.append(PageBreak())

    # ====================================================
    # PAGE 2: HARDWARE INTERFACE & FRONT PANEL CONTROLS
    # ====================================================
    story.append(Paragraph("3. Hardware Interface & Controls Overview", style_h1))
    story.append(Paragraph("The physical layout puts all performance controls directly under your fingers:", style_body))
    story.append(Spacer(1, 4))
    story.append(Image(img_panel, width=6.5*inch, height=4.17*inch))
    story.append(Spacer(1, 8))

    controls_info = [
        ("Rate / Stutter Encoder", "<b>Push & Hold:</b> Engages loop capture immediately. <b>Rotate:</b> Sweeps playback speed / pitch (0.25x to 4.0x)."),
        ("5-Position Rotary Switch", "Selects loop subdivision length: <b>1/32, 1/16, 1/8, 1/4, or 1/2 note</b>."),
        ("Wet / Dry Potentiometer", "Adjusts the analog mix between your live dry signal and the stutter loop (0% to 100% Wet)."),
        ("Menu Encoder", "<b>Rotate:</b> Scroll OLED menu items. <b>Short Press:</b> Select / Edit parameter or confirm changes."),
        ("CON (Confirm) Button", "Standalone tactile button acting as a redundant 'Enter' / Confirm key for menu navigation."),
        ("BAK (Back) Button", "Standalone button to cancel menu edits, step back a screen, or exit the Debug view."),
        ("Stutter Indicator LED", "Glows bright red whenever a stutter loop is active (recording or playing back)."),
        ("OLED Display (128x64)", "High-contrast SH1106 display showing play state, rate multiplier, subdivision, and MIDI clock.")
    ]

    table_data = [[Paragraph(f"<b>{c[0]}</b>", style_body), Paragraph(c[1], style_body)] for c in controls_info]
    t_controls = Table(table_data, colWidths=[140, 364])
    t_controls.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#F8FAFC")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E1")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('PADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t_controls)

    story.append(PageBreak())

    # ====================================================
    # PAGE 3: STEP-BY-STEP OPERATION & QUANTIZATION MODES
    # ====================================================
    story.append(Paragraph("4. Step-by-Step Operating Guide", style_h1))
    
    story.append(Paragraph("4.1 Basic Momentary Stuttering", style_h2))
    story.append(Paragraph("1. Set your desired loop length using the <b>5-Position Rotary Switch</b> (e.g., 1/8 note).", style_bullet))
    story.append(Paragraph("2. Turn the <b>Wet/Dry</b> knob to 12 o'clock (50/50 mix) or fully clockwise for 100% effect.", style_bullet))
    story.append(Paragraph("3. When you reach a spot in your audio you want to stutter, <b>press and hold the Rate Encoder</b>.", style_bullet))
    story.append(Paragraph("4. The <b>Stutter LED</b> lights up, and the OLED status changes from <code>IDLE</code> to <code>PLAY</code>.", style_bullet))
    story.append(Paragraph("5. Rotate the Rate Encoder left or right to bend the pitch down to 0.25x or up to 4.00x.", style_bullet))
    story.append(Paragraph("6. Release the Rate Encoder to stop looping and instantly resume dry audio playback.", style_bullet))

    story.append(Spacer(1, 4))
    story.append(Paragraph("4.2 OLED Screen & Menu Navigation", style_h2))
    story.append(Paragraph("The system features four screen modes driven by an intuitive state machine:", style_body))

    menu_screens = [
        ["Screen", "Trigger / Event", "Description"],
        ["STATUS", "Default Boot View", "Displays live play state (IDLE/REC/PLAY), rate multiplier (e.g. 1.00x), subdivision, and MIDI clock BPM."],
        ["BROWSE", "Press Menu Encoder or CON", "Scroll through settings list (MIDI SYNC, QUANTIZE, PLAYBACK RATE MODE, DEBUG). Times out after 5s idle."],
        ["EDIT", "Press CON on a Setting", "Adjust parameter value using the Menu Encoder. Press CON to commit changes, or BAK to cancel."],
        ["DEBUG", "Select DEBUG in Menu", "Shows raw voltages, pin states, and switch values for hardware verification. Press BAK or CON to exit."]
    ]
    t_menu = Table([[Paragraph(f"<b>{cell}</b>" if i==0 else cell, style_body) for cell in row] for i, row in enumerate(menu_screens)], colWidths=[65, 125, 314])
    t_menu.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#F1F5F9")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E1")),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('PADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t_menu)
    story.append(Spacer(1, 8))

    story.append(Paragraph("5. Advanced Quantization Modes", style_h1))
    story.append(Paragraph(
        "By default, turning the Rate Encoder produces continuous pitch sweeps (like changing tape speed). "
        "However, you can configure <b>Playback Rate Mode (PRM)</b> in the Settings Menu to quantize pitch to musical semitones!", style_body
    ))

    prm_data = [
        ["Mode", "Name", "Behavior & Sound Character"],
        ["OFF", "Continuous (Default)", "Smooth, unquantized rate sweeps from 0.25x to 4.00x. Ideal for classic tape stops and manual pitch bends."],
        ["LFQ", "Loop-Frequency Quantized", "Treats the loop length as a fundamental frequency oscillator. Rate snaps to musical semitones relative to loop length!"],
        ["PTQ", "Pitch-Detection Quantized", "Runs an autocorrelation algorithm on captured audio to detect root pitch, then snaps knob sweeps to relative musical notes."]
    ]
    t_prm = Table([[Paragraph(f"<b>{cell}</b>" if i==0 else cell, style_body) for cell in row] for i, row in enumerate(prm_data)], colWidths=[40, 135, 329])
    t_prm.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#F1F5F9")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E1")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('PADDING', (0, 0), (-1, -1), 4),
    ]))
    story.append(t_prm)

    story.append(PageBreak())

    # ====================================================
    # PAGE 4: MIDI CC MAPPING, TROUBLESHOOTING & PLACEHOLDER
    # ====================================================
    story.append(Paragraph("6. Complete MIDI Control Mapping", style_h1))
    story.append(Paragraph("Connect a MIDI controller or DAW via TRS MIDI to remotely trigger or automate parameters:", style_body))

    midi_headers = ["Control", "Target Variable", "Msg Type", "CC #", "Value Translation & Behavior"]
    midi_rows = [
        ["Stutter Trigger", "runtime.trigger_active", "Note / CC", "20", "Note On = Rec/Play, Note Off = Idle | CC >= 64 = Active"],
        ["Stutter Toggle", "runtime.state", "Note / CC", "21", "Toggles latching stutter state ON / OFF"],
        ["Playback Rate", "runtime.target_rate", "CC / Pitch", "22", "0 = 0.25x, 64 = 1.0x (Unity), 127 = 4.0x"],
        ["Semitone Offset", "runtime.semitone_offset", "CC / Pitch", "23", "0 = -24 semitones, 64 = 0 semitones, 127 = +24 semitones"],
        ["Wet / Dry Mix", "runtime.wet", "CC", "24", "0 = 100% Dry, 127 = 100% Wet"],
        ["Subdivision Switch", "runtime.subdiv_pos", "CC", "25", "0-25: 1/32, 26-50: 1/16, 51-76: 1/8, 77-102: 1/4, 103-127: 1/2"],
        ["MIDI Sync Enable", "config.midi_sync", "CC", "26", "0-63 = OFF (Internal Tempo), 64-127 = ON (MIDI Clock)"],
        ["Quantize Trigger", "config.quantize", "CC", "27", "0-63 = Immediate Trigger, 64-127 = Beat Quantized"],
        ["Playback Rate Mode", "config.prm_mode", "CC", "28", "0-42 = OFF, 43-85 = LFQ, 86-127 = PTQ"],
        ["Manual BPM", "runtime.bpm", "CC", "29", "0-127 mapped linearly to 40 - 240 BPM"],
        ["Clear / Reset", "Reset Routine", "Note / CC", "30", "Instantly clears active buffer and returns to IDLE state"],
        ["Bypass / Active", "Audio Path Bypass", "CC", "31", "0-63 = True Bypass, 64-127 = Effect Engaged"]
    ]

    table_midi_data = [[Paragraph(h, style_table_header) for h in midi_headers]]
    for row in midi_rows:
        table_midi_data.append([Paragraph(cell, style_body) for cell in row])

    t_midi = Table(table_midi_data, colWidths=[85, 105, 55, 34, 225])
    t_midi.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#1E1E2E")),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E1")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('PADDING', (0, 0), (-1, -1), 3),
    ]))
    story.append(t_midi)
    story.append(Spacer(1, 6))

    story.append(Paragraph("7. Troubleshooting & Hardware Reference", style_h1))
    
    trouble_items = [
        ("No sound when holding Stutter?", "Check Wet/Dry knob position and ensure audio input jacks are connected with 10µF DC-blocking capacitors."),
        ("Buffer length out of beat sync?", "Ensure <b>MIDI SYNC</b> is set to ON in the OLED menu and incoming MIDI clock (0xF8) is active."),
        ("OLED Screen not displaying?", "Verify I2C wiring (SCL to D11, SDA to D12) and 3.3V digital power supply connection."),
        ("Enclosure Customization Graphics Placeholder", "Below is a placeholder area for your physical enclosure artwork / drill template graphics.")
    ]
    for q, a in trouble_items[:-1]:
        story.append(Paragraph(f"<b>• {q}</b>", style_h2))
        story.append(Paragraph(a, style_body))

    story.append(Paragraph(f"<b>• {trouble_items[-1][0]}</b>", style_h2))
    story.append(Paragraph(trouble_items[-1][1], style_body))
    
    placeholder_data = [[
        Paragraph("<b>[ GRAPHICS / ENCLOSURE ARTWORK PLACEHOLDER ]</b><br/>"
                  "<i>Insert custom pedal faceplate photo, wiring diagram, or vector drill template graphics here.</i>", 
                  ParagraphStyle('PlaceHolder', parent=styles['Normal'], fontName='Helvetica-Bold', fontSize=9, textColor=colors.HexColor("#64748B"), alignment=1))
    ]]
    placeholder_table = Table(placeholder_data, colWidths=[504], rowHeights=[50])
    placeholder_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor("#F1F5F9")),
        ('BORDER', (0, 0), (-1, -1), 1.5, colors.HexColor("#94A3B8")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
    ]))
    story.append(placeholder_table)

    # Build Document
    doc.build(story, canvasmaker=NumberedCanvas)
    print(f"PDF Successfully generated: {filename}")

if __name__ == '__main__':
    build_pdf()
