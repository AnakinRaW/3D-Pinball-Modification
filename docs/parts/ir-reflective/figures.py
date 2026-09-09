"""The figures of the IR ball sensing design, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/ir-reflective/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.

`group`, `label` and `section` are how design.md addresses a quantity: the
group is the name in the first column of a code block or the first cell of a
table, the section the bold lead above it, the label a substring of the line.
`--groups` dumps what the parser found, which is how these are written.
"""
import pathlib

from figcheck import Model, Q, ceil_to, db, exp, floor_to, interp_log, ln, log10

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("ir-reflective", HERE / "design.md",
              section=None, until="## Sources",
              drawings=[HERE / "ir-sensor-mainboard.svg",
                        HERE / "pulsed-schematic.svg"])

def _to_printed(q, unit, decimals):
    """A quantity as the appendix prints it, so a chain of stated figures adds
    up the way the document does."""
    return Q.of(round(q.to(unit), decimals), unit)


ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
gr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="graph", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
fig = MODEL.derived

SENSOR = "IR-reflective-gp2s700hcp_e.pdf"
ADC = "MCP3004-3008-Microchip.pdf"
ISO = "ISO7761-TI.pdf"
MOSFET = "AO3400A-AOS.pdf"
REG = "ISL85415-Renesas.pdf"
POLOLU = "D24V5Fx-Pololu-schematic.pdf"

# the appendix sections, as the parser reads their bold leads
BASE = "Base quantities"
IFIXED = "I_fixed, the board's draw beside the emitters and the channel nodes"
DISS = "Dissipation, one line per part"
R220 = "220 Ω, LED series resistor"
DROOP = "Phase-start droop"
CONST = "The constant is measured, not derived"
CONV = "The converter, what the part has to do"
ACQ = "Acquisition, and why no capacitor sits at a converter input"
READ = "The read block and the phase"
GATE = "2 kΩ, gate resistor"
PD = "4.7 kΩ, signal pull-down"
PCT47 = "The 47 % is accepted, not fixed by a longer phase"
PIN1 = "Why Pin 1 needs no external resistor"
KNOB = "Bounds on the adjustment knobs"
SWEEP = "The pull-down's upper bound"
PI = Q(3.141592653589793)

# ===========================================================================
# decisions: the values this design picked
# ===========================================================================
dec("n_channels", 16, "", src="sixteen positions, the case every figure is derived at")
dec("n_stock", 3, "", src="the three sensors the stock machine fits")
dec("n_eight", 8, "", src="eight positions, one converter fitted")
dec("budget_grain", 10, "µs", src="the step the firmware rounds the read block up to, so a phase holds whole tens")
dec("n_contacts", 4, "", src="two connectors on the J-PWR cable, each with a contact on both conductors")
dec("switch_r_factor", 3, "", src="three times the converter's switch resistance, the what-if the acquisition window is tested against")
dec("phases_per_two_values", 5, "", src="two values span five phases: a value spans three and consecutive values share the middle one")
dec("r_led", 220, "Ω", src="E12 value inside the knob bounds derived below")
dec("r_pd", 4.7, "kΩ", src="E12 value inside the knob bounds derived below")
dec("r_gate", 2, "kΩ", src="R33, E12 value inside the gate bounds")
dec("r_gate_pd", 100, "kΩ", src="R35, E12 value inside the gate bounds")
dec("r_miso", 2, "kΩ", src="R34, the same value as R33")
dec("r_dout", 1, "kΩ", src="R37 and R38")
dec("r_dout_pu", 10, "kΩ", src="R36")
dec("r_ref", 47, "Ω", src="R39, puts the filter corner two decades under the burst rate")
dec("c_ref", 22, "µF", src="C6, printed value")
dec("c_entry", 22, "µF", src="C5, printed value")
dec("c_decoupling", 100, "nF", src="C1 to C4, one at each supply pin")
dec("t_phase", 600, "µs", src="the phase, sitting on the dwell bound derived below")
dec("v_mod_nom", 3.3, "V", group="V_OUT", section=BASE,
    stated=True, src="the D24V5F3 variant chosen for the 3.3 V rail")
dec("working_margin", 0.5, "%", group="V_OUT", section=BASE,
    stated=True, src="carried on top of the module maximum as the working bound")
dec("bits", 10, "", src="the MCP3008's resolution")
dec("i_teensy_3v3", 250, "mA", src="PJRC pin assignment card 11a rev4, the 3.3 V rail "
    "available to external circuits")
dec("r_led_e12_below", 47, "Ω", src="the E12 value above the single-channel bound")
dec("r_led_alt_low", 100, "Ω", src="one standard value above the single-channel bound")
dec("r_led_alt_high", 390, "Ω", src="one standard value below the characterising point")
dec("r_pd_alt", 2.2, "kΩ", src="one standard value, twice the resolution floor")
dec("r_pd_alt_high", 10, "kΩ", src="the pull-down one decade up, weighed in the "
    "sweep below")
dec("r_gate_pd_low", 3, "kΩ", src="the gate pull-down weighed and rejected on the "
    "threshold")
dec("r_gate_pd_mid", 10, "kΩ", src="the next gate pull-down up, also weighed")
dec("l_bead_alt", 10, "µH", src="the inductor weighed against R39")
dec("phase_alt", 750, "µs", src="the next phase up, weighed against the dwell")
dec("r_led_e12_0805", 82, "Ω", src="the E12 value an 0805 carries at half its rating")

# ===========================================================================
# assumptions: no datasheet gives these
# ===========================================================================
asm("t_a", 40, "°C", group="T_A", section=BASE, stated=True,
    src="an open frame with no heat source nearby, on a hot day")
asm("i_photo", 50, "µA", src="the ball signal; measurement B replaces it")
asm("rds_irl_est", 0.1, "Ω", src="an estimate above the 4.0 V figure, since the "
    "IRL540N specifies nothing at 3.3 V")
asm("cable_r", 0.1, "Ω", src="the round build figure for the J-PWR cable; it costs "
    "0.2 mV more than the allowance, inside the millivolt the rail is printed at")
dec("cable_drop_bound", 18, "mV", group="V_OUT", section=BASE, stated=True,
    src="the drop the rail minimum leaves for the cable, from which the 0.1 Ω follows")
asm("contact_r", 20, "mΩ", src="one crimped 2.54 mm contact")
asm("awg28_per_m", 0.20, "Ω", src="round figure for 28 AWG per metre, one conductor")
asm("f_c_pess", 20, "kHz", src="pessimistic reading of the module's crossover, against "
    "the 75 kHz of the FN8373.2 worked example")
asm("f_c_worst", 10, "kHz", src="the crossover the module's own capacitance is quoted at")
asm("t_ovh", 2, "µs", src="reserved per conversion for the firmware's own overhead")
asm("t_jitter", 10, "µs", src="reserved for the timer's start jitter")
asm("stray_dout", 15, "pF", src="stray at the ADC-DOUT net")
asm("stray_miso", 20, "pF", src="stray at the MISO conductor and the Teensy pin")
asm("derate", 50, "%", src="ceramic capacitance kept under DC bias, as FN8373.2 itself "
    "advises")
asm("vcc2_overshoot", 70, "mV", src="load-release overshoot on VCC2, the order of the "
    "droop")
asm("pulse_overshoot_module", 23, "mV", src="Q_pulse / C on the module's capacitance "
    "alone")
asm("burst_freq_low", 9, "kHz", src="the low end of the power-save burst rate at the "
    "dark-phase load")
asm("further_drop", 17, "mV", src="one step of further cable drop past the bound")
asm("cable_length", 5, "cm", src="the run from the module to J-PWR the bound is "
    "taken at")
dec("one_metre", 1, "m", src="the length awg28_per_m is quoted per")
asm("rds_hypothetical", 1, "Ω", src="the R_DS(on) the IRL540N estimate is tested "
    "against, an order of magnitude above it")
asm("esr_electrolytic", 2, "Ω", src="the ESR an aluminium electrolytic would bring "
    "in place of C5")

# ===========================================================================
# measured on the stock machine
# ===========================================================================
msr("r_col", 1.585, "kΩ", src="the collector load on the stock P33 sensor board, read "
    "with a meter, research/Rokr/2_ir-reflective-sensor-p33.md")
msr("dwell", 3, "ms", src="the stock machine's emitter period; it needs both windows of "
    "that period, so a ball it catches dwells at least one full period")
msr("sag_three_coils", 3.75, "V", src="the machine's 5 V with three bumper coils firing "
    "and a poor supply cable, research/Rokr/3_bumper-control.md")
msr("vf_diode_range", 1.082, "V", src="the stock board read in a meter's diode range, "
    "which sits on the 25 °C curve at the meter's test current")
msr("i_meter", 0.86, "mA", src="the meter's diode-range test current, from the 1.365 V "
    "it showed across the measured 1.585 kΩ")

# ===========================================================================
# datasheet: Pololu D24V5F3 and the ISL85415 on it
# ===========================================================================
ds("mod_tol", 4, "%", group="V_OUT", section=BASE, stated=True,
   src="Pololu D24V5F3 product page 2842, output voltage 3.3 V within 4 %")
ds("i_module", 500, "mA", src="Pololu D24V5F3, maximum output current")
ds("v_mod_in_min", 3.4, "V", src="Pololu D24V5F3, minimum input voltage")
ds("l_mod", 22, "µH", src="D24V5F3 inductor", sheet=POLOLU)
ds("f_sw", 500, "kHz", src="D24V5F3 switching frequency", sheet=POLOLU)
ds("c_module_printed", 10, "µF", src="two 10 µF on VOUT", sheet=POLOLU)
ds("psm_band", 1, "%", src="FN8373.2, the power-save comparator holds the output in a "
   "band of 1 %", sheet=REG)
