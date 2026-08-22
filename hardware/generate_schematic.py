#!/usr/bin/env python3
"""Genere un premier jet de schema KiCad (hucheor.kicad_sch) a partir du
cablage documente dans WIRING.md.

IMPORTANT : genere sans avoir KiCad installe sur cette machine, donc jamais
verifie visuellement ni passe a l'ERC. A ouvrir dans KiCad et verifier avant
de faire confiance a ce fichier pour quoi que ce soit de reel - voir la note
en tete de hardware/README.md.
"""

import uuid as uuid_mod

from kiutils.schematic import Schematic, SchematicSymbol, Property, LocalLabel, NoConnect
from kiutils.items.common import Position, Effects, Font
from kiutils.items.schitems import SymbolProjectInstance, SymbolProjectPath
from kiutils.symbol import Symbol, SymbolPin, SyRect

PROJECT_NAME = "hucheor"
PIN_LEN = 2.54
ROW = 2.54


def new_uuid():
    return str(uuid_mod.uuid4())


def make_module_symbol(entry_name, value, left_pins, right_pins, is_power=False):
    """left_pins/right_pins: liste de (name, number, electrical_type)."""
    n_left, n_right = len(left_pins), len(right_pins)
    n_max = max(n_left, n_right, 1)
    half_height = (n_max - 1) * ROW / 2 + ROW
    half_width = 12.7  # assez large pour les noms de broche les plus longs

    symbol = Symbol.create_new(id=entry_name, reference="U", value=value)
    symbol.isPower = is_power
    symbol.pinNames = True
    symbol.pinNumbers = True

    symbol.graphicItems.append(SyRect(
        start=Position(X=-half_width, Y=half_height),
        end=Position(X=half_width, Y=-half_height),
    ))

    def add_side(pins, side):
        count = len(pins)
        top_y = (count - 1) * ROW / 2
        for i, (name, number, etype) in enumerate(pins):
            y = top_y - i * ROW
            if side == "L":
                x = -half_width - PIN_LEN
                angle = 180
            else:
                x = half_width + PIN_LEN
                angle = 0
            symbol.pins.append(SymbolPin(
                electricalType=etype,
                position=Position(X=x, Y=y, angle=angle),
                length=PIN_LEN,
                name=name,
                number=str(number),
            ))

    add_side(left_pins, "L")
    add_side(right_pins, "R")
    return symbol


def place(schematic, symbol_def, ref, value, x, y, pin_side_map):
    """Place une instance du symbole sur la feuille, renvoie un dict
    {pin_name: (abs_x, abs_y)} pour poser les labels de net ensuite."""
    inst = SchematicSymbol()
    inst.libId = symbol_def.libId
    inst.position = Position(X=x, Y=y)
    inst.inBom = True
    inst.onBoard = True
    inst.uuid = new_uuid()
    inst.properties = [
        Property(key="Reference", value=ref, id=0,
                  position=Position(X=x, Y=y - 15), effects=Effects(font=Font(width=1.27, height=1.27))),
        Property(key="Value", value=value, id=1,
                  position=Position(X=x, Y=y - 12.5), effects=Effects(font=Font(width=1.27, height=1.27))),
    ]
    inst.instances = [SymbolProjectInstance(
        name=PROJECT_NAME,
        paths=[SymbolProjectPath(sheetInstancePath="/", reference=ref, unit=1)],
    )]

    pin_positions = {}
    for pin in symbol_def.pins:
        inst.pins[pin.number] = new_uuid()
        pin_positions[pin.name] = (x + pin.position.X, y + pin.position.Y)

    schematic.schematicSymbols.append(inst)
    return pin_positions


def add_net_label(schematic, text, pos, angle=0):
    schematic.labels.append(LocalLabel(
        text=text,
        position=Position(X=pos[0], Y=pos[1], angle=angle),
        effects=Effects(font=Font(width=1.27, height=1.27)),
    ))


def add_no_connect(schematic, pos):
    schematic.noConnects.append(NoConnect(position=Position(X=pos[0], Y=pos[1])))


