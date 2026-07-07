from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    Image,
    KeepTogether,
    ListFlowable,
    ListItem,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "output" / "pdf"
PDF_PATH = OUT_DIR / "kb2040-midi-controller-manual.pdf"
PINOUT_IMAGE = ROOT / "docs" / "reference" / "kb2040-pinout.png"


PAGE_WIDTH, PAGE_HEIGHT = letter
MARGIN = 0.48 * inch
CONTENT_WIDTH = PAGE_WIDTH - 2 * MARGIN


styles = getSampleStyleSheet()
styles.add(
    ParagraphStyle(
        "ManualTitle",
        parent=styles["Title"],
        fontName="Helvetica-Bold",
        fontSize=24,
        leading=29,
        textColor=colors.HexColor("#111111"),
        alignment=TA_CENTER,
        spaceAfter=14,
    )
)
styles.add(
    ParagraphStyle(
        "ManualSubtitle",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=10.5,
        leading=14,
        textColor=colors.HexColor("#333333"),
        alignment=TA_CENTER,
        spaceAfter=18,
    )
)
styles.add(
    ParagraphStyle(
        "SectionTitle",
        parent=styles["Heading1"],
        fontName="Helvetica-Bold",
        fontSize=15,
        leading=18,
        textColor=colors.HexColor("#111111"),
        spaceBefore=2,
        spaceAfter=8,
    )
)
styles.add(
    ParagraphStyle(
        "SubTitle",
        parent=styles["Heading2"],
        fontName="Helvetica-Bold",
        fontSize=10.5,
        leading=13,
        textColor=colors.HexColor("#222222"),
        spaceBefore=8,
        spaceAfter=4,
    )
)
styles.add(
    ParagraphStyle(
        "BodySmall",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=8.8,
        leading=11.5,
        textColor=colors.HexColor("#222222"),
        spaceAfter=6,
    )
)
styles.add(
    ParagraphStyle(
        "BodyTiny",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=7.4,
        leading=9.2,
        textColor=colors.HexColor("#222222"),
    )
)
styles.add(
    ParagraphStyle(
        "Callout",
        parent=styles["BodyText"],
        fontName="Helvetica-Bold",
        fontSize=9,
        leading=12,
        textColor=colors.HexColor("#111111"),
        backColor=colors.HexColor("#F2F4F7"),
        borderColor=colors.HexColor("#D0D5DD"),
        borderWidth=0.5,
        borderPadding=6,
        spaceBefore=6,
        spaceAfter=8,
    )
)
styles.add(
    ParagraphStyle(
        "Footer",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=7,
        leading=8,
        textColor=colors.HexColor("#667085"),
    )
)


def p(text, style="BodySmall"):
    return Paragraph(text, styles[style])


def bullet(items):
    return ListFlowable(
        [ListItem(p(item), bulletColor=colors.HexColor("#111111")) for item in items],
        bulletType="bullet",
        start="circle",
        leftIndent=13,
        bulletFontSize=5,
        spaceAfter=7,
    )


def table(data, col_widths, font_size=7.3, leading=8.5, padding=2.5):
    paragraph_data = []
    for row_index, row in enumerate(data):
        row_style = ParagraphStyle(
            f"TableRow{row_index}",
            parent=styles["BodyTiny"],
            fontSize=font_size,
            leading=leading,
            textColor=colors.HexColor("#101828"),
        )
        paragraph_data.append([Paragraph(str(cell), row_style) for cell in row])

    output = Table(paragraph_data, colWidths=col_widths, repeatRows=1)
    output.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#111827")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#D0D5DD")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), padding + 1),
                ("RIGHTPADDING", (0, 0), (-1, -1), padding + 1),
                ("TOPPADDING", (0, 0), (-1, -1), padding),
                ("BOTTOMPADDING", (0, 0), (-1, -1), padding),
                ("BACKGROUND", (0, 1), (-1, -1), colors.white),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F9FAFB")]),
            ]
        )
    )
    return output


def page_header_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 7)
    canvas.setFillColor(colors.HexColor("#667085"))
    canvas.drawString(MARGIN, 0.29 * inch, "KB2040 MIDI Controller Manual")
    canvas.drawRightString(PAGE_WIDTH - MARGIN, 0.29 * inch, f"Page {doc.page}")
    canvas.restoreState()