ds("psm_ripple", 40, "mV", src="FN8373.2 Figure 45, about 40 mV peak to peak at 20 mA",
   sheet=REG)
ds("psm_burst_ripple", 44, "mV", src="FN8373.2, the burst band at the dark-phase load",
   sheet=REG)
ds("psm_burst_current", 300, "mA", src="FN8373.2, the pulses a burst is made of",
   sheet=REG)
ds("f_c_example", 75, "kHz", src="FN8373.2, the worked example for external compensation "
   "reaches 75 kHz", sheet=REG)
ds("f_c_ceiling", 100, "kHz", src="FN8373.2 keeps the crossover under 100 kHz", sheet=REG)
ds("transient_psm", 160, "mV", src="FN8373.2 Figures 47 and 48, a 500 mA step from "
   "power-save", sheet=REG)
ds("transient_pwm", 95, "mV", src="FN8373.2 Figures 47 and 48, the same step from PWM",
   sheet=REG)
ds("transient_step", 500, "mA", src="FN8373.2 Figures 47 and 48, the step size", sheet=REG)
ds("transient_freq", 800, "kHz", src="FN8373.2 Figures 47 and 48, the switching "
   "frequency of those figures", sheet=REG)
ds("recovery", 200, "µs", src="FN8373.2 Figures 47 and 48, recovered within 200 µs",
   sheet=REG)
ds("psm_burst_period_ds", 80, "µs", src="FN8373.2 Figure 45, a burst every 80 µs at "
   "20 mA", sheet=REG)
ds("v_in_machine", 5, "V", src="the machine's rail, the ROKR EG01 5 V adapter input")

# ===========================================================================
# datasheet: Sharp GP2S700HCP, D3-A02201EN
# ===========================================================================
_C25 = "GP2S700HCP D3-A02201EN Figure 3, V_F against I_F, 25 °C curve"
_C75 = "GP2S700HCP D3-A02201EN Figure 3, V_F against I_F, 75 °C curve"
for _k, _i, _v in [("vf25_1ma", 1, 1.09), ("vf25_10ma", 10, 1.25),
                   ("vf25_20ma", 20, 1.32), ("vf25_50ma", 50, 1.46)]:
    gr(_k, _v, "V", src=_C25, sheet=SENSOR)
    gr(_k + "_at", _i, "mA", src=_C25, sheet=SENSOR)
gr("vf75_10ma", 1.18, "V", src=_C75, sheet=SENSOR)
gr("vf75_10ma_at", 10, "mA", src=_C75, sheet=SENSOR)
gr("vf75_50ma", 1.38, "V", src=_C75, sheet=SENSOR)
gr("vf75_50ma_at", 50, "mA", src=_C75, sheet=SENSOR)
ds("t_curve_25", 25, "°C", src="GP2S700HCP D3-A02201EN Figure 3, the lower curve's "
   "temperature", sheet=SENSOR)
ds("t_curve_75", 75, "°C", src="GP2S700HCP D3-A02201EN Figure 3, the upper curve's "
   "temperature", sheet=SENSOR)
gr("vf_nominal_read", 1.24, "V", src="GP2S700HCP D3-A02201EN Figure 3, the 25 °C curve "
   "read at the 9.3 mA the nominal case settles at", sheet=SENSOR)
ds("vf_table_max", 1.4, "V", src="GP2S700HCP D3-A02201EN, V_F maximum at I_F = 20 mA",
   sheet=SENSOR)
ds("vf_table_typ", 1.2, "V", src="GP2S700HCP D3-A02201EN, V_F typical at I_F = 20 mA",
   sheet=SENSOR)
ds("i_f_max", 50, "mA", src="GP2S700HCP D3-A02201EN, absolute maximum forward current",
   sheet=SENSOR)
ds("i_c_abs_max", 20, "mA", src="GP2S700HCP D3-A02201EN, absolute maximum collector "
   "current", sheet=SENSOR)
ds("i_f_char", 4, "mA", src="GP2S700HCP D3-A02201EN, the forward current I_C is "
   "characterised at", sheet=SENSOR)
ds("i_c_ds_min", 60, "µA", src="GP2S700HCP D3-A02201EN, I_C minimum at I_F = 4 mA, "
   "V_CE = 2 V, d = 4 mm against an aluminium mirror", sheet=SENSOR)
ds("i_c_ds_max", 410, "µA", src="GP2S700HCP D3-A02201EN, I_C maximum at the same "
   "condition", sheet=SENSOR)
ds("v_ce_char", 2, "V", src="GP2S700HCP D3-A02201EN, the V_CE I_C is characterised at",
   sheet=SENSOR)
ds("t_rf_max_1k", 100, "µs", src="GP2S700HCP D3-A02201EN, t_r and t_f maximum at "
   "R_L = 1 kΩ", sheet=SENSOR)
ds("t_rf_typ_1k", 20, "µs", src="GP2S700HCP D3-A02201EN, t_r and t_f typical at the same "
   "load", sheet=SENSOR)
ds("rise_low", 10, "%", src="GP2S700HCP D3-A02201EN, the lower end of the rise-time band",
   sheet=SENSOR)
ds("rise_high", 90, "%", src="GP2S700HCP D3-A02201EN, the upper end", sheet=SENSOR)
gr("fig6_1k", 52, "µs", src="GP2S700HCP D3-A02201EN Figure 6, response time at 1 kΩ",
   sheet=SENSOR)
gr("fig6_4k7", 71, "µs", src="GP2S700HCP D3-A02201EN Figure 6, read at 4.7 kΩ",
   sheet=SENSOR)
gr("fig6_10k", 94, "µs", src="GP2S700HCP D3-A02201EN Figure 6, at 10 kΩ", sheet=SENSOR)
gr("i_c_at_40c", 92, "%", src="GP2S700HCP D3-A02201EN Figure 4, collector current "
   "against ambient temperature", sheet=SENSOR)
gr("i_c_at_25c", 100, "%", src="GP2S700HCP D3-A02201EN Figure 4 normalises I_C to its "
   "value at 25 °C, which is the curve's anchor", sheet=SENSOR)
gr("fig6_1k_at", 1, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis",
   sheet=SENSOR)
gr("fig6_4k7_at", 4.7, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis",
   sheet=SENSOR)
gr("fig6_10k_at", 10, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis",
   sheet=SENSOR)

# ===========================================================================
# datasheet: AO3400A and IRL540N
# ===========================================================================
ds("rds_ao", 48, "mΩ", src="AO3400A, R_DS(on) maximum at V_GS = 2.5 V and I_D = 3 A",
   sheet=MOSFET)
ds("v_gs_test", 2.5, "V", src="AO3400A, the gate voltage R_DS(on) is specified at",
   sheet=MOSFET)
ds("rds_ao_10v_25", 26.5, "mΩ", src="AO3400A, the 10 V row at 25 °C", sheet=MOSFET)
ds("rds_ao_10v_125", 38, "mΩ", src="AO3400A, the same row at T_J = 125 °C", sheet=MOSFET)
ds("t_j_hot", 125, "°C", src="AO3400A, the junction temperature of that row", sheet=MOSFET)
ds("qg_ao", 7, "nC", src="AO3400A, Q_g maximum at V_GS = 4.5 V and I_D = 5.7 A",
   sheet=MOSFET)
ds("vgs_th_ao_max", 1.45, "V", src="AO3400A, V_GS(th) maximum", sheet=MOSFET)
ds("vgs_th_ao_min", 0.65, "V", src="AO3400A, V_GS(th) minimum", sheet=MOSFET)
ds("vgs_rating_ao", 12, "V", src="AO3400A, V_GS rating", sheet=MOSFET)
ds("qg_irl", 74, "nC", src="IRL540N, Q_g maximum at V_GS = 5.0 V and I_D = 18 A")
ds("vgs_th_irl_max", 2.0, "V", src="IRL540N, V_GS(th) maximum")
ds("vgs_th_irl_min", 1.0, "V", src="IRL540N, V_GS(th) minimum")
ds("vgs_rating_irl", 16, "V", src="IRL540N, the V_GS I_GSS is specified at")
ds("i_gss", 100, "nA", src="both parts give I_GSS as 100 nA maximum")
ds("r_leak_test", 1, "MΩ", src="the resistance the gate-leakage bound is taken at")
ds("p_q1_rating", 1.4, "W", src="AO3400A, power dissipation at 25 °C free air",
   sheet=MOSFET)
ds("p_0805", 125, "mW", src="the 25 °C free-air rating of an 0805 thick-film resistor")
ds("gate_charge_limit", 100, "µs", src="the gate edge the phase tolerates")

# ===========================================================================
# datasheet: MCP3008, DS21295D
# ===========================================================================
ds("f_clk", 1.35, "MHz", src="DS21295D, the clock at VDD = 2.7 V", sheet=ADC)
ds("f_clk_5v", 3.6, "MHz", src="DS21295D, the clock at VDD = 5 V", sheet=ADC)
ds("clocks_protocol", 17, "clocks", src="DS21295D section 5.0", sheet=ADC)
ds("clocks_frame", 24, "clocks", src="DS21295D section 6.1, three whole bytes", sheet=ADC)
ds("acq_clocks", 1.5, "clocks", src="DS21295D, the acquisition window", sheet=ADC)
ds("c_pin", 7, "pF", src="DS21295D Figure 4-1, the pad capacitance", sheet=ADC)
ds("c_sample", 20, "pF", src="DS21295D Figure 4-1, the sample capacitor", sheet=ADC)
ds("r_switch", 1, "kΩ", src="DS21295D Figure 4-1, the switch resistance, typical",
   sheet=ADC)