def main():
    sch = Schematic.create_new()
    sch.titleBlock = None

    # --- Definitions de symboles (modules du commerce, pas de symbole
    # officiel KiCad pour ces breakouts hobbyistes) ---
    esp32 = make_module_symbol(
        "ESP32_DevKit", "ESP32-WROOM-32 DevKit",
        left_pins=[
            ("VIN_5V", 1, "power_in"), ("GND", 2, "power_in"), ("3V3", 3, "power_out"),
            ("GPIO18", 4, "bidirectional"), ("GPIO19", 5, "bidirectional"), ("GPIO23", 6, "bidirectional"),
        ],
        right_pins=[
            ("GPIO5", 7, "bidirectional"), ("GPIO4", 8, "bidirectional"), ("GPIO26", 9, "bidirectional"),
            ("GPIO25", 10, "bidirectional"), ("GPIO27", 11, "bidirectional"), ("GPIO32", 12, "bidirectional"),
        ],
    )
    cc1101 = make_module_symbol(
        "CC1101_Module", "CC1101 868MHz",
        left_pins=[("VCC", 1, "power_in"), ("GND", 2, "power_in"), ("CSN", 3, "input"), ("GDO0", 4, "output")],
        right_pins=[("SCK", 5, "input"), ("MISO", 6, "output"), ("MOSI", 7, "input"), ("GDO2", 8, "output")],
    )
    max98357a = make_module_symbol(
        "MAX98357A_Module", "MAX98357A I2S Amp",
        left_pins=[
            ("VIN", 1, "power_in"), ("GND", 2, "power_in"),
            ("BCLK", 3, "input"), ("LRC", 4, "input"), ("DIN", 5, "input"),
        ],
        right_pins=[("SD", 6, "input"), ("GAIN", 7, "input"), ("SPKP", 8, "output"), ("SPKN", 9, "output")],
    )
    dcf77 = make_module_symbol(
        "DCF77_Module", "Recepteur DCF77",
        left_pins=[("VCC", 1, "power_in"), ("GND", 2, "power_in")],
        right_pins=[("OUT", 3, "output")],
    )
    speaker = make_module_symbol(
        "Speaker", "HP 4ohm 3-5W",
        left_pins=[("A", 1, "passive")],
        right_pins=[("B", 2, "passive")],
    )
    power_jack = make_module_symbol(
        "Power_5V", "Alimentation 5V",
        left_pins=[("PLUS", 1, "passive")],
        right_pins=[("GND", 2, "passive")],
    )

    for s in (esp32, cc1101, max98357a, dcf77, speaker, power_jack):
        sch.libSymbols.append(s)

    # --- Placement ---
    p_u1 = place(sch, esp32, "U1", "ESP32-WROOM-32 DevKit", 70, 100, None)
    p_u2 = place(sch, cc1101, "U2", "CC1101 868MHz", 160, 40, None)
    p_u3 = place(sch, max98357a, "U3", "MAX98357A I2S Amp", 160, 100, None)
    p_u4 = place(sch, dcf77, "U4", "Recepteur DCF77", 160, 160, None)
    p_sp1 = place(sch, speaker, "SP1", "HP 4ohm 3-5W", 230, 100, None)
    p_j1 = place(sch, power_jack, "J1", "Alimentation 5V", 20, 40, None)

    # --- Connexions par labels de net (voir WIRING.md pour la correspondance) ---
    nets = [
        ("+5V", [p_u1["VIN_5V"], p_u3["VIN"], p_j1["PLUS"]]),
        ("GND", [p_u1["GND"], p_u2["GND"], p_u3["GND"], p_u4["GND"], p_j1["GND"]]),
        ("+3V3", [p_u1["3V3"], p_u2["VCC"], p_u4["VCC"]]),
        ("GPIO18_SCK", [p_u1["GPIO18"], p_u2["SCK"]]),
        ("GPIO19_MISO", [p_u1["GPIO19"], p_u2["MISO"]]),
        ("GPIO23_MOSI", [p_u1["GPIO23"], p_u2["MOSI"]]),
        ("GPIO5_CSN", [p_u1["GPIO5"], p_u2["CSN"]]),
        ("GPIO4_GDO0", [p_u1["GPIO4"], p_u2["GDO0"]]),
        ("GPIO26_BCLK", [p_u1["GPIO26"], p_u3["BCLK"]]),
        ("GPIO25_LRC", [p_u1["GPIO25"], p_u3["LRC"]]),
        ("GPIO27_DIN", [p_u1["GPIO27"], p_u3["DIN"]]),
        ("GPIO32_DCF", [p_u1["GPIO32"], p_u4["OUT"]]),
        ("SPK_POS", [p_u3["SPKP"], p_sp1["A"]]),
        ("SPK_NEG", [p_u3["SPKN"], p_sp1["B"]]),
    ]
    for net_name, positions in nets:
        for pos in positions:
            add_net_label(sch, net_name, pos)

    # Broches volontairement non connectees (voir WIRING.md)
    for pos in (p_u2["GDO2"], p_u3["SD"], p_u3["GAIN"]):
        add_no_connect(sch, pos)

    sch.to_file("hucheor.kicad_sch")
    print("Ecrit hucheor.kicad_sch")


if __name__ == "__main__":
    main()