def pinout_page():
    pin_rows = [
        ["Area", "KB2040 pin", "Connects to", "Notes"],
        ["Power", "RAW", "5V OUT from buck converter", "Powers KB2040 from pedalboard supply"],
        ["Ground", "GND", "All switches, OLED, encoder, DIN shield", "Shared common ground"],
        ["Recovery", "D1 / GPIO1", "Jumper or switch to GND", "Hold at boot for web config"],
        ["OLED SDA", "D2 / GPIO2", "SSD1306 SDA", "Display uses Wire1"],
        ["OLED SCL", "D3 / GPIO3", "SSD1306 SCL", "Display uses Wire1"],
        ["Stomp 1", "D9 / GPIO9", "Momentary switch to GND", "Default Previous Preset, CC72 value 63"],
        ["Stomp 2", "D8 / GPIO8", "Momentary switch to GND", "Default Next Preset, CC72 value 127"],
        ["Stomp 3", "D7 / GPIO7", "Momentary switch to GND", "Default Tuner On/Off, CC68 value 127"],
        ["Stomp 4", "D6 / GPIO6", "Momentary switch to GND", "Default Snapshot 1, CC69 value 0"],
        ["Stomp 5", "D5 / GPIO5", "Momentary switch to GND", "Default Snapshot 2, CC69 value 1"],
        ["Stomp 6", "D4 / GPIO4", "Momentary switch to GND", "Default Snapshot 3, CC69 value 2"],
        ["DIN MIDI TX", "D0 / GPIO0", "DIN pin 5 through 220 ohm", "UART TX at 31250 baud"],
        ["DIN MIDI +5V", "5V OUT", "DIN pin 4 through 220 ohm", "From buck converter output"],
        ["DIN MIDI shield", "GND", "DIN pin 2", "Pins 1 and 3 not connected"],
        ["STEMMA QT encoder", "STEMMA QT JST", "VIN, GND, SDA, SCL", "Default menu control, seesaw at 0x36"],
        ["QT encoder INT", "D10 / GPIO10", "Encoder INT pad", "Active in STEMMA QT mode"],
        ["Two-button Previous", "D10 / GPIO10", "Button to GND", "Alternate mode, shares D10"],
        ["Two-button Next", "A0 / GPIO26", "Button to GND", "Alternate mode"],
        ["GPIO encoder A", "A1 / GPIO27", "Encoder A", "Reserved while QT mode is active"],
        ["GPIO encoder B", "A2 / GPIO28", "Encoder B", "Reserved while QT mode is active"],
        ["GPIO encoder switch", "A3 / GPIO29", "Switch to GND", "Reserved while QT mode is active"],
    ]

    image = Image(str(PINOUT_IMAGE))
    image.drawWidth = CONTENT_WIDTH * 0.75
    image.drawHeight = CONTENT_WIDTH * 0.75
    image.drawHeight = image.drawWidth * 0.75
    image.hAlign = "CENTER"

    return [
        p("Pinout And Wiring Map", "SectionTitle"),
        p(
            "This page is the single-page wiring reference for the current KB2040 build. "
            "The default menu control is the Adafruit STEMMA QT rotary encoder. GPIO encoder and two-button menu pins remain configurable alternatives.",
            "BodySmall",
        ),
        image,
        Spacer(1, 4),
        table(
            pin_rows,
            [1.0 * inch, 1.05 * inch, 2.18 * inch, 3.15 * inch],
            font_size=5.65,
            leading=6.5,
            padding=1.3,
        ),
    ]