ds("t_do_adc", 200, "ns", src="DS21295D, t_DO at VDD = 2.7 V", sheet=ADC)
ds("t_dis_adc", 100, "ns", src="DS21295D, t_DIS maximum", sheet=ADC)
ds("t_setup_adc", 50, "ns", src="DS21295D, setup and hold", sheet=ADC)
ds("i_adc_idd", 550, "µA", src="DS21295D, IDD maximum", sheet=ADC)
ds("i_adc_vref", 150, "µA", src="DS21295D, the VREF drain", sheet=ADC)
ds("v_ol_adc", 0.4, "V", src="DS21295D, V_OL maximum at I_OL = 1 mA and VDD = 4.5 V",
   sheet=ADC)
ds("i_ol_adc", 1, "mA", src="DS21295D, the I_OL that V_OL is specified at", sheet=ADC)
ds("vdd_adc_at_vol", 4.5, "V", src="DS21295D, the VDD that V_OL is specified at",
   sheet=ADC)
ds("vdd_floor", 2.7, "V", src="DS21295D, minimum VDD", sheet=ADC)
ds("vdd_ceiling", 5.5, "V", src="DS21295D, maximum VDD", sheet=ADC)
ds("vref_floor", 0.25, "V", src="DS21295D, minimum VREF", sheet=ADC)
ds("adc_abs_margin", 0.6, "V", src="DS21295D, inputs and outputs rated to VDD + 0.6 V",
   sheet=ADC)

# ===========================================================================
# datasheet: ISO7761F, SLLSER1H
# ===========================================================================
ds("i_iso_s1", 10.3, "mA", src="SLLSER1H, the 3.3 V DC row, side 1, every input at its "
   "own supply", sheet=ISO)
ds("i_iso_s2", 6.9, "mA", src="SLLSER1H, the same row, side 2", sheet=ISO)
ds("i_iso_ac_s1_1m", 6.5, "mA", src="SLLSER1H, side 1 at 1 Mbps", sheet=ISO)
ds("i_iso_ac_s2_1m", 6.3, "mA", src="SLLSER1H, side 2 at 1 Mbps", sheet=ISO)
ds("i_iso_ac_s1_10m", 7.5, "mA", src="SLLSER1H, side 1 at 10 Mbps", sheet=ISO)
ds("i_iso_ac_s2_10m", 8.6, "mA", src="SLLSER1H, side 2 at 10 Mbps", sheet=ISO)
ds("i_out_iso", 2, "mA", src="SLLSER1H, the I_OH the outputs are specified at", sheet=ISO)
ds("t_pd_iso", 18.5, "ns", src="SLLSER1H, propagation delay maximum at 3.3 V ± 10 % over "
   "the full temperature range", sheet=ISO)
ds("skew_iso", 4.4, "ns", src="SLLSER1H, channel-to-channel skew maximum", sheet=ISO)
ds("r_in_iso", 1.5, "MΩ", src="SLLSER1H Figure 7-3, the internal input pull-down",
   sheet=ISO)
ds("uvlo_lo", 1.7, "V", src="SLLSER1H, the lower undervoltage lockout threshold",
   sheet=ISO)
ds("uvlo_hi", 2.25, "V", src="SLLSER1H, the upper threshold", sheet=ISO)
ds("v_il_factor", 30, "%", src="SLLSER1H, V_IL guaranteed at 0.3 × VCC", sheet=ISO)
ds("iso_rate_lo", 1, "MHz", src="SLLSER1H, the lower AC row", sheet=ISO)
ds("iso_rate_hi", 10, "MHz", src="SLLSER1H, the upper AC row", sheet=ISO)

# ===========================================================================
# base quantities
# ===========================================================================
fig("v_mod_min", "V", group="V_OUT", section=BASE)(
    lambda v_mod_nom, mod_tol: v_mod_nom * (1 - mod_tol.raw))
fig("v_mod_max", "V", group="V_OUT", section=BASE)(
    lambda v_mod_nom, mod_tol: v_mod_nom * (1 + mod_tol.raw))


@fig("cable_drop", "mV", group="V_OUT", section=BASE,
     rises_with=["cable_r", "i_led_worst"])
def _(cable_r, i_tot):
    return cable_r * i_tot


@fig("v_out_min", "V", group="V_OUT", section=BASE,
     rises_with=["v_mod_nom"],
     falls_with=["mod_tol", "cable_r"])
def _(v_mod_min, cable_drop):
    return v_mod_min - cable_drop


@fig("v_out_max", "V", group="V_OUT", section=BASE,
     rises_with=["v_mod_nom", "mod_tol", "working_margin"])
def _(v_mod_max, working_margin):
    return v_mod_max * (1 + working_margin.raw)


def _vf75(vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, i):
    """The 75 °C curve of Figure 3, between the two points it publishes."""
    return interp_log([(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)], i)


@fig("v_f_75c_at_50ma", "V", stated=False)
def _(vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, i_f_max):
    return _vf75(vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, i_f_max)


@fig("rds_ao_hot_factor", "×", stated=False)
def _(rds_ao_10v_25, rds_ao_10v_125):
    return rds_ao_10v_125 / rds_ao_10v_25


@fig("rds_ao_hot", "mΩ", group="Q1", section=DISS)
def _(rds_ao, rds_ao_hot_factor):
    return rds_ao * rds_ao_hot_factor


@fig("v_ds_ao", "mV", group="V_DS", section=BASE)
def _(i_bus, rds_ao):
    return i_bus * rds_ao


@fig("v_ds_ao_hot", "mV", group="V_DS", section=BASE)
def _(i_bus, rds_ao_hot):
    return i_bus * rds_ao_hot


@fig("v_ds_irl", "mV", group="V_DS", section=BASE)
def _(i_bus, rds_irl_est):
    return i_bus * rds_irl_est


@fig("u_r_nominal", "V", group="U_R", section=BASE,
     rises_with=["v_mod_nom"],
     falls_with=["vf_nominal_read"])
def _(v_mod_nom, vf_nominal_read, v_ds_ao):
    # the appendix carries V_DS at the tenth of a millivolt it prints
    return v_mod_nom - vf_nominal_read - _to_printed(v_ds_ao, "mV", 1)


@fig("u_r_most", "V", group="U_R", section=BASE,
     rises_with=["v_out_max"],
     falls_with=["vf75_10ma"])
def _(v_out_max, vf75_10ma):
    return v_out_max - vf75_10ma


@fig("u_r_least", "V", group="U_R", section=BASE,
     rises_with=["v_out_min"],
     falls_with=["vf_table_max", "rds_irl_est"])
def _(v_out_min, vf_table_max, v_ds_irl):
    # the appendix carries V_DS at the millivolt it prints
    return v_out_min - vf_table_max - _to_printed(v_ds_irl, "mV", 0)


@fig("i_led_nominal", "mA", group="I_LED", section=BASE,
     rises_with=["v_mod_nom"],
     falls_with=["vf_nominal_read", "r_led"])
def _(u_r_nominal, r_led):
    return u_r_nominal / r_led


@fig("i_led_worst", "mA", group="I_LED", section=BASE,
     rises_with=["v_out_max"],
     falls_with=["vf75_10ma", "r_led"])
def _(u_r_most, r_led):
    return u_r_most / r_led


@fig("i_led_least", "mA", group="I_LED", section=BASE,
     rises_with=["v_out_min"],
     falls_with=["vf_table_max", "r_led"])
def _(u_r_least, r_led):
    return u_r_least / r_led


@fig("i_c", "mA", group="I_C", section=BASE,
     rises_with=["v_out_max"],
     falls_with=["r_col", "r_pd"])
def _(v_out_max, r_col, r_pd):
    return v_out_max / (r_col + r_pd)


@fig("i_adc_one", "mA", group="I_ADC", section=BASE)
def _(i_adc_idd, i_adc_vref):
    return i_adc_idd + i_adc_vref


@fig("i_adc_both", "mA", group="I_ADC", section=BASE)
def _(i_adc_one):
    return 2 * i_adc_one


@fig("i_bus", "mA", group="I_BUS", section=BASE,
     rises_with=["n_channels", "v_out_max"],
     falls_with=["r_led"])
def _(n_channels, i_led_worst):
    return n_channels.raw * i_led_worst


@fig("i_tot", "mA", group="I_TOT", section=BASE,
     rises_with=["i_led_worst", "n_channels", "i_adc_idd"],
     falls_with=["r_led"])
def _(i_bus, n_channels, i_c, i_adc_both, i_iso_s2):
    return i_bus + n_channels.raw * i_c + i_adc_both + i_iso_s2


@fig("i_tot_eight", "mA", group="I_TOT", section=BASE)
def _(i_led_worst, i_c, i_adc_one, i_iso_s2, n_eight):
    return n_eight.raw * i_led_worst + n_eight.raw * i_c + i_adc_one + i_iso_s2


@fig("i_tot_bench", "mA", group="I_TOT", section=BASE,
     rises_with=["i_iso_s1"],
     falls_with=["r_led"])
def _(i_tot, i_iso_s1):
    return i_tot + i_iso_s1


@fig("i_dark", "mA", stated=False)
def _(n_channels, i_c, i_adc_both, i_iso_s2):
    return n_channels.raw * i_c + i_adc_both + i_iso_s2


# ===========================================================================
# I_fixed
# ===========================================================================
@fig("i_fixed_module", "mA", group="from the module, side 1 still on the Teensy",
     section=IFIXED)
def _(i_adc_both, i_iso_s2):
    return i_adc_both + i_iso_s2


@fig("i_fixed_teensy", "mA",
     group="from the Teensy's 3V3 pin, both isolator sides out of it",
     section=IFIXED)
def _(i_adc_both, i_iso_s1, i_iso_s2):
    return i_adc_both + i_iso_s1 + i_iso_s2


