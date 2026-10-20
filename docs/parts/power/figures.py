"""The figures of the power board, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/power/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, exp, ln

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("power", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "power-schematic.svg"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
gr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="graph", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
fig = MODEL.derived

Q1 = "ST STP160N3LL, DocID025073 Rev 3"
BULK = "Rubycon 16ZLH1000MEFC8X20"
Q1_SHEET = "STP160N3LL-ST.pdf"
BULK_SHEET = "ZLH-Rubycon.pdf"
WIRE = "ASTM B258, annealed copper at 20 °C"
AMPS = "PowerStream American Wire Gauge table, maximum amps for chassis wiring"

LOADS = "Loads"
INPUT = "Input path"
RAIL = "Rail ceiling"
FLOORS = "Supply floors"
FUSE_SEC = "Fuse"
ALLOW = "What a replacement part has to meet"

# ===========================================================================
# what the other subsystems draw and accept
# ===========================================================================
MODEL.uses("i_supply_3a", of="lighting", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_supply_6a", of="lighting", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_budget", of="lighting", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_budget_6a", of="lighting", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_feed_max", of="solenoid", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_coil", of="solenoid", group="LOADS", section=LOADS, stated=True)
MODEL.uses("t_on_max", of="solenoid", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_stall", of="servo", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_amp", of="audio", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_teensy_40", of="teensy-4.1", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_teensy_3v3", of="teensy-4.1", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_f_max", of="break-beam", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_tot", of="ir-reflective", group="LOADS", section=LOADS, stated=True)
MODEL.uses("i_disp", of="display", group="LOADS", section=LOADS, stated=True)
MODEL.uses("v_in_mod_min", of="display", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("v_mod_max", of="ir-reflective", group="LOADS", section=LOADS, stated=True)
MODEL.uses("v_mod_nom", of="ir-reflective")
MODEL.uses("v_in_max", of="teensy-4.1", group="RAIL", section=RAIL, stated=True)
MODEL.uses("v_in_min", of="teensy-4.1", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("v_mod_in_min", of="ir-reflective", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("vcc_min", of="lighting", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("vcc_u1_min", of="servo")
MODEL.uses("v_rail", of="lighting")
MODEL.uses("v_buffer_min", of="lighting")
MODEL.uses("v_buffer_min_6a", of="lighting")
MODEL.uses("r_lead", of="solenoid", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("i_coils_at_once", of="solenoid", group="FLOORS", section=FLOORS, stated=True)
# D1 on the Teensy motherboard is the solenoids' flyback type, a 1N5819
MODEL.uses("v_f_diode", of="solenoid", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("i_f_av", of="solenoid")
MODEL.uses("v_coil_min", of="solenoid", group="FLOORS", section=FLOORS, stated=True)
MODEL.uses("t_a", of="ir-reflective", group="INPUT", section=INPUT, stated=True)
# the bulk on the subsystem boards: C3 is this board's part, C91 and C1 a smaller one
MODEL.uses("n_bulk_light", of="lighting")
MODEL.uses("c_c91", of="solenoid")
MODEL.uses("c_c1", of="servo")
MODEL.uses("z_c1", of="servo")
MODEL.uses("l_feed", of="solenoid")

# ===========================================================================
# the adapter and the cables
# ===========================================================================
dec("v_jack_min", 5.0, "V", src="the least the adapter delivers at the jack at its full "
    "current, which the build requires of the adapter, since every subsystem's figures "
    "start from a 5.0 V rail", group="INPUT", section=INPUT, stated=True)
asm("v_jack_max", 5.25, "V", src="the most the adapter puts out with no load, its load "
    "steps included: 5 % over its nominal 5 V, the usual tolerance of a regulated "
    "adapter; the BTF-LIGHTING listing states none", group="RAIL", section=RAIL, stated=True)
asm("len_adapter_min", 1.0, "m", src="the shortest adapter cable the build expects; a "
    "shorter cable damps the plug-in ringing less", group="RAIL", section=RAIL, stated=True)
asm("len_adapter_max", 2.0, "m", src="the longest adapter cable the build expects; a "
    "longer cable stores more energy at a load release", group="RAIL", section=RAIL,
    stated=True)
asm("r_awg_adapter", 20.95, "mΩ/m", src=f"the adapter's cable taken as 18 AWG, {WIRE}; "
    "a thick gauge for a barrel-plug adapter, so the damping below is on the low side",
    group="RAIL", section=RAIL, stated=True)
asm("l_per_m", 1, "µH/m", src="a loose pair's loop inductance, roughly 1 µH per meter, "
    "the figure the solenoid design's feed takes; estimated from the conductor and not "
    "measured", group="RAIL", section=RAIL, stated=True)
dec("len_dcin", 30, "cm", src="the longest pair from the barrel jack to the board",
    group="INPUT", section=INPUT, stated=True)
ds("r_awg16", 13.17, "mΩ/m", src=f"{WIRE}, 16 AWG", group="INPUT", section=INPUT,
   stated=True)
asm("r_board", 5, "mΩ", src="the board's own wiring from the input to the outputs, both "
    "conductors and their solder joints, estimated and not measured",
    group="INPUT", section=INPUT, stated=True)
ds("i_awg16_max", 22, "A", src=f"{AMPS}, 16 AWG", stated=False)
ds("i_awg22_max", 7, "A", src=f"{AMPS}, 22 AWG", stated=False)

# ===========================================================================
# the Teensy on USB alone: D2 across the cut VUSB-VIN trace, the same 1N5819 as D1
# ===========================================================================
USB = "Universal Serial Bus Specification Revision 2.0"
ds("v_usb_min", 4.75, "V", src=f"{USB}, Table 7-7: VBUS at a high-power port, minimum",
   group="FLOORS", section=FLOORS, label="at least", stated=True)
ds("i_usb_max", 500, "mA", src=f"{USB}, 7.2.1: the most a high-power port supplies, five "
   "unit loads", group="FLOORS", section=FLOORS, stated=True)

# ===========================================================================
# Q1, the reverse-polarity switch in the ground line. It is optional, and every figure takes it
# fitted: leaving it out only takes its drop off the bus.
# ===========================================================================
ds("rdson_q1", 4.2, "mΩ", sheet=Q1_SHEET, src=f"{Q1}, Table 4, R_DS(on) maximum at V_GS = 4.5 V, "
   "I_D = 60 A, T_case = 25 °C", group="INPUT", section=INPUT, stated=True)
ds("t_row", 25, "°C", sheet=Q1_SHEET, src=f"{Q1}, Table 4, the case temperature the R_DS(on) rows are "
   "given at", group="INPUT", section=INPUT, stated=True)
ds("v_gs_spec", 4.5, "V", sheet=Q1_SHEET, src=f"{Q1}, Table 4, the gate voltage the 4.2 mΩ row is "
   "specified at", group="INPUT", section=INPUT, stated=True)
ds("v_gs_th_max", 2.5, "V", sheet=Q1_SHEET, src=f"{Q1}, Table 4, V_GS(th) maximum at I_D = 250 µA",
   group="RAIL", section=RAIL, stated=True)
ds("v_ds_max_q1", 30, "V", sheet=Q1_SHEET, src=f"{Q1}, Table 2, drain-source voltage", stated=False)
ds("v_gs_max_q1", 20, "V", sheet=Q1_SHEET, src=f"{Q1}, Table 2, gate-source voltage, ±20 V", stated=False)
ds("i_d_pkg", 120, "A", sheet=Q1_SHEET, src=f"{Q1}, Table 2, drain current continuous at "
   "T_case = 25 °C, which the sheet marks as limited by the package; the body diode's "
   "current runs through the same package, and Table 7 tests the diode at 120 A, so the "
   "figure bounds the plug-in current through the diode too", group="RAIL", section=RAIL,
   stated=True)
ds("i_dss_q1", 1, "µA", sheet=Q1_SHEET, src=f"{Q1}, Table 4, I_DSS maximum at V_DS = 30 V",
   group="INPUT", section=INPUT, stated=True)
ds("rth_q1", 62.5, "°C/W", sheet=Q1_SHEET, src=f"{Q1}, Table 3, thermal resistance junction-ambient",
   group="INPUT", section=INPUT, stated=True)
gr("t_fig10_a", 45, "°C", src=f"{Q1}, Figure 10, normalized R_DS(on) against "
   "temperature, the gridline at 45 °C", stated=False)
gr("k_fig10_a", 1.1, "", src=f"{Q1}, Figure 10, the normalized R_DS(on) read at 45 °C",
   stated=False)
gr("t_hot", 95, "°C", src=f"{Q1}, Figure 10, the gridline at 95 °C",
   group="INPUT", section=INPUT, stated=True)
gr("k_hot", 1.4, "", src=f"{Q1}, Figure 10, the normalized R_DS(on) read at 95 °C, "
   "which bounds it for any junction below that", group="INPUT", section=INPUT, stated=True)
gr("t_fig10_c", 145, "°C", src=f"{Q1}, Figure 10, the gridline at 145 °C", stated=False)
gr("k_fig10_c", 1.65, "", src=f"{Q1}, Figure 10, the normalized R_DS(on) read at 145 °C",
   stated=False)
ds("i_gss_q1", 100, "nA", sheet=Q1_SHEET, src=f"{Q1}, Table 4, I_GSS maximum at V_DS = 0 V "
   "and V_GS = ±20 V", group="INPUT", section=INPUT, stated=True)
dec("r_gate", 10, "kΩ", src="R1, from Q1's gate to the 5 V bus: the 10 kΩ of the resistor "
    "kit. The gate draws only its leakage, so nothing bounds "
    "R1 from below", stated=False)
MODEL.input("kit_tol", 5, "%", kind="datasheet", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland product page: tolerance 5 %", stated=False)

MODEL.curve(f"{Q1}, Figure 10, normalized R_DS(on) against junction temperature",
            [("t_fig10_a", "k_fig10_a"), ("t_hot", "k_hot"), ("t_fig10_c", "k_fig10_c")],
            rises=True)

# ===========================================================================
# C1 and C2, the bulk at the input
# ===========================================================================
dec("c_one", 1000, "µF", src=f"C1 and C2 each, {BULK}; the C2 on each lighting board is the "
    "same part and takes its figures from here, while the solenoid board's C91 and the "
    "servo's C1 are a smaller part of their own", group="RAIL", section=RAIL,
    stated=True)
dec("n_bulk", 2, "", src="C1 and C2 in parallel, the fewest of that part that hold the "
    "release under the Teensy's VIN maximum with the 6 A adapter", stated=False)
ds("z_c", 29, "mΩ", sheet=BULK_SHEET, src=f"{BULK}, impedance maximum at 100 kHz as DigiKey lists it from "
   "the ZLH sheet",
   group="RAIL", section=RAIL, stated=True)
ds("v_c_rating", 16, "V", sheet=BULK_SHEET, src=f"{BULK}, rated voltage", stated=False)
dec("derate_elko", 80, "%", src="the electrolytic voltage target of the design-review "
    "skill, at most 80 % of the rating", stated=False)
asm("place_bulk", 20, "mm", src="how close C1 and C2 sit to J-IN, where the input current "
    "enters the bus; no datasheet states one", stated=False)

# ===========================================================================
# the D24V5F3 at the IR mainboard
# ===========================================================================
asm("eta_u1", 80, "%", src="the D24V5F3's efficiency, the low end of the 80 % to 93 % Pololu gives "
    "as typical for the D24V5F3; no minimum is published", group="LOADS", section=LOADS,
    stated=True)

# ===========================================================================
# F1, the optional fuse in the 5 V of the outputs that draw little current
# ===========================================================================
FUSE = "a T2A fuse, 2 A time-lag, 5 x 20 mm"
FST = "Schurter FST 5x20 datasheet, schurter.com"
dec("i_f1", 2, "A", src=f"F1, {FUSE}: the smallest standard time-lag rating above what the "
    "outputs behind it draw at most", group="FUSE", section=FUSE_SEC, stated=True)
msr("i_meas_f1", 1.6, "A", src="bench: the fuse fitted as F1 in series with a bench supply set "
    "to 1 V, the current read off the supply", group="FUSE", section=FUSE_SEC, stated=True)
msr("v_meas_f1", 55, "mV", src="bench: the voltage across that fuse at that current, read with "
    "a multimeter", group="FUSE", section=FUSE_SEC, stated=True)
dec("v_margin_f1", 10, "mV", src="added to the measured drop for the warming between the "
    "measured current and the most behind F1", group="FUSE", section=FUSE_SEC, stated=True)
ds("k_open", 2.1, "×", src=f"{FST}, Pre-Arcing Time to IEC 60127-2/3, the multiple of In with a "
   "maximum time for 0.125 to 6.3 A", group="FUSE", section=FUSE_SEC, stated=True)
ds("t_open", 120, "s", src=f"{FST}, Pre-Arcing Time, the maximum at 2.1 × In for 0.125 to 6.3 A",
   group="FUSE", section=FUSE_SEC, stated=True)


# ===========================================================================
# derived: the input path
# ===========================================================================
@fig("r_dcin", "mΩ", group="INPUT", section=INPUT, rises_with=["len_dcin", "r_awg16"])
def _(len_dcin, r_awg16):
    return 2 * len_dcin * r_awg16


# Q1's R_DS(on) is bounded by its reading at 95 °C, which holds for any junction below.
@fig("r_q1", "mΩ", group="INPUT", section=INPUT, prints="up",
     rises_with=["rdson_q1", "k_hot"])
def _(rdson_q1, k_hot):
    return rdson_q1 * k_hot


@fig("p_q1_6a", "W", group="INPUT", section=INPUT, prints="up",
     rises_with=["i_supply_6a", "rdson_q1", "k_hot"])
def _(i_supply_6a, r_q1):
    return i_supply_6a * i_supply_6a * r_q1


@fig("t_j_6a", "°C", group="INPUT", section=INPUT, prints="up",
     rises_with=["t_a", "i_supply_6a", "rdson_q1", "k_hot", "rth_q1"])
def _(t_a, p_q1_6a, rth_q1):
    return t_a + p_q1_6a * rth_q1


# The adapter's whole current crosses the input pair, Q1 and the board's wiring. It
# never exceeds the adapter's rating, so the rating bounds the drop.
@fig("v_bus_3a", "V", group="INPUT", section=INPUT, prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_3a", "len_dcin", "r_awg16",
                                            "rdson_q1", "r_board"])
def _(v_jack_min, i_supply_3a, r_dcin, r_q1, r_board):
    return v_jack_min - i_supply_3a * (r_dcin + r_q1 + r_board)


@fig("v_bus_6a", "V", group="INPUT", section=INPUT, prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_6a", "len_dcin", "r_awg16",
                                            "rdson_q1", "r_board"])
def _(v_jack_min, i_supply_6a, r_dcin, r_q1, r_board):
    return v_jack_min - i_supply_6a * (r_dcin + r_q1 + r_board)


# The gate's leakage flows through R1, so the most R1 is the one whose drop at its high end
# still leaves the gate at the voltage the R_DS(on) row is specified at, on the bus at 6 A.
@fig("r_gate_max", "MΩ", group="INPUT", section=INPUT, prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_6a", "len_dcin", "r_awg16", "rdson_q1",
                                            "r_board", "v_gs_spec", "i_gss_q1", "kit_tol"])
def _(v_bus_6a, v_gs_spec, i_gss_q1, kit_tol):
    return (v_bus_6a - v_gs_spec) / (i_gss_q1 * (1 + kit_tol))


# ===========================================================================
# derived: the loads
# ===========================================================================
# The Teensy's regulator is linear, so whatever the 3V3 pin delivers comes out of VIN.
# The break beam's emitter takes its current from VIN behind D1 as well.
@fig("i_teensy_vin", "mA", group="LOADS", section=LOADS,
     rises_with=["i_teensy_40", "i_teensy_3v3", "i_f_max"])
def _(i_teensy_40, i_teensy_3v3, i_f_max):
    return i_teensy_40 + i_teensy_3v3 + i_f_max


# J-MAIN feeds the Teensy motherboard, where the amplifier module sits beside the Teensy.
@fig("i_main", "mA", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_teensy_40", "i_teensy_3v3", "i_f_max", "i_amp"])
def _(i_teensy_vin, i_amp):
    return i_teensy_vin + i_amp


# The D24V5F3 at the IR mainboard takes its input over J-IR's pair. Its input current
# at its lowest input voltage bounds the pair's drop, and the module's input behind that
# drop gives its current.
@fig("i_ir_max", "mA", group="LOADS", section=LOADS, prints="up", rises_with=["i_tot", "v_mod_max"],
     falls_with=["eta_u1", "v_mod_in_min"])
def _(i_tot, v_mod_max, eta_u1, v_mod_in_min):
    return i_tot * v_mod_max / (eta_u1 * v_mod_in_min)


@fig("v_ir_end", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_6a", "r_lead", "i_tot", "v_meas_f1"])
def _(v_bus_6a, i_ir_max, r_lead, v_f1):
    return v_bus_6a - 2 * i_ir_max * r_lead - v_f1


@fig("i_ir_in", "mA", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_tot", "v_mod_max", "i_supply_6a"], falls_with=["eta_u1", "v_jack_min"])
def _(i_tot, v_mod_max, eta_u1, v_ir_end):
    return i_tot * v_mod_max / (eta_u1 * v_ir_end)


@fig("i_permanent", "mA", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_teensy_40", "i_teensy_3v3", "i_amp", "i_tot", "i_disp"],
     falls_with=["eta_u1"])
def _(i_teensy_vin, i_amp, i_ir_in, i_disp):
    return i_teensy_vin + i_amp + i_ir_in + i_disp


# The most that draws at once: the lighting's cap, the permanent consumers and the coil
# case the solenoid model declares, two bumpers and the scoop. The servo moves only while
# no coil pulls, which the game holds to.
@fig("i_most_3a", "A", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_budget", "i_coils_at_once", "i_amp"])
def _(i_budget, i_coils_at_once, i_permanent):
    return i_budget + i_coils_at_once + i_permanent


@fig("i_most_6a", "A", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_budget_6a", "i_coils_at_once", "i_amp"])
def _(i_budget_6a, i_coils_at_once, i_permanent):
    return i_budget_6a + i_coils_at_once + i_permanent


# the lighting at its 6 A cap with two bumpers and the scoop pulling at once, past the adapter
@fig("i_over_6a", "A", group="LOADS", section=LOADS, prints="up",
     rises_with=["i_budget_6a", "i_coils_at_once", "i_amp", "i_disp"],
     falls_with=["i_supply_6a"])
def _(i_budget_6a, i_coils_at_once, i_permanent, i_supply_6a):
    return i_budget_6a + i_coils_at_once + i_permanent - i_supply_6a


# ===========================================================================
# derived: the bulk and the rail's ceiling
# ===========================================================================
@fig("c_total", "µF", group="RAIL", section=RAIL, rises_with=["c_one", "n_bulk"])
def _(c_one, n_bulk):
    return c_one * n_bulk


# The subsystem boards' bulk sits on the rail too, behind their pairs, and charges
# through Q1 at a live plug-in.
@fig("c_rail", "µF", group="RAIL", section=RAIL,
     rises_with=["c_one", "n_bulk", "n_bulk_light", "c_c91", "c_c1"])
def _(c_one, n_bulk, n_bulk_light, c_c91, c_c1):
    return c_one * (n_bulk + n_bulk_light) + c_c91 + c_c1


@fig("esr_bus", "mΩ", group="RAIL", section=RAIL, rises_with=["z_c"], falls_with=["n_bulk"])
def _(z_c, n_bulk):
    return z_c / n_bulk


# When the whole load stops at once, the cables' inductance drives its current on into
# the bulk: half L I squared into half C dV squared.
@fig("l_up_max", "µH", group="RAIL", section=RAIL,
     rises_with=["len_adapter_max", "len_dcin", "l_per_m"])
def _(len_adapter_max, len_dcin, l_per_m):
    return (len_adapter_max + len_dcin) * l_per_m


@fig("v_release_3a", "V", group="RAIL", section=RAIL, prints="up",
     rises_with=["v_jack_max", "i_supply_3a", "len_adapter_max", "l_per_m"],
     falls_with=["c_one", "n_bulk"])
def _(v_jack_max, i_supply_3a, l_up_max, c_total):
    return v_jack_max + i_supply_3a * (l_up_max / c_total) ** (1 / 2)


@fig("v_release_6a", "V", group="RAIL", section=RAIL, prints="up",
     rises_with=["v_jack_max", "i_supply_6a", "len_adapter_max", "l_per_m"],
     falls_with=["c_one", "n_bulk"])
def _(v_jack_max, i_supply_6a, l_up_max, c_total):
    return v_jack_max + i_supply_6a * (l_up_max / c_total) ** (1 / 2)


# The same with one capacitor fewer, which shows the count is the least that holds.
@fig("v_release_6a_less", "V", group="RAIL", section=RAIL, prints="up",
     rises_with=["v_jack_max", "i_supply_6a", "len_adapter_max", "l_per_m"],
     falls_with=["c_one", "n_bulk"])
def _(v_jack_max, i_supply_6a, l_up_max, c_one, n_bulk):
    return v_jack_max + i_supply_6a * (l_up_max / (c_one * (n_bulk - 1))) ** (1 / 2)


# Plugged in live, the adapter's charged output meets the empty bulk through the
# cables, a series RLC. Its damping is half the resistance times the root of C over L,
# least with the shortest cable. The sheet gives C1's and C2's resistance only as a
# maximum, so the damping leaves it out.
@fig("l_up_min", "µH", stated=False, rises_with=["len_adapter_min", "len_dcin", "l_per_m"])
def _(len_adapter_min, len_dcin, l_per_m):
    return (len_adapter_min + len_dcin) * l_per_m


@fig("r_up_min", "mΩ", group="RAIL", section=RAIL, prints="down",
     rises_with=["len_adapter_min", "r_awg_adapter", "len_dcin", "r_board"])
def _(len_adapter_min, r_awg_adapter, r_dcin, r_board):
    return 2 * len_adapter_min * r_awg_adapter + r_dcin + r_board


@fig("zeta", "", group="RAIL", section=RAIL, prints="down",
     rises_with=["r_awg_adapter", "r_board", "c_one"], falls_with=["l_per_m"])
def _(r_up_min, c_total, l_up_min):
    return r_up_min / 2 * (c_total / l_up_min) ** (1 / 2)


# The bulk of the whole rail charges through Q1's body diode until the rail passes Q1's
# gate threshold, and through its channel from there. The current of the overdamped RLC
# peaks where its two exponentials cross, and more bulk moves that peak up toward V/R;
# the diode's own drop and the boards' pairs, left out, would only lower it. Without
# overdamping, the resistance alone bounds the current.
@fig("i_inrush", "A", group="RAIL", section=RAIL, prints="up",
     rises_with=["v_jack_max", "c_one", "n_bulk", "n_bulk_light", "c_c91", "c_c1"],
     falls_with=["r_awg_adapter", "r_board", "l_per_m"])
def _(v_jack_max, r_up_min, l_up_min, c_rail):
    damp = r_up_min / (2 * l_up_min)
    spread2 = damp * damp - 1 / (l_up_min * c_rail)
    if spread2.v <= 0:
        return v_jack_max / r_up_min
    spread = spread2 ** (1 / 2)
    s_slow, s_fast = spread - damp, -spread - damp
    t_peak = ln(s_fast / s_slow) / (s_slow - s_fast)
    return v_jack_max / (l_up_min * (s_slow - s_fast)) \
        * (exp(s_slow * t_peak) - exp(s_fast * t_peak))


# ===========================================================================
# derived: F1
# ===========================================================================
# What passes F1 at most: every output behind it, the servo stalled and the IR mainboard's
# module at its lowest input, where it draws the most.
@fig("i_small", "A", group="FUSE", section=FUSE_SEC, prints="up",
     rises_with=["i_teensy_40", "i_teensy_3v3", "i_amp", "i_tot", "i_disp", "i_stall"],
     falls_with=["eta_u1", "v_mod_in_min"])
def _(i_teensy_vin, i_amp, i_ir_max, i_disp, i_stall):
    return i_teensy_vin + i_amp + i_ir_max + i_disp + i_stall


# The measured drop with its margin bounds the resistance up to the most behind F1.
@fig("r_f1", "mΩ", group="FUSE", section=FUSE_SEC, prints="up",
     rises_with=["v_meas_f1", "v_margin_f1"], falls_with=["i_meas_f1"])
def _(v_meas_f1, v_margin_f1, i_meas_f1):
    return (v_meas_f1 + v_margin_f1) / i_meas_f1


@fig("v_f1", "mV", group="FUSE", section=FUSE_SEC, prints="up",
     rises_with=["v_meas_f1", "v_margin_f1", "i_stall", "i_disp"], falls_with=["i_meas_f1"])
def _(r_f1, i_small):
    return r_f1 * i_small


@fig("i_f1_open", "A", group="FUSE", section=FUSE_SEC, prints="up", rises_with=["i_f1", "k_open"])
def _(i_f1, k_open):
    return i_f1 * k_open.raw


# The most a T2A may drop at its rating before the servo's buffer, the tightest floor behind
# F1, leaves its supply range.
@fig("v_drop_f1_max", "mV", group="FUSE", section=FUSE_SEC, prints="down",
     rises_with=["v_jack_min", "i_f1"], falls_with=["vcc_u1_min", "i_stall"])
def _(v_servo_6a, v_f1, vcc_u1_min, i_f1, i_small):
    return (v_servo_6a + v_f1 - vcc_u1_min) * i_f1 / i_small


# ===========================================================================
# derived: the floors the consumers see
# ===========================================================================
# F1 is taken fitted. Leaving it out only raises what the outputs behind it see.
# The lighting's own figures start from 5 V at the board. Behind the board the rail
# starts lower by the board's drop, and the bulk's resistance adds its share of the
# lighting's step.
@fig("v_light_3a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "v_buffer_min", "n_bulk"],
     falls_with=["i_supply_3a", "rdson_q1", "r_board", "z_c", "i_budget"])
def _(v_buffer_min, v_rail, v_bus_3a, i_budget, esr_bus):
    return v_buffer_min - (v_rail - v_bus_3a) - i_budget * esr_bus


@fig("v_light_6a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "v_buffer_min_6a", "n_bulk"],
     falls_with=["i_supply_6a", "rdson_q1", "r_board", "z_c", "i_budget_6a"])
def _(v_buffer_min_6a, v_rail, v_bus_6a, i_budget_6a, esr_bus):
    return v_buffer_min_6a - (v_rail - v_bus_6a) - i_budget_6a * esr_bus


# The servo's buffer behind the servo's pair while it stalls.
@fig("v_servo_3a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_3a", "rdson_q1", "r_board", "i_stall", "r_lead", "z_c",
                 "v_meas_f1"])
def _(v_bus_3a, i_stall, r_lead, esr_bus, v_f1):
    return v_bus_3a - 2 * i_stall * r_lead - i_stall * esr_bus - v_f1


@fig("v_servo_6a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_6a", "rdson_q1", "r_board", "i_stall", "r_lead", "z_c",
                 "v_meas_f1"])
def _(v_bus_6a, i_stall, r_lead, esr_bus, v_f1):
    return v_bus_6a - 2 * i_stall * r_lead - i_stall * esr_bus - v_f1


# The solenoids' coil board behind its pair while two bumpers and the scoop pull at once.
@fig("v_sol_3a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_3a", "rdson_q1", "r_board", "i_coils_at_once", "r_lead", "z_c"])
def _(v_bus_3a, i_coils_at_once, r_lead, esr_bus):
    return v_bus_3a - 2 * i_coils_at_once * r_lead - i_coils_at_once * esr_bus


@fig("v_sol_6a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_6a", "rdson_q1", "r_board", "i_coils_at_once", "r_lead", "z_c"])
def _(v_bus_6a, i_coils_at_once, r_lead, esr_bus):
    return v_bus_6a - 2 * i_coils_at_once * r_lead - i_coils_at_once * esr_bus


# The servo's start: C1 alone carries the first microseconds of the 800 mA step, before
# the current in the servo's pair follows, and U1 on the same board sees the dip.
@fig("v_servo_start_6a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min", "c_c1"], falls_with=["i_supply_6a", "i_stall", "z_c1", "l_feed",
                                                   "v_meas_f1"])
def _(v_bus_6a, i_stall, z_c1, l_feed, c_c1, v_f1):
    return v_bus_6a - v_f1 - i_stall * (z_c1 + (l_feed / c_c1) ** (1 / 2))


# The Teensy's VIN sits behind D1 on the motherboard. J-MAIN's pair carries the
# amplifier's current as well.
@fig("v_teensy_6a", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min"],
     falls_with=["i_supply_6a", "rdson_q1", "r_lead", "v_meas_f1", "v_f_diode", "i_amp"])
def _(v_bus_6a, i_main, r_lead, v_f1, v_f_diode):
    return v_bus_6a - 2 * i_main * r_lead - v_f1 - v_f_diode


# On USB alone, VIN takes the USB port's 5 V through D2 across the cut VUSB-VIN trace.
@fig("v_teensy_usb", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_usb_min"], falls_with=["v_f_diode"])
def _(v_usb_min, v_f_diode):
    return v_usb_min - v_f_diode


# U1 at the display sits at the end of J-DISP's pair
@fig("v_disp_in", "V", group="FLOORS", section=FLOORS, prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_6a", "rdson_q1", "r_lead", "i_disp",
                                           "v_meas_f1"])
def _(v_bus_6a, i_disp, r_lead, v_f1):
    return v_bus_6a - 2 * i_disp * r_lead - v_f1


# The Display section restates that floor in prose.
@fig("v_disp_in_prose", "V", group="Display", stated="loose", prints="down",
     rises_with=["v_jack_min"], falls_with=["i_supply_6a", "rdson_q1", "r_lead", "i_disp",
                                           "v_meas_f1"])
def _(v_disp_in):
    return v_disp_in



# ===========================================================================
# derived: what a replacement bulk part has to meet
# ===========================================================================
# The least bulk at J-IN that holds the 6 A release under VIN's maximum: the energy
# balance of the release, solved for C.
# the room between the adapter's no-load voltage and VIN's maximum
@fig("dv_release", "V", stated=False, rises_with=["v_in_max"], falls_with=["v_jack_max"])
def _(v_in_max, v_jack_max):
    return v_in_max - v_jack_max


@fig("c_total_min", "µF", group="ALLOW", section=ALLOW, prints="up",
     rises_with=["i_supply_6a", "len_adapter_max", "l_per_m"],
     falls_with=["v_in_max", "dv_release"])
def _(i_supply_6a, l_up_max, dv_release):
    return l_up_max * (i_supply_6a / dv_release) ** 2


# Each floor is linear in the part's impedance, which enters once at the board's own
# bulk and once more, divided by the count, at J-IN. The allowance is the impedance at
# which the floor touches its limit, taken with the 6 A adapter, where every floor is
# lowest. The lighting's floor is rebuilt from a lossless part first, since the
# lighting's own figure already carries the impedance.
@fig("z_allow_light", "mΩ", group="ALLOW", section=ALLOW, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_budget_6a", "vcc_min", "i_supply_6a", "rdson_q1", "r_board"])
def _(v_buffer_min_6a, z_c, i_budget_6a, v_rail, v_bus_6a, n_bulk, vcc_min):
    v_lossless = v_buffer_min_6a + i_budget_6a * z_c
    return (v_lossless - (v_rail - v_bus_6a) - vcc_min) * n_bulk / (i_budget_6a * (n_bulk + 1))


@fig("z_allow_sol", "mΩ", group="ALLOW", section=ALLOW, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_6a", "r_lead", "v_coil_min", "i_coils_at_once", "rdson_q1", "r_board"])
def _(v_bus_6a, i_coils_at_once, r_lead, v_coil_min, n_bulk):
    return (v_bus_6a - 2 * i_coils_at_once * r_lead - v_coil_min) * n_bulk / i_coils_at_once


@fig("z_allow_servo", "mΩ", group="ALLOW", section=ALLOW, prints="down",
     rises_with=["v_jack_min", "n_bulk"],
     falls_with=["i_supply_6a", "r_lead", "vcc_u1_min", "i_stall", "rdson_q1", "r_board"])
def _(v_bus_6a, i_stall, r_lead, vcc_u1_min, n_bulk):
    return (v_bus_6a - 2 * i_stall * r_lead - vcc_u1_min) * n_bulk / i_stall


# The least of the allowances is the ceiling a replacement part's listed impedance has
# to stay under.
@fig("z_allow", "mΩ", group="ALLOW", section=ALLOW, label="listed impedance", prints="down",
     rises_with=["v_jack_min", "n_bulk"], falls_with=["i_supply_6a", "rdson_q1", "r_board"])
def _(z_allow_light, z_allow_sol, z_allow_servo):
    return min(z_allow_light, z_allow_sol, z_allow_servo)


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("the input pair carries the 6 A adapter's whole current",
   lambda v: v.i_supply_6a <= v.i_awg16_max)
_I("a 22 AWG output pair carries the 6 A adapter's whole current",
   lambda v: v.i_supply_6a <= v.i_awg22_max)
_I("Q1's gate, which the bus drives, clears the voltage its R_DS(on) row is specified at",
   lambda v: v.v_bus_6a >= v.v_gs_spec)
_I("Q1's junction stays under the temperature its R_DS(on) bound is read at",
   lambda v: v.t_j_6a < v.t_hot)
_I("R1 stays under the most that still leaves Q1's gate at its specified drive",
   lambda v: v.r_gate <= v.r_gate_max)
_I("Q1's gate stays inside its rating at the highest rail",
   lambda v: v.v_release_6a < v.v_gs_max_q1)
_I("swapped polarity at J-IN stays inside Q1's drain-source rating",
   lambda v: v.v_jack_max < v.v_ds_max_q1)
_I("the plug-in current, with every bulk of the build on the rail, stays inside what "
   "Q1's package carries", lambda v: v.i_inrush < v.i_d_pkg)
_I("the bulk at J-IN is at least what the 6 A release needs",
   lambda v: v.c_total >= v.c_total_min)
_I("the part's impedance stays under what the floors allow",
   lambda v: v.z_c <= v.z_allow)
_I("a live plug-in leaves the rail without overshoot", lambda v: v.zeta >= 1)
_I("a load release of the 3 A adapter's whole current stays under the Teensy's VIN maximum",
   lambda v: v.v_release_3a <= v.v_in_max)
_I("a load release of the 6 A adapter's whole current stays under the Teensy's VIN maximum",
   lambda v: v.v_release_6a <= v.v_in_max)
_I("one capacitor fewer would not, so the count is the least that holds",
   lambda v: v.v_release_6a_less > v.v_in_max)
_I("C1 and C2 see at most 80 % of their rated voltage",
   lambda v: v.v_in_max <= v.v_c_rating * v.derate_elko)
_I("with the 3 A adapter, the lighting's buffers stay inside their supply range",
   lambda v: v.v_light_3a >= v.vcc_min)
_I("with the 6 A adapter, the lighting's buffers stay inside their supply range",
   lambda v: v.v_light_6a >= v.vcc_min)
_I("the servo's buffer stays inside its supply range while the servo stalls, with either "
   "adapter", lambda v: min(v.v_servo_3a, v.v_servo_6a) >= v.vcc_u1_min)
_I("the solenoids' coil board stays above what a stock coil kicks down to while two bumpers "
   "and the scoop pull, with either adapter", lambda v: min(v.v_sol_3a, v.v_sol_6a) >= v.v_coil_min)
_I("U1 stays inside its supply range through the servo's start",
   lambda v: v.v_servo_start_6a >= v.vcc_u1_min)
_I("the Teensy's VIN stays inside its range", lambda v: v.v_teensy_6a >= v.v_in_min)
_I("D1 carries the Teensy's current inside its rating", lambda v: v.i_teensy_vin < v.i_f_av)
_I("on USB alone, the Teensy's VIN stays inside its range",
   lambda v: v.v_teensy_usb >= v.v_in_min)
_I("on USB alone, D2 carries the Teensy's current inside its rating",
   lambda v: v.i_teensy_vin < v.i_f_av)
_I("on USB alone, a high-power port supplies the Teensy's whole current",
   lambda v: v.i_teensy_vin <= v.i_usb_max)
_I("the D24V5F3 at the IR mainboard gets more than its lowest input",
   lambda v: v.v_ir_end >= v.v_mod_in_min)
_I("the display's regulator gets the input it needs at full load",
   lambda v: v.v_disp_in >= v.v_in_mod_min)
_I("F1 carries the most the outputs behind it draw", lambda v: v.i_small <= v.i_f1)
_I("a fault the 6 A adapter feeds opens F1 within its maximum time",
   lambda v: v.i_f1_open <= v.i_supply_6a)