def build_story():
    story = []
    story.extend(
        [
            Spacer(1, 0.35 * inch),
            p("KB2040 MIDI Controller", "ManualTitle"),
            p(
                "Current-build manual for the Adafruit KB2040 based USB and DIN MIDI stompbox controller. "
                "Covers operation, web configuration, onboard menu use, wiring, flashing, and recovery.",
                "ManualSubtitle",
            ),
            p("Current Hardware", "SectionTitle"),
            bullet(
                [
                    "Adafruit KB2040 running the Philhower RP2040 Arduino core.",
                    "Six configurable stomp switches, wired to ground with internal pull-ups.",
                    "128x64 SSD1306 I2C OLED on Wire1, with the active preset name and six full-width switch labels on the home screen.",
                    "Adafruit STEMMA QT I2C rotary encoder as the default menu control, with D10 used for the interrupt line.",
                    "5-pin DIN MIDI output plus USB MIDI output, with output routing selectable in the onboard menu or web configurator.",
                    "USB web configurator served from the pedal at http://192.168.7.1.",
                ]
            ),
            p("Quick Use", "SectionTitle"),
            bullet(
                [
                    "Tap a stomp switch to send its configured MIDI command. Toggle and momentary behavior are configured per command.",
                    "Hold the encoder button for about 800 ms to open the onboard menu. Rotate to move, short-press to select, and hold to go back.",
                    "Open Menu > System > Web Config > Start web config, then browse to http://192.168.7.1.",
                    "Hold D1 to GND during boot for hard web-config recovery. In STEMMA QT mode, holding the encoder button during boot also enters web config if the encoder is detected.",
                ]
            ),
            p(
                "Saved configuration is versioned and stored in LittleFS. Flashing firmware normally preserves presets, commands, pin mappings, and device settings.",
                "Callout",
            ),
            PageBreak(),
        ]
    )

    story.extend(pinout_page())
    story.append(PageBreak())

    story.extend(
        [
            p("Onboard Controls And OLED", "SectionTitle"),
            p(
                "The home screen shows the active preset at the top and six stacked switch labels below it. Long labels scroll when needed.",
                "BodySmall",
            ),
            p("Menu Navigation", "SubTitle"),
            bullet(
                [
                    "Performance mode ignores encoder rotation and short presses so accidental bumps do not change presets.",
                    "Hold the encoder button for about 800 ms to open the menu.",
                    "Rotate to move. Short-press to select. Hold to go back one level.",
                    "A right-edge mini scrollbar shows menu position. Menu pages no longer use text like 1 of 9.",
                    "The menu exits back to the home screen after 10 seconds without input.",
                ]
            ),
            p("Main Menu Items", "SubTitle"),
            table(
                [
                    ["Menu item", "What it does"],
                    ["Presets", "Choose the active preset. The secondary line shows the current preset name."],
                    ["Switch Assign", "Edit which command is assigned to each configured stomp switch."],
                    ["MIDI Output", "Select USB and DIN, DIN only, or USB only."],
                    ["Home Delay", "Set how long the last-button output screen stays visible."],
                    ["Hold Edit", "Open the On/Off submenu for stomp-switch hold-to-edit behavior."],
                    ["System", "Open Web Config, Reset State, Firmware Update, or Restore to default settings."],
                    ["Exit", "Close the menu and return to the home screen."],
                ],
                [1.55 * inch, 5.75 * inch],
                font_size=7.4,
                leading=8.7,
            ),
            Spacer(1, 8),
            p("System Menu Items", "SubTitle"),
            table(
                [
                    ["System item", "What it does"],
                    ["Web Config", "Start USB network configuration mode. The menu shows 192.168.7.1."],
                    ["Reset State", "Clear live toggle state for the current preset without sending MIDI."],
                    ["Firmware Update", "Restart into the RP2040 ROM bootloader as the RPI-RP2 drive."],
                    ["Restore to default settings", "Restore factory defaults after an Are You Sure confirmation."],
                    ["Back", "Return to the main menu."],
                ],
                [1.95 * inch, 5.35 * inch],
                font_size=7.15,
                leading=8.5,
            ),
        ]
    )
    story.append(PageBreak())

    story.extend(
        [
            p("Web Configurator", "SectionTitle"),
            p(
                "Configuration mode presents the KB2040 as a USB network adapter and serves the web app directly from firmware flash. "
                "It does not require Web MIDI, WebUSB, an internet connection, or a browser extension.",
                "BodySmall",
            ),
            p("Entering Web Config", "SubTitle"),
            bullet(
                [
                    "Preferred: Menu > System > Web Config > Start web config.",
                    "Hard recovery: hold D1 to GND while powering or resetting.",
                    "STEMMA QT shortcut: if STEMMA QT mode is active and the encoder is detected, hold the encoder button while powering or resetting.",
                    "Two-button mode can also use the configured Previous plus Next boot combo.",
                ]
            ),
            p("Address", "SubTitle"),
            p("Open http://192.168.7.1 in a browser after the OLED reports configuration mode.", "Callout"),
            p("What You Can Edit", "SubTitle"),
            bullet(
                [
                    "Device name, theme preference, stomp switch count, switch pin mapping, and menu control mode.",
                    "MIDI output routing and last-button display return-home delay.",
                    "Presets, active preset, preset page toggle, and switch command assignments.",
                    "Device Profiles, MIDI channels, CC commands, Program Change commands, and PC display numbering.",
                    "Firmware update mode from the Device tab or System menu, which restarts the board as an RPI-RP2 USB drive.",
                ]
            ),
            p("Save & restart writes the complete configuration to flash and restarts the controller in normal MIDI mode.", "BodySmall"),
        ]
    )
    story.append(PageBreak())

    story.extend(
        [
            p("Presets, Profiles, And Commands", "SectionTitle"),
            p(
                "The controller separates pedal pages from device command libraries. A preset chooses what each stomp switch does. "
                "A Device Profile owns the command definitions for a MIDI device.",
                "BodySmall",
            ),
            p("Presets", "SubTitle"),
            bullet(
                [
                    "Up to 8 named presets can be stored.",
                    "Each preset has its own switch mapping.",
                    "The active preset name is shown at the top of the OLED home screen.",
                    "One preset page toggle can reserve a stomp switch to flip between two selected presets without sending MIDI.",
                ]
            ),
            p("Device Profiles", "SubTitle"),
            bullet(
                [
                    "Up to 8 Device Profiles can be stored.",
                    "Each profile has its own MIDI channel from 1-16.",
                    "Each profile can hold up to 40 commands.",
                    "Program Change display can be 0-127 or 1-128 while the raw MIDI byte remains 0-127.",
                ]
            ),
            p("Command Types", "SubTitle"),
            table(
                [
                    ["Type", "Behavior"],
                    ["Fixed value", "Sends one configured value when pressed."],
                    ["Toggle", "Alternates between value on and value off."],
                    ["Momentary", "Sends value on at press and value off at release."],
                    ["Program Change", "Sends a MIDI Program Change message instead of a Control Change message."],
                ],
                [1.5 * inch, 5.8 * inch],
                font_size=7.6,
                leading=9,
            ),
            Spacer(1, 10),
            p(
                "Default HX Stomp assignments are Previous Preset, Next Preset, Tuner On/Off, Snapshot 1, Snapshot 2, and Snapshot 3, but all six switch assignments are configurable.",
                "Callout",
            ),
        ]
    )
    story.append(PageBreak())

    story.extend(
        [
            p("Build, Flash, And Recovery", "SectionTitle"),
            p("Board Target", "SubTitle"),
            table(
                [
                    ["Setting", "Value"],
                    ["Board", "Adafruit KB2040"],
                    ["Core", "Raspberry Pi Pico/RP2040/RP2350 by Earle F. Philhower, III"],
                    ["FQBN", "rp2040:rp2040:adafruit_kb2040:flash=8388608_524288"],
                    ["USB stack", "Pico SDK TinyUSB"],
                    ["Required libraries", "U8g2, MIDI Library, ArduinoJson, Adafruit seesaw Library"],
                ],
                [1.65 * inch, 5.65 * inch],
                font_size=7.4,
                leading=8.7,
            ),
            p("Normal Flash Flow", "SubTitle"),
            bullet(
                [
                    "Compile the sketch for the KB2040 FQBN above.",
                    "Upload through the current COM port, commonly COM3 on the current Windows setup.",
                    "During upload the board resets, mounts briefly as RPI-RP2, receives NEW.UF2, then reappears as Adafruit KB2040.",
                    "After flashing, re-run board list to confirm the board returned.",
                ]
            ),
            p("Recovery", "SubTitle"),
            bullet(
                [
                    "If normal firmware boots: use Menu > System > Firmware Update to restart into the RPI-RP2 bootloader.",
                    "If the firmware cannot start: use the physical KB2040 BOOT/RESET method.",
                    "If you only need web config: hold D1 to GND while booting, or hold the STEMMA QT encoder button at boot when that mode is active and the encoder is detected.",
                ]
            ),
            p("Troubleshooting", "SubTitle"),
            table(
                [
                    ["Symptom", "Check"],
                    ["No menu movement", "Confirm menu mode is STEMMA QT, encoder JST is connected, and D10 INT is wired."],
                    ["Encoder direction feels reversed", "STEMMA QT direction is firmware-adjusted; GPIO encoder direction can be changed by swapping A and B."],
                    ["No DIN MIDI", "Check DIN pin numbering, D0 to pin 5 through 220 ohm, and 5V to pin 4 through 220 ohm."],
                    ["Web page unavailable", "Confirm USB network mode and browse to 192.168.7.1. Use D1 recovery if needed."],
                    ["OLED blank", "Check OLED power, GND, D2 SDA, D3 SCL, and I2C address compatibility."],
                ],
                [2.0 * inch, 5.3 * inch],
                font_size=7.3,
                leading=8.6,
            ),
        ]
    )
    return story


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    document = SimpleDocTemplate(
        str(PDF_PATH),
        pagesize=letter,
        rightMargin=MARGIN,
        leftMargin=MARGIN,
        topMargin=0.42 * inch,
        bottomMargin=0.48 * inch,
        title="KB2040 MIDI Controller Manual",
        author="Codex",
        subject="Current build manual for the KB2040 MIDI controller",
    )
    document.build(build_story(), onFirstPage=page_header_footer, onLaterPages=page_header_footer)
    print(PDF_PATH)


if __name__ == "__main__":
    main()