@fig("left_teensy", "mA", group=IFIXED, stated="loose")
def _(i_teensy_3v3, i_fixed_teensy, n_channels, i_c):
    return i_teensy_3v3 - i_fixed_teensy - n_channels.raw * i_c


@fig("left_module", "mA", group=IFIXED, stated="loose")
def _(i_module, i_fixed_module, n_channels, i_c):
    return i_module - i_fixed_module - n_channels.raw * i_c


# ===========================================================================
# dissipation, one line per part
# ===========================================================================
@fig("p_q1", "mW", group="Q1", section=DISS,
     rises_with=["i_led_worst", "rds_ao"],
     falls_with=["r_led"])
def _(i_bus, rds_ao_hot):
    return i_bus ** 2 * rds_ao_hot


@fig("p_q1_pct", "%", group="Q1", section=DISS)
def _(p_q1, p_q1_rating):
    return p_q1 / p_q1_rating


@fig("p_r_led", "mW", group="220 Ω", section=DISS,
     rises_with=["v_out_max"],
     falls_with=["r_led"])
def _(i_led_worst, r_led):
    return i_led_worst ** 2 * r_led


@fig("p_r_led_pct", "%", group="220 Ω", section=DISS)
def _(p_r_led, p_0805):
    return p_r_led / p_0805


@fig("u_node_max", "V", group="4.7 kΩ", section=DISS,
     rises_with=["v_out_max", "r_pd"],
     falls_with=["r_col"])
def _(v_out_max, r_pd, r_col):
    return v_out_max * r_pd / (r_col + r_pd)


@fig("p_r_pd", "mW", group="4.7 kΩ", section=DISS)
def _(u_node_max, r_pd):
    return u_node_max ** 2 / r_pd


@fig("p_r_pd_pct", "%", group="4.7 kΩ", section=DISS)
def _(p_r_pd, p_0805):
    return p_r_pd / p_0805


@fig("i_dout_low", "mA", stated=False)
def _(v_out_max, r_dout, r_dout_pu):
    return v_out_max / (r_dout + r_dout_pu)


@fig("p_r36", "mW", group="R36", section=DISS)
def _(i_dout_low, r_dout_pu):
    return i_dout_low ** 2 * r_dout_pu


@fig("p_r36_pct", "%", group="R36", section=DISS)
def _(p_r36, p_0805):
    return p_r36 / p_0805


@fig("v_gate_max", "V", group="R35", section=DISS,
     rises_with=["v_out_max", "r_gate_pd"],
     falls_with=["r_gate"])
def _(v_out_max, r_gate_pd, r_gate):
    return v_out_max * r_gate_pd / (r_gate_pd + r_gate)


@fig("p_r35", "µW", group="R35", section=DISS)
def _(v_gate_max, r_gate_pd):
    return v_gate_max ** 2 / r_gate_pd


@fig("p_r37", "µW", group="R37, R38", section=DISS)
def _(i_dout_low, r_dout):
    return i_dout_low ** 2 * r_dout


@fig("p_r39", "µW", group="R39", section=DISS)
def _(i_adc_both, r_ref):
    return i_adc_both ** 2 * r_ref


@fig("i_gate_divider", "µA", group="R33", section=DISS)
def _(v_mod_nom, r_gate_pd):
    return v_mod_nom / r_gate_pd


@fig("p_r33", "µW", group="R33", section=DISS)
def _(i_gate_divider, r_gate):
    return i_gate_divider ** 2 * r_gate


# ===========================================================================
# 220 Ω, the emitter series resistor
# ===========================================================================
@fig("p_r_led_rounded", "mW", group="P", section=R220)
def _(p_r_led):
    return p_r_led


@fig("p_0805_over_p_r_led", "×", group="P", section=R220)
def _(p_0805, p_r_led):
    return p_0805 / p_r_led


@fig("i_at_e12_below", "mA", group="P", section=R220)
def _(u_r_at_i_f_max, r_led_e12_below):
    return u_r_at_i_f_max / r_led_e12_below


@fig("p_at_e12_below", "mW", group="P", section=R220)
def _(i_at_e12_below, r_led_e12_below):
    return i_at_e12_below ** 2 * r_led_e12_below


@fig("p_at_e12_below_pct", "%", group="P", section=R220)
def _(p_at_e12_below, p_0805):
    return p_at_e12_below / p_0805


def _i_led_at(r, v_rail, curve):
    """The emitter current through r, V_F read at the current it settles at.

    The seed is any current inside the curve; the loop leaves it behind.
    """
    i = curve[0][0]
    for _ in range(60):
        i = (v_rail - interp_log(curve, i)) / r
    return i


def _bisect_r(target_power, v_rail, curve, lo, hi):
    """The resistor at which the emitter branch dissipates target_power."""
    for _ in range(60):
        mid = (lo + hi) / 2
        if _i_led_at(mid, v_rail, curve) ** 2 * mid > target_power:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


@fig("r_at_half_0805", "Ω", group="P", section=R220,
     rises_with=["v_out_max"], falls_with=["p_0805"],
     prints="up")
def _(v_out_max, vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, p_0805,
      r_min_single, r_led_alt_high):
    # The emitter branch reaches half an 0805's rating at this resistor. V_F
    # moves with the current, so the resistance is bisected between the
    # single-channel bound and the characterising point.
    curve = [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)]
    return _bisect_r(p_0805 / 2, v_out_max, curve, r_min_single, r_led_alt_high)


@fig("i_f_max_over_worst", "×", group=R220, stated="loose")
def _(i_f_max, i_led_worst):
    return i_f_max / i_led_worst


# ===========================================================================
# Q1 and the IRL540N
# ===========================================================================
_AO = "Q1 is an AO3400A"
_IRL = "The IRL540N is the through-hole variant"


@fig("v_gate_min", "V", group=_AO, stated="loose")
def _(v_out_min, r_gate_pd, r_gate):
    return v_out_min * r_gate_pd / (r_gate_pd + r_gate)


@fig("gate_over_th_ao", "V", group=_AO, stated="loose")
def _(v_gate_min, vgs_th_ao_max):
    return v_gate_min - vgs_th_ao_max


@fig("v_ds_at_1ohm", "mV", group=_IRL, stated="loose")
def _(i_bus, rds_hypothetical):
    return i_bus * rds_hypothetical


@fig("v_ds_at_1ohm_pct", "%", group=_IRL, stated="loose")
def _(v_ds_at_1ohm, u_r_most):
    return v_ds_at_1ohm / u_r_most


@fig("gate_over_th_irl", "V", group=_IRL, stated="loose")
def _(v_mod_nom, vgs_th_irl_max):
    return v_mod_nom - vgs_th_irl_max


# ===========================================================================
# phase-start droop
# ===========================================================================
@fig("delta_i", "mA", group="ΔI", section=DROOP,
     rises_with=["v_out_max", "n_channels"],
     falls_with=["r_led"])
def _(i_tot, i_dark):
    return i_tot - i_dark


@fig("c_module_derated", "µF", group="C_module", section=DROOP)
def _(c_module_printed, derate):
    return 2 * c_module_printed * derate.raw


@fig("c5_derated", "µF", group="C5", section=DROOP)
def _(c_entry, derate):
    return c_entry * derate.raw


@fig("c_total_derated", "µF", group="Droop", section=DROOP)
def _(c_module_derated, c5_derated):
    return c_module_derated + c5_derated


def _droop(di, f, c):
    return di / (2 * PI * f * c)


@fig("droop_75k", "mV", group="Droop", section=DROOP)
def _(delta_i, f_c_example, c_total_derated):
    return _droop(delta_i, f_c_example, c_total_derated)


@fig("droop_20k", "mV", group="Droop", section=DROOP,
     rises_with=["delta_i"],
     falls_with=["c_entry", "f_c_pess"])
def _(delta_i, f_c_pess, c_total_derated):
    return _droop(delta_i, f_c_pess, c_total_derated)


@fig("droop_module_20k", "mV", group="Droop", section=DROOP)
def _(delta_i, f_c_pess, c_module_derated):
    return _droop(delta_i, f_c_pess, c_module_derated)


@fig("droop_module_10k", "mV", group="Droop", section=DROOP)
def _(delta_i, f_c_worst, c_module_derated):
    return _droop(delta_i, f_c_worst, c_module_derated)


@fig("transient_scaled_pwm", "mV", group=DROOP, stated="loose")
def _(transient_pwm, delta_i, transient_step):
    return transient_pwm * (delta_i / transient_step)


@fig("transient_scaled_psm", "mV", group=DROOP, stated="loose")
def _(transient_psm, delta_i, transient_step):
    return transient_psm * (delta_i / transient_step)


@fig("floor_margin_c5", "mV", group=DROOP, stated="loose", tol_units=5)
def _(v_out_min, droop_20k, vdd_floor):
    return v_out_min - droop_20k - vdd_floor


@fig("floor_margin_module", "mV", group=DROOP, stated="loose", tol_units=5)
def _(v_out_min, droop_module_10k, vdd_floor):
    return v_out_min - droop_module_10k - vdd_floor


@fig("droop_of_u_r", "%", group=DROOP, stated="loose")
def _(droop_20k, u_r_most):
    return droop_20k / u_r_most


_C5S = "What C5 does"


@fig("c5_esr_bound", "Ω", group=_C5S, stated="loose")
def _(droop_20k, delta_i):
    return droop_20k / delta_i


@fig("esr_at_2ohm", "mV", group=_C5S, stated="loose")
def _(delta_i, esr_electrolytic):
    return delta_i * esr_electrolytic


@fig("psm_band_mv", "mV", group=_C5S, stated="loose")
def _(psm_band, v_mod_nom):
    return psm_band.raw * v_mod_nom


@fig("pulse_overshoot_c5", "mV", group=_C5S, stated="loose")
def _(pulse_overshoot_module, c_module_derated, c_total_derated):
    return pulse_overshoot_module * (c_module_derated / c_total_derated)


@fig("vcc2_excursion", "V", group="Load release overshoots by the same order",
     stated="loose")
def _(v_mod_max, vcc2_overshoot):
    return v_mod_max + vcc2_overshoot


# ===========================================================================
# the supply cable
# ===========================================================================
@fig("sag_margin", "V",
     group="The 5 V the module runs from sags while the bumper solenoids fire",
     stated="loose")
def _(sag_three_coils, v_mod_in_min):
    return sag_three_coils - v_mod_in_min


_CABLE = "How close the module has to sit to J-PWR"


@fig("cable_bound_drop", "mV", group=_CABLE, stated="loose")
def _(cable_r, i_tot):
    return cable_r * i_tot


@fig("four_contacts", "mΩ", group=_CABLE, stated="loose")
def _(contact_r, n_contacts):
    return n_contacts.raw * contact_r


@fig("wire_5cm", "mΩ", group=_CABLE, stated="loose")
def _(awg28_per_m, cable_length, one_metre):
    return 2 * awg28_per_m * (cable_length / one_metre).raw


@fig("further_drop_of_least", "%", group=_CABLE, stated="loose")
def _(further_drop, u_r_least):
    return further_drop / u_r_least


# ===========================================================================
# R39 and C6 at the reference
# ===========================================================================
_FILTER = "What R39 and C6 take off the reference"


@fig("z_c6_at_fsw", "Ω", group=_FILTER, stated="loose")
def _(f_sw, c_ref, derate):
    return 1 / (2 * PI * f_sw * c_ref * derate.raw)


@fig("filter_attenuation", "dB", group=_FILTER, stated="loose")
def _(r_ref, z_c6_at_fsw):
    return db(r_ref / z_c6_at_fsw)


@fig("filter_corner", "Hz", group=_FILTER, stated="loose", tol_units=5,
     falls_with=["r_ref", "c_ref"])
def _(r_ref, c_ref, derate):
    return 1 / (2 * PI * r_ref * c_ref * derate.raw)


@fig("ripple_left", "mV", group=_FILTER, stated="loose",
     rises_with=["psm_burst_ripple"],
     falls_with=["r_ref", "c_ref", "burst_freq_low"])
def _(psm_burst_ripple, filter_corner, burst_freq_low):
    return psm_burst_ripple * (filter_corner / burst_freq_low)


@fig("r39_offset", "mV", group=_FILTER, stated="loose",
     rises_with=["r_ref", "i_adc_idd"])
def _(i_adc_both, r_ref):
    return i_adc_both * r_ref


@fig("v_ref_min", "V", group="Supply and reference", section=CONV)
def _(v_out_min, r39_offset):
    return v_out_min - r39_offset


@fig("v_ref_max", "V", group="Supply and reference", section=CONV,
     rises_with=["v_out_max"],
     falls_with=["r_ref"])
def _(v_out_max, r39_offset):
    return v_out_max - r39_offset


@fig("ripple_of_v_ref", "%", group=_FILTER, stated="loose")
def _(ripple_left, v_ref_max):
    return ripple_left / v_ref_max


@fig("ripple_steps_full", "step", group=_FILTER, stated="loose")
def _(ripple_of_v_ref, bits):
    return Q(2 ** bits.raw) * ripple_of_v_ref.raw


_BEAD = _FILTER  # the bead paragraph carries no bold of its own


@fig("z_10uh_at_fsw", "Ω", group=_BEAD, stated="loose")
def _(f_sw, l_bead_alt):
    return 2 * PI * f_sw * l_bead_alt


@fig("z_10uh_at_burst", "Ω", group=_BEAD, stated="loose")
def _(burst_freq_low, l_bead_alt):
    return 2 * PI * burst_freq_low * l_bead_alt


# ===========================================================================
# the reference correction
# ===========================================================================
@fig("one_step_of_rail", "mV", group="bound", section=CONST)
def _(v_mod_nom, bits):
    return v_mod_nom / Q(2 ** bits.raw)


_STEP165 = CONST


@fig("dcm_boundary", "mA", group=_STEP165, stated="loose",
     falls_with=["l_mod", "f_sw"])
def _(v_in_machine, v_mod_nom, l_mod, f_sw):
    return (v_in_machine - v_mod_nom) * v_mod_nom / (v_in_machine * l_mod * f_sw) / 2


_FOLLOWS = CONST


@fig("signal_follows_rail", "×", stated=False)
def _(v_mod_nom, u_r_nominal):
    return v_mod_nom / u_r_nominal


@fig("reading_over_window", "%", group=_FOLLOWS, stated="loose")
def _(signal_follows_rail, mod_tol):
    return (signal_follows_rail - Q(1)) * mod_tol.raw


# ===========================================================================
# acquisition
# ===========================================================================
@fig("c_node", "pF", group="τ_adc", section=ACQ)
def _(c_pin, c_sample):
    return c_pin + c_sample


@fig("tau_adc", "ns", group="τ_adc", section=ACQ,
     rises_with=["r_pd", "c_sample", "r_switch"])
def _(r_pd, c_node, r_switch, c_sample):
    return r_pd * c_node + r_switch * c_sample


@fig("acq_window", "µs", group="window", section=ACQ,
     rises_with=["acq_clocks"],
     falls_with=["f_clk"])
def _(acq_clocks, f_clk):
    return acq_clocks.raw / f_clk


@fig("window_in_tau", "τ_adc", group="window", section=ACQ,
     rises_with=["acq_clocks"],
     falls_with=["f_clk", "r_pd", "c_sample"])
def _(acq_window, tau_adc):
    return acq_window / tau_adc


@fig("tau_needed", "τ_adc", group="needed", section=ACQ)
def _(bits):
    return ln(Q(2 ** bits.raw))


@fig("window_over_needed", "×", group="needed", section=ACQ)
def _(window_in_tau, tau_needed):
    return window_in_tau / tau_needed


@fig("window_at_3x_switch", "τ_adc", group=ACQ, stated="loose")
def _(acq_window, r_pd, c_node, r_switch, c_sample, switch_r_factor):
    return acq_window / (r_pd * c_node
                         + switch_r_factor.raw * r_switch * c_sample)


@fig("previous_channel_left", "steps", group=ACQ, stated="loose")
def _(window_in_tau, bits):
    return exp(-window_in_tau) * Q(2 ** bits.raw)


# ===========================================================================
# R37, R38, R36, R34, U3
# ===========================================================================
_R37 = "R37 and R38, 1 kΩ at each converter's DOUT pin"
_R36 = "R36, 10 kΩ from ADC-DOUT to the board's rail"
_R34 = "R34, 2 kΩ in the MISO line at U3's output"
_U3 = "U3 costs up to 10.3 mA on the Teensy's rail and 6.9 mA on the board's"


@fig("dout_contention", "mA", group=_R37, stated="loose")
def _(v_out_max, r_dout):
    return v_out_max / (2 * r_dout)


@fig("dout_low_level", "V", group=_R37, stated="loose",
     rises_with=["v_out_max", "r_dout", "v_ol_adc"],
     falls_with=["r_dout_pu"])
def _(v_ol_adc, v_out_max, r_dout, r_dout_pu):
    return v_ol_adc + (v_out_max - v_ol_adc) * r_dout / (r_dout + r_dout_pu)


@fig("guaranteed_low", "V", group=_R37, stated="loose")
def _(v_il_factor, v_mod_max):
    return v_il_factor.raw * v_mod_max


@fig("dout_sink", "mA", group=_R36, stated="loose",
     rises_with=["v_out_max"],
     falls_with=["r_dout", "r_dout_pu"])
def _(i_dout_low):
    return i_dout_low


@fig("miso_contention", "mA", group=_R34, stated="loose",
     rises_with=["v_mod_nom"],
     falls_with=["r_miso"])
def _(v_mod_nom, r_miso):
    return v_mod_nom / r_miso


@fig("i_iso_ac_s1", "mA", stated=False)
def _(i_iso_ac_s1_1m, i_iso_ac_s1_10m, f_clk, iso_rate_lo, iso_rate_hi):
    f = (f_clk - iso_rate_lo) / (iso_rate_hi - iso_rate_lo)
    return i_iso_ac_s1_1m + (i_iso_ac_s1_10m - i_iso_ac_s1_1m) * f.raw


@fig("i_iso_ac_s2", "mA", group=_U3, stated="loose")
def _(i_iso_ac_s2_1m, i_iso_ac_s2_10m, f_clk, iso_rate_lo, iso_rate_hi):
    f = (f_clk - iso_rate_lo) / (iso_rate_hi - iso_rate_lo)
    return i_iso_ac_s2_1m + (i_iso_ac_s2_10m - i_iso_ac_s2_1m) * f.raw


@fig("t_r37_net", "ns", group=_U3)
def _(r_dout, stray_dout):
    return r_dout * stray_dout


@fig("t_r34_net", "ns", group=_U3)
def _(r_miso, stray_miso):
    return r_miso * stray_miso


@fig("half_period", "ns", group=_U3,
     falls_with=["f_clk"])
def _(f_clk):
    return 1 / (2 * f_clk)


@fig("dout_round_trip", "ns", group=_U3,
     rises_with=["t_pd_iso", "t_do_adc", "r_dout", "r_miso"])
def _(t_pd_iso, t_do_adc, t_r37_net, t_r34_net):
    return 2 * t_pd_iso + t_do_adc + t_r37_net + t_r34_net


# ===========================================================================
# the read block and the phase
# ===========================================================================
@fig("t_conversion", "µs", group="per conversion", section=READ,
     rises_with=["clocks_frame"],
     falls_with=["f_clk"])
def _(clocks_frame, f_clk):
    return clocks_frame.raw / f_clk


@fig("block_16", "µs", group="block", section=READ)
def _(n_channels, t_conversion):
    return n_channels.raw * t_conversion


@fig("block_8", "µs", group="block", section=READ)
def _(t_conversion, n_eight):
    return n_eight.raw * t_conversion


@fig("budget_16", "µs", group="budget", section=READ,
     rises_with=["n_channels", "t_ovh", "clocks_frame"],
     falls_with=["f_clk"])
def _(n_channels, t_conversion, t_ovh, budget_grain):
    return ceil_to(n_channels.raw * (t_conversion + t_ovh), "µs",
                   budget_grain.to("µs"))


@fig("first_read_16", "µs", group="first read", section=READ,
     rises_with=["t_phase", "f_clk"],
     falls_with=["n_channels", "t_ovh", "t_jitter"])
def _(t_phase, budget_16, t_jitter):
    return t_phase - budget_16 - t_jitter


@fig("tau_pessimistic", "µs", stated=False,
     rises_with=["t_rf_max_1k", "r_pd"])
def _(t_rf_max_1k, r_pd, r_switch, rise_to_tau):
    return t_rf_max_1k * (r_pd / r_switch).raw / rise_to_tau.raw


@fig("tau_ln2", "µs", group="first read", section=READ,
     rises_with=["t_rf_max_1k", "r_pd"])
def _(tau_pessimistic):
    return tau_pessimistic * ln(Q(2)).raw


@fig("channels_the_phase_holds", "", stated=False)
def _(t_phase, t_conversion, t_ovh, tau_ln2, budget_grain):
    grain, n = budget_grain.to("µs"), 1
    while ceil_to((n + 1) * (t_conversion + t_ovh), "µs", grain) \
            + tau_ln2 <= t_phase:
        n += 1
    return Q(n)


# ===========================================================================
# the gate resistor
# ===========================================================================
@fig("gate_i_peak", "mA", group="I_peak", section=GATE,
     rises_with=["v_mod_nom"],
     falls_with=["r_gate"])
def _(v_mod_nom, r_gate):
    return v_mod_nom / r_gate


@fig("gate_time_ao", "µs", group="t", section=GATE)
def _(qg_ao, gate_i_peak):
    return qg_ao / (gate_i_peak / 2)


@fig("gate_time_irl", "µs", group="t", section=GATE,
     rises_with=["qg_irl", "r_gate"],
     falls_with=["v_mod_nom"])
def _(qg_irl, gate_i_peak):
    return qg_irl / (gate_i_peak / 2)


@fig("gate_time_of_phase", "%", group="t", section=GATE)
def _(gate_time_irl, t_phase):
    return gate_time_irl / t_phase


@fig("gate_peak_at_100r", "mA", group="t", section=GATE)
def _(v_mod_nom, r_led_alt_low):
    return v_mod_nom / r_led_alt_low


@fig("gate_peak_over_iso", "×", group="t", section=GATE)
def _(gate_peak_at_100r, i_out_iso):
    return gate_peak_at_100r / i_out_iso


@fig("led_settled_before_read", "µs", group=GATE, stated="loose")
def _(first_read_16, gate_time_irl):
    return first_read_16 - gate_time_irl


# ===========================================================================
# the signal pull-down
# ===========================================================================
@fig("headroom", "V", group="Headroom", section=PD)
def _(u_node_max):
    return u_node_max


@fig("signal_settled", "V", group="Resolution", section=PD,
     rises_with=["i_photo", "r_pd"])
def _(i_photo, r_pd):
    return i_photo * r_pd


@fig("step_nominal", "mV", stated=False)
def _(v_mod_nom, r39_offset, bits):
    return (v_mod_nom - r39_offset) / Q(2 ** bits.raw)


@fig("signal_steps_nominal", "steps", group="Resolution", section=PD)
def _(signal_settled, step_nominal):
    return signal_settled / step_nominal


@fig("signal_steps_max", "steps", group="Resolution", section=PD,
     rises_with=["i_photo", "r_pd"],
     falls_with=["v_out_max"])
def _(signal_settled, step_max):
    return signal_settled / step_max


@fig("swing_pessimistic", "%", group="Settling", section=PD,
     rises_with=["t_phase"],
     falls_with=["r_pd", "t_rf_max_1k", "n_channels"])
def _(first_read_16, tau_pessimistic, t_phase):
    x = exp(-(t_phase / tau_pessimistic))
    return Q(1) - 2 * exp(-(first_read_16 / tau_pessimistic)) / (1 + x)


@fig("signal_steps_first_read", "steps", group="Resolution", section=PD)
def _(signal_steps_max, swing_pessimistic):
    return signal_steps_max * swing_pessimistic.raw


@fig("r_node_total", "kΩ", group="Ambient headroom", section=PD)
def _(r_col, r_pd):
    return r_col + r_pd


@fig("ambient_headroom", "µA", group="Ambient headroom", section=PD,
     rises_with=["v_out_min"],
     falls_with=["r_col", "r_pd"])
def _(v_out_min, r_node_total):
    return v_out_min / r_node_total


@fig("ambient_headroom_10k", "µA", group="Ambient headroom", section=PD)
def _(v_out_min, r_col, r_pd_alt_high):
    return v_out_min / (r_col + r_pd_alt_high)


@fig("headroom_ratio", "×", group="Ambient headroom", section=PD)
def _(ambient_headroom, ambient_headroom_10k):
    return ambient_headroom / ambient_headroom_10k


@fig("t_rf_scaled", "µs", group="Settling", section=PD)
def _(t_rf_max_1k, r_pd, r_switch):
    return t_rf_max_1k * (r_pd / r_switch).raw


@fig("tau_from_max", "µs", group="Settling", section=PD)
def _(tau_pessimistic):
    return tau_pessimistic


@fig("tau_from_typ", "µs", group="Settling", section=PD)
def _(t_rf_typ_1k, r_pd, r_switch, rise_to_tau):
    return t_rf_typ_1k * (r_pd / r_switch).raw / rise_to_tau.raw


@fig("tau_fig6", "µs", group="Settling", section=PD)
def _(t_rf_max_1k, fig6_4k7, fig6_1k, rise_to_tau):
    return t_rf_max_1k * (fig6_4k7 / fig6_1k).raw / rise_to_tau.raw


def _swing(t, tau, T):
    x = exp(-(T / tau))
    return Q(1) - 2 * exp(-(t / tau)) / (1 + x)


@fig("swing_fig6", "%", group="Settling", section=PD)
def _(first_read_16, tau_fig6, t_phase):
    return _swing(first_read_16, tau_fig6, t_phase)


@fig("swing_typ", "%", group="Settling", section=PD)
def _(first_read_16, tau_from_typ, t_phase):
    return _swing(first_read_16, tau_from_typ, t_phase)


@fig("sign_inversion", "µs", group="Settling", section=PD)
def _(tau_pessimistic, t_phase):
    x = exp(-(t_phase / tau_pessimistic))
    return tau_pessimistic * -ln((1 + x) / Q(2)).raw


@fig("first_read_over_inversion", "×", group="Settling", section=PD)
def _(first_read_16, sign_inversion):
    return first_read_16 / sign_inversion


# ===========================================================================
# Figure 6, the swing, and where the 50 µA comes from
# ===========================================================================
_FIG6 = "Figure 6 of the sensor datasheet does not support that linear scaling"
_SWING = "The swing is the periodic one, not a single step"
_50UA = "Where the 50 µA comes from"
_SMALLER = _50UA


@fig("fig6_rise_1_to_10k", "×", stated=False)
def _(fig6_10k, fig6_1k):
    return fig6_10k / fig6_1k


@fig("fig6_4k7_over_1k", "×", stated=False)
def _(fig6_4k7, fig6_1k):
    return fig6_4k7 / fig6_1k


# the swing block writes its intermediate steps without units, and a bare
# number is addressable even though an expression full of them is not required
@fig("swing_x", "", group="at T", section=_SWING)
def _(t_phase, tau_pessimistic):
    return exp(-(t_phase / tau_pessimistic))


@fig("swing_exp_term", "", group="at T", section=_SWING)
def _(first_read_16, tau_pessimistic):
    return exp(-(first_read_16 / tau_pessimistic))


@fig("swing_denominator", "", group="at T", section=_SWING)
def _(swing_x):
    return 1 + swing_x


@fig("swing_result", "", group="at T", section=_SWING)
def _(swing_pessimistic):
    return Q(swing_pessimistic.raw)


@fig("gate_edge_in_tau", "τ", group="The swing is the periodic one, not a single step", stated="loose")
def _(gate_time_irl, tau_pessimistic):
    return gate_time_irl / tau_pessimistic


@fig("i_photo_scaled_to_char", "µA", group=_50UA, stated="loose")
def _(i_photo, i_f_char, i_led_nominal):
    return i_photo * (i_f_char / i_led_nominal)


@fig("emitter_over_char", "×", group=_50UA, stated="loose")
def _(i_led_nominal, i_f_char):
    return i_led_nominal / i_f_char


@fig("signal_at_first_read", "mV", group=_SMALLER, stated="loose")
def _(signal_settled, swing_pessimistic):
    return signal_settled * swing_pessimistic.raw


@fig("signal_at_2k2", "mV", group=_SMALLER, stated="loose")
def _(i_photo, first_read_16, t_rf_max_1k, r_switch, t_phase, r_pd_alt, rise_to_tau):
    tau = t_rf_max_1k * (r_pd_alt / r_switch).raw / rise_to_tau.raw
    return i_photo * r_pd_alt * _swing(first_read_16, tau, t_phase).raw


@fig("signal_fig6", "mV", group=_SMALLER, stated="loose")
def _(signal_settled, first_read_16, tau_fig6, t_phase):
    return signal_settled * _swing(first_read_16, tau_fig6, t_phase).raw


# ===========================================================================
# the swing table
# ===========================================================================
# A keyword default that is a string names the dependency, so one loop body
# serves every row and the formula stays readable to the linter.
for _n, _count, _label in ((3, "n_stock", "3, the stock sensors"),
                           (8, "n_eight", "8")):
    @fig(f"budget_{_n}", "µs", group=_label, section=PCT47)
    def _(t_conversion, t_ovh, budget_grain, n=_count):
        return ceil_to(n.raw * (t_conversion + t_ovh), "µs", budget_grain.to("µs"))

for _n, _label in ((3, "3, the stock sensors"), (8, "8"), (16, "16, the design case")):
    @fig(f"row{_n}_first_read", "µs", group=_label, section=PCT47)
    def _(t_phase, t_jitter, budget=f"budget_{_n}"):
        return t_phase - budget - t_jitter

    @fig(f"row{_n}_swing", "%", group=_label, section=PCT47)
    def _(tau_pessimistic, t_phase, first=f"row{_n}_first_read"):
        return _swing(first, tau_pessimistic, t_phase)

    @fig(f"row{_n}_signal", "mV", group=_label, section=PCT47)
    def _(signal_settled, swing=f"row{_n}_swing"):
        return signal_settled * swing.raw

    @fig(f"row{_n}_steps", "steps", group=_label, section=PCT47)
    def _(step_max, signal=f"row{_n}_signal"):
        return signal / step_max


@fig("budget_16_row", "µs", group="16, the design case", section=PCT47)
def _(budget_16):
    return budget_16


# ===========================================================================
# why Pin 1 needs no external resistor
# ===========================================================================
@fig("i_c_max_pin1", "mA", group="I_C,max", section=PIN1)
def _(v_out_max, r_node_total):
    return v_out_max / r_node_total


# ===========================================================================
# bounds on the adjustment knobs
# ===========================================================================
@fig("u_r_at_i_f_max", "V", group=KNOB, section=KNOB,
     rises_with=["v_out_max"],
     falls_with=["vf75_50ma"])
def _(v_out_max, v_f_75c_at_50ma):
    return v_out_max - v_f_75c_at_50ma


@fig("r_min_single", "Ω", group=KNOB, section=KNOB,
     rises_with=["v_out_max"],
     falls_with=["i_f_max", "vf75_50ma"],
     prints="up")
def _(u_r_at_i_f_max, i_f_max):
    return u_r_at_i_f_max / i_f_max


def _r_for_supply(i_supply, i_fixed, n, i_c, v_rail, curve):
    """The emitter resistor at which n channels exactly fill i_supply."""
    per = (i_supply - i_fixed - n * i_c) / n
    return (v_rail - interp_log(curve, per)) / per


@fig("knob_r_lower_8_teensy", "Ω", group=KNOB, section=KNOB,
     prints="up")
def _(i_teensy_3v3, i_fixed_teensy, i_c, v_out_max, vf75_10ma, vf75_10ma_at,
      vf75_50ma, vf75_50ma_at, n_eight):
    return _r_for_supply(i_teensy_3v3, i_fixed_teensy, n_eight.raw, i_c, v_out_max,
                         [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])


@fig("knob_r_lower_16_teensy", "Ω", group=KNOB, section=KNOB,
     prints="up")
def _(i_teensy_3v3, i_fixed_teensy, n_channels, i_c, v_out_max, vf75_10ma,
      vf75_10ma_at, vf75_50ma, vf75_50ma_at):
    return _r_for_supply(i_teensy_3v3, i_fixed_teensy, n_channels.raw, i_c, v_out_max,
                         [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])


@fig("knob_r_lower_16_module", "Ω", group=KNOB, section=KNOB,
     prints="up")
def _(i_module, i_fixed_module, n_channels, i_c, v_out_max, vf75_10ma,
      vf75_10ma_at, vf75_50ma, vf75_50ma_at):
    return _r_for_supply(i_module, i_fixed_module, n_channels.raw, i_c, v_out_max,
                         [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])


@fig("knob_r_upper", "Ω", group=KNOB, section=KNOB,
     rises_with=["v_out_min"],
     falls_with=["i_f_char", "vf_table_max"],
     prints="down")
def _(u_r_least, i_f_char):
    return u_r_least / i_f_char


_PDKNOB = "4.7 kΩ lower"


@fig("knob_pd_50mv", "mV", group=_PDKNOB, section=KNOB)
def _(i_photo, r_switch):
    return i_photo * r_switch


@fig("step_max", "mV", group=_PDKNOB, section=KNOB)
def _(v_ref_max, bits):
    return v_ref_max / Q(2 ** bits.raw)


@fig("knob_pd_steps_1k", "steps", group=_PDKNOB, section=KNOB)
def _(knob_pd_50mv, step_max):
    return knob_pd_50mv / step_max


@fig("t_switch_sample", "ns", group=_PDKNOB, section=KNOB)
def _(r_switch, c_sample):
    return r_switch * c_sample


@fig("knob_pd_acq_wall", "kΩ", group=_PDKNOB, section=KNOB,
     rises_with=["acq_clocks"],
     falls_with=["f_clk", "c_sample"],
     prints="down")
def _(acq_window, tau_needed, t_switch_sample, c_node):
    return (acq_window / tau_needed.raw - t_switch_sample) / c_node


@fig("knob_pd_ambient_at_wall", "µA", group=_PDKNOB, section=KNOB)
def _(v_out_min, r_col, knob_pd_acq_wall):
    return v_out_min / (r_col + knob_pd_acq_wall)


@fig("knob_pd_i_c_1k", "mA", group=_PDKNOB, section=KNOB)
def _(v_out_max, r_col, r_switch):
    return v_out_max / (r_col + r_switch)


@fig("knob_pd_i_tot_1k", "mA", group=_PDKNOB, section=KNOB)
def _(i_bus, n_channels, knob_pd_i_c_1k, i_adc_both, i_iso_s2):
    return i_bus + n_channels.raw * knob_pd_i_c_1k + i_adc_both + i_iso_s2


@fig("knob_gate_lower", "kΩ", group="2 kΩ", section=KNOB,
     prints="up")
def _(v_mod_nom, i_out_iso):
    return v_mod_nom / i_out_iso


@fig("knob_gate_upper_irl", "kΩ", group="gate", section=KNOB,
     prints="down")
def _(gate_charge_limit, v_mod_nom, qg_irl):
    return gate_charge_limit * v_mod_nom / (2 * qg_irl)


@fig("knob_gate_upper_ao", "kΩ", group="gate", section=KNOB,
     prints="down")
def _(gate_charge_limit, v_mod_nom, qg_ao):
    return gate_charge_limit * v_mod_nom / (2 * qg_ao)


@fig("knob_gate_at_3k", "V", group="100 kΩ lower", section=KNOB)
def _(v_mod_nom, r_gate, r_gate_pd_low):
    return v_mod_nom * r_gate_pd_low / (r_gate_pd_low + r_gate)


@fig("knob_gate_at_10k", "V", group="100 kΩ lower", section=KNOB)
def _(v_mod_nom, r_gate, r_gate_pd_mid):
    return v_mod_nom * r_gate_pd_mid / (r_gate_pd_mid + r_gate)


@fig("knob_gate_leakage", "mV", group="100 kΩ lower", section=KNOB)
def _(i_gss, r_leak_test):
    return i_gss * r_leak_test


@fig("knob_phase_lower", "µs", group="phase", section=KNOB,
     rises_with=["n_channels", "t_ovh", "r_pd"],
     falls_with=["f_clk"],
     prints="up")
def _(budget_16, tau_ln2):
    return budget_16 + tau_ln2


@fig("knob_firmware_bound", "µs", group="phase", section=KNOB,
     prints="down")
def _(t_phase, tau_ln2, budget_grain):
    return floor_to(t_phase - tau_ln2, "µs", budget_grain.to("µs"))


@fig("knob_t_ovh_ceiling", "µs", group="phase", section=KNOB, label="t_ovh",
     rises_with=["t_phase", "f_clk"],
     falls_with=["n_channels", "r_pd"],
     prints="down")
def _(knob_firmware_bound, n_channels, t_conversion):
    return knob_firmware_bound / n_channels.raw - t_conversion


@fig("knob_five_at_750", "ms", group="phase", section=KNOB)
def _(phase_alt, phases_per_two_values):
    return phases_per_two_values.raw * phase_alt


# one standard value inside each of those walls
@fig("i_at_100r", "mA", group="100 Ω", section=KNOB)
def _(v_out_max, vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at,
      r_led_alt_low):
    return _i_led_at(r_led_alt_low, v_out_max,
                     [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])


@fig("p_at_100r", "mW", group="100 Ω", section=KNOB)
def _(i_at_100r, r_led_alt_low):
    return i_at_100r ** 2 * r_led_alt_low


@fig("i_at_390r", "mA", group="390 Ω", section=KNOB)
def _(u_r_least, r_led_alt_high):
    return u_r_least / r_led_alt_high


@fig("signal_at_2k2_std", "mV", group="2.2 kΩ", section=KNOB)
def _(signal_at_2k2):
    return signal_at_2k2


@fig("steps_at_2k2", "steps", group="2.2 kΩ", section=KNOB)
def _(signal_at_2k2, step_max):
    return signal_at_2k2 / step_max


@fig("window_at_4k7", "τ", group="4.7 kΩ", section=KNOB)
def _(window_in_tau):
    return Q(window_in_tau.raw)


@fig("ambient_at_4k7", "µA", group="4.7 kΩ", section=KNOB)
def _(ambient_headroom):
    return ambient_headroom


# ===========================================================================
# the pull-down's upper bound, one row per value
# ===========================================================================
# A single-pole step crosses the sheet's 10 % and 90 % marks ln(9) time
# constants apart, which is how a 10-to-90 % rise time becomes a τ.
@fig("rise_to_tau", "", stated=False)
def _(rise_low, rise_high):
    return ln((1 - rise_low.raw) / (1 - rise_high.raw))


# `r` is bound to the declared input of that row's value, so no resistance is
# written into a formula.
for _key, _row in (("r_switch", "1 kΩ"), ("r_pd_alt", "2.2 kΩ"),
                   ("r_pd", "4.7 kΩ"), ("r_pd_alt_high", "10 kΩ")):
    @fig(f"sweep_{_key}_tau", "µs", group=_row, section=SWEEP)
    def _(t_rf_max_1k, r_switch, rise_to_tau, r=_key):
        return t_rf_max_1k * (r / r_switch).raw / rise_to_tau.raw

    @fig(f"sweep_{_key}_swing", "%", group=_row, section=SWEEP)
    def _(first_read_16, t_phase, tau=f"sweep_{_key}_tau"):
        return _swing(first_read_16, tau, t_phase)

    @fig(f"sweep_{_key}_signal", "mV", group=_row, section=SWEEP)
    def _(i_photo, r=_key, swing=f"sweep_{_key}_swing"):
        return i_photo * r * swing.raw

    @fig(f"sweep_{_key}_ambient", "µA", group=_row, section=SWEEP)
    def _(v_out_min, r_col, r=_key):
        return v_out_min / (r_col + r)

    @fig(f"sweep_{_key}_window", "τ", group=_row, section=SWEEP)
    def _(acq_window, c_node, r_switch, c_sample, r=_key):
        return acq_window / (r * c_node + r_switch * c_sample)


# ===========================================================================
# what the main body states on its own
# ===========================================================================
# These sit outside the appendix: the objections table, the troubleshooting
# table and the connector tables. They are stated in prose, so each is located
# by the section that holds it.
asm("ball_diameter", 9, "mm", src="the steel ball the EG01 kit supplies, taken as 9 mm; "
    "no measurement of it is recorded")
msr("stock_pulse_rate", 333, "Hz", src="the stock mainboard's emitter drive, 1.5 ms on and "
    "1.5 ms off at the P33 sensor board, docs/research/Rokr/2_ir-reflective-sensor-p33.md")
dec("r_led_brighter", 150, "Ω", src="the E12 value below 220 Ω, for a channel that "
    "returns too little light")
dec("c_bulk_rating", 10, "V", src="the voltage rating asked of C5 and C6, three times the rail")
dec("r_col_e96", 1.58, "kΩ", src="the E96 value beside the stock board's 1.585 kΩ")
dec("r_col_e24", 1.6, "kΩ", src="the E24 value beside the stock board's 1.585 kΩ")

# A resistor change moves the operating point, so V_F has to be taken at the
# new current. These two are the forward voltages the stated currents imply.
# They are not fresh readings: Figure 3 is only checked for bracketing them.
asm("vf25_at_150", 1.29, "V", src="the forward voltage 13.4 mA at 150 Ω implies. It "
    "lies between Figure 3's 25 °C points at 10 mA and 20 mA, and was not read off "
    "the curve")
asm("vf75_at_150", 1.21, "V", src="the forward voltage 14.9 mA at 150 Ω implies. It "
    "lies between Figure 3's 75 °C points at 10 mA and 50 mA, and was not read off "
    "the curve")


@fig("i_led_nominal_150", "mA", group="Adjustment knobs", stated="loose")
def _(v_mod_nom, vf25_at_150, r_led_brighter):
    return (v_mod_nom - vf25_at_150) / r_led_brighter


@fig("i_led_worst_150", "mA", group="Adjustment knobs", stated="loose")
def _(v_out_max, vf75_at_150, r_led_brighter):
    return (v_out_max - vf75_at_150) / r_led_brighter


# The channel ceiling is the divider at V_OUT max, so a different collector load
# moves it. Both alternatives to the stock 1.585 kΩ are quoted by that shift.
@fig("ceiling_shift_e96", "mV", group="Rebuilt sensor boards", stated="loose")
def _(v_out_max, r_pd, r_col, r_col_e96):
    return abs(v_out_max * r_pd / (r_col_e96 + r_pd)
               - v_out_max * r_pd / (r_col + r_pd))


@fig("ceiling_shift_e24", "mV", group="Rebuilt sensor boards", stated="loose")
def _(v_out_max, r_pd, r_col, r_col_e24):
    return abs(v_out_max * r_pd / (r_col_e24 + r_pd)
               - v_out_max * r_pd / (r_col + r_pd))


# ===========================================================================
# the plotted curves, as the points read off them
# ===========================================================================
# A curve reading appears nowhere in the sheet's text, so `--sheets` cannot
# look it up. Grouping the points of one curve gives the next best thing: the
# shape has to hold, each reading has to sit between the points around it, and
# where the sheet's table covers the same condition the reading has to sit
# inside it. A transposed digit or a point taken off the wrong axis fails here.
MODEL.curve("Figure 3, V_F against I_F at 25 °C",
            [("vf25_1ma_at", "vf25_1ma"), ("vf25_10ma_at", "vf25_10ma"),
             ("vf25_20ma_at", "vf25_20ma"), ("vf25_50ma_at", "vf25_50ma")],
            rises=True,
            on=[("i_led_nominal", "vf_nominal_read"),
                ("i_led_nominal_150", "vf25_at_150")],
            bounded=[("vf25_20ma", "vf_table_typ", "vf_table_max")])

MODEL.curve("Figure 3, V_F against I_F at 75 °C",
            [("vf75_10ma_at", "vf75_10ma"), ("vf75_50ma_at", "vf75_50ma")],
            rises=True,
            on=[("i_led_worst_150", "vf75_at_150")])

MODEL.curve("Figure 6, response time against load resistance",
            [("fig6_1k_at", "fig6_1k"), ("fig6_4k7_at", "fig6_4k7"),
             ("fig6_10k_at", "fig6_10k")],
            rises=True,
            bounded=[("fig6_1k", "t_rf_typ_1k", "t_rf_max_1k")])

MODEL.curve("Figure 4, collector current against ambient temperature",
            [("t_curve_25", "i_c_at_25c"), ("t_a", "i_c_at_40c")],
            rises=False)

# ===========================================================================
# what has to hold whatever the formulas are
# ===========================================================================
# Each of these is a requirement of the design, written without reference to
# any formula above. A slip that satisfies the arithmetic still has to satisfy
# these, and several of them are the safety claims of the whole subsystem.
_I = MODEL.invariant
_I("the three emitter cases are ordered",
   lambda v: v.i_led_least < v.i_led_nominal < v.i_led_worst)
_I("the rail brackets its nominal",
   lambda v: v.v_out_min < v.v_mod_nom < v.v_out_max)
_I("the reference brackets, and sits under the rail",
   lambda v: v.v_ref_min < v.v_ref_max < v.v_out_max)
_I("the board fits the module",
   lambda v: v.i_tot < v.i_module)
_I("the bench case fits the Teensy's 3V3 pin",
   lambda v: v.i_tot_bench < v.i_teensy_3v3)
_I("the emitter survives its absolute maximum",
   lambda v: v.i_led_worst < v.i_f_max)
_I("the phototransistor survives its absolute maximum",
   lambda v: v.i_c < v.i_c_abs_max)
_I("the channel node stays inside the converter's input range",
   lambda v: v.u_node_max < v.v_ref_max)
_I("the swing is a fraction of the settled signal",
   lambda v: 0 < v.swing_pessimistic.raw < 1)
_I("the acquisition window covers what ten bits need",
   lambda v: v.window_in_tau > v.tau_needed)
_I("the first read sits past the sign inversion",
   lambda v: v.first_read_16 > v.sign_inversion)
_I("the phase clears its own lower bound",
   lambda v: v.knob_phase_lower <= v.t_phase)
_I("a DOUT bit is home inside the half period",
   lambda v: v.dout_round_trip < v.half_period)
_I("the emitter resistor is above the single-channel bound",
   lambda v: v.r_min_single < v.r_led)
_I("the emitter resistor is below the characterising point",
   lambda v: v.r_led < v.knob_r_upper)
_I("the emitter resistor stays inside an 0805",
   lambda v: v.p_r_led < v.p_0805)
_I("Q1 stays inside its dissipation rating",
   lambda v: v.p_q1 < v.p_q1_rating)
_I("the gate draws no more than the isolator output gives",
   lambda v: v.gate_i_peak <= v.i_out_iso)
_I("the gate clears the worse of the two thresholds",
   lambda v: v.v_gate_min > v.vgs_th_irl_max)
_I("a converter's low is inside the isolator's guaranteed low",
   lambda v: v.dout_low_level < v.guaranteed_low)
_I("the rail with the bus on stays above the converter's floor",
   lambda v: v.v_out_min - v.droop_20k > v.vdd_floor)
_I("the stated cable allowance is the drop it stands for, to the millivolt",
   lambda v: abs(v.cable_drop - v.cable_drop_bound) <= Q.of(1, "mV"))
_I("the module's input survives the worst solenoid sag",
   lambda v: v.sag_three_coils > v.v_mod_in_min)
_I("the pull-down is under the acquisition wall",
   lambda v: v.r_pd < v.knob_pd_acq_wall)

_I("the hot forward-voltage curve sits under the cold one at 10 mA",
   lambda v: v.vf75_10ma < v.vf25_10ma)
_I("and at 50 mA",
   lambda v: v.vf75_50ma < v.vf25_50ma)
