"""The figures of the IR ball sensing design, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "ir-reflective",
                 drawings=["ir-sensor-mainboard.svg", "pulsed-schematic.svg",
                           "mainboard-placement.svg"],
                 documents=["../../firmware/drivers/ir-sensing.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()


def _to_printed(q, unit, decimals):
    """A quantity as the appendix prints it, so a chain of stated figures adds
    up the way the document does."""
    return Q.of(round(q.to(unit), decimals), unit)


SENSOR = "IR-reflective-gp2s700hcp_e.pdf"
ADC = "MCP3004-3008-Microchip.pdf"
ISO = "ISO7761-TI.pdf"
IRL = "IRL540N-Infineon.pdf"
REG = "ISL85415-Renesas.pdf"
POLOLU = "D24V5Fx-Pololu-schematic.pdf"

# the appendix sections, as the parser reads their bold leads
BASE = "Base quantities"
IFIXED = "I_fixed, the board's draw beside the emitters and the channel nodes"
DISS = "Dissipation, one line per part"
R220 = "220 Ω, LED series resistor"
DROOP = "Phase-start droop"
CONST = "Nothing scales the dark reading"
CONV = "The converter, what the part has to do"
ACQ = "Acquisition, and why no capacitor sits at a converter input"
READ = "The read block and the phase"
GATE = "2.2 kΩ, gate resistor"
PD = "4.7 kΩ, signal pull-down"
PCT96 = "The first read carries 96 % of the swing"
PIN1 = "Why Pin 1 needs no external resistor"
KNOB = "Bounds on the adjustment knobs"
SWEEP = "The pull-down's upper bound"
PI = Q(3.141592653589793)

# -- decisions: the values this design picked ---------------------------------
dec("n_channels", 16, "", src="sixteen positions, the case every figure is derived at")
dec("n_stock", 3, "", src="the three sensors the stock machine fits")
dec("n_eight", 8, "", src="eight positions, one converter fitted")
MODEL.uses("t_monitor", of="firmware")
dec("t_channel_fail", 1, "s", src="how long a channel's difference may stay under half its clear "
    "value before the driver reports DeviceFailed")
dec("n_cal", 200, "", src="readings the start-up calibration averages per channel, the quarter "
    "second that also gives the noise floor")
dec("e_cal", 5, "%", src="the factor error the averaging may leave, well under the drift of tens "
    "of per cent the factor corrects")

# the clear-track difference a channel needs, in multiples of the noise of one reading, so that
# the factor averaged over n_cal readings keeps its error within e_cal
fig("clear_over_noise_min", "×", "1 / (e_cal * sqrt(n_cal))", "up", moves="-e_cal -n_cal")
dec("budget_grain", 10, "µs", src="the step the firmware rounds the read block up to, so a phase "
    "holds whole tens")
dec("n_contacts", 4, "", src="two connectors on the J-PWR cable, each with a contact on both "
    "conductors")
dec("switch_r_factor", 3, "", src="three times the converter's switch resistance, the what-if the "
    "acquisition window is tested against")
dec("k_confirm", 2, "", src="readings in a row a hit has to survive before a ball is reported")
dec("r_led", 220, "Ω", src="E12 value inside the knob bounds derived below: the 220 Ω of the "
    "resistor kit")
dec("r_led_sharp", 100, "Ω", src="the three stock channels, set from the measured ball signal of "
    "27 converter steps at 220 Ω, research/Rokr/2_ir-reflective-sensor-p33.md: the 100 Ω of the "
    "resistor kit")
dec("r_pd", 4.7, "kΩ", src="E12 value inside the knob bounds derived below: the 4.7 kΩ of the "
    "resistor kit")
dec("r_gate", 2.2, "kΩ", src="R33, E12 value inside the gate bounds: the 2.2 kΩ of the resistor "
    "kit")
dec("r_gate_pd", 100, "kΩ", src="R35, E12 value inside the gate bounds: the 100 kΩ of the resistor "
    "kit")
dec("r_miso", 2.2, "kΩ", src="R34: the 2.2 kΩ of the resistor kit")
dec("r_dout", 1.2, "kΩ", src="R37 and R38: the 1.2 kΩ of the resistor kit")
dec("r_dout_pu", 10, "kΩ", src="R36: the 10 kΩ of the resistor kit")
dec("r_ref", 47, "Ω", src="R39, puts the filter corner under the burst rate: the 47 Ω of the "
    "resistor kit")
ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland "
   "product page: tolerance 5 %")
MODEL.uses("v_3v3_max", of="teensy-4.1")
dec("i_dout_contention_max", 2, "mA", src="the current two converter outputs may drive into each "
    "other until firmware raises a chip select: DS21295D rates none, and 2 mA keeps it at the "
    "order of the 1 mA its V_OL is specified at, as the isolator's outputs are held to their 2 mA",
    group="R37 and R38, 1.2 kΩ at each converter's DOUT pin", stated="loose")
dec("ripple_steps_max", 0.5, "step", src="the reference ripple a full-scale reading may carry, "
    "half a converter step, so it cannot move a reading by one")
dec("r_pd_floor", 1, "kΩ", src="the pull-down's lower end in the knob table, where a ball still "
    "gives about 11 steps of a ten-bit read")
dec("c_ref", 22, "µF", src="C6, printed value")
dec("c_entry", 22, "µF", src="C5, printed value")
dec("c_decoupling", 100, "nF", src="C1 to C4, one at each supply pin: the 0.1 µF SLLSER1H asks for "
    "at VCC1 and VCC2; at the converters DS21295D 6.4 asks for 1 µF at the pin, which C6 at their "
    "VDD pins supplies")
dec("t_phase", 600, "µs", src="the phase, sitting on the dwell bound derived below")
dec("t_phase_fallback", 1.5, "ms", src="the phase the driver falls back to after two failed "
    "initializations, the stock machine's own half period")
MODEL.uses("f_bus", of="teensy-4.1")
dec("qt_prescale", 4, "×", src="QuadTimer3's prescaler, the smallest power of two whose span holds "
    "the fallback phase")
MODEL.uses("qt_range", of="teensy-4.1")
with MODEL.block("V_OUT", section=BASE):
    dec("v_mod_nom", 3.3, "V", src="the D24V5F3 variant chosen for the 3.3 V rail")
    dec("working_margin", 0.5, "%", src="carried on top of the module maximum as the working bound")
dec("bits", 10, "", src="the MCP3008's resolution")
MODEL.uses("i_teensy_3v3", of="teensy-4.1")
dec("r_led_e12_below", 56, "Ω", src="the E12 value above the single-channel bound, also at 5 % "
    "under its value")
dec("r_led_alt_high", 390, "Ω", src="one standard value below the characterizing point")
dec("r_pd_alt", 2.2, "kΩ", src="one standard value, twice the resolution floor")
dec("r_pd_alt_high", 10, "kΩ", src="the pull-down one decade up, weighed in the sweep below")
dec("r_gate_pd_low", 3, "kΩ", src="the gate pull-down weighed and rejected on the threshold")
dec("r_gate_pd_mid", 10, "kΩ", src="the next gate pull-down up, also weighed")
dec("l_bead_alt", 10, "µH", src="the inductor weighed against R39")
dec("phase_alt", 750, "µs", src="the next phase up, weighed against the dwell")

# -- assumptions: no datasheet gives these ------------------------------------
asm("t_a", 40, "°C", group="T_A", section=BASE, stated=True, src="an open frame with no heat "
    "source nearby, on a hot day")
msr("i_photo", 36.6, "µA", src="bench, measurement B at the installed position with 100 Ω in the "
    "emitter line: 171.9 mV across the 4.7 kΩ, from 215 mV with a ball against 46.9 mV on a clear "
    "track, each minus its own dark reading, research/Rokr/2_ir-reflective-sensor-p33.md")
asm("rds_irl_est", 0.1, "Ω", src="an estimate above Fig. 1's typical 3.0 V curve, since the "
    "IRL540N specifies R_DS(on) only from V_GS = 4.0 V")
asm("cable_r", 0.1, "Ω", src="the round build figure for the J-PWR cable; it costs 0.2 mV more "
    "than the allowance, inside the millivolt the rail is printed at")
dec("cable_drop_bound", 22, "mV", group="V_OUT", section=BASE, stated=True, label="bounded at",
    src="the drop the rail minimum leaves for the cable, from which the 0.1 Ω follows")
asm("contact_r", 20, "mΩ", src="one crimped 2.54 mm contact")
asm("awg28_per_m", 0.20, "Ω", src="round figure for 28 AWG per meter, one conductor")
asm("f_c_pess", 20, "kHz", src="pessimistic reading of the module's crossover, against the 75 kHz "
    "of the FN8373.2 worked example")
asm("f_c_worst", 10, "kHz", src="the crossover the module's own capacitance is quoted at")
asm("t_ovh", 2, "µs", src="reserved per conversion for the firmware's own overhead")
asm("t_jitter", 10, "µs", src="reserve kept at the end of the phase, sized by the worst case for a "
    "trigger that fires late")
asm("stray_dout", 15, "pF", src="stray at the ADC-DOUT net")
asm("stray_miso", 20, "pF", src="stray at the MISO conductor and the Teensy pin")
asm("derate", 50, "%", src="ceramic capacitance kept under DC bias, as FN8373.2 itself advises")
asm("vcc2_overshoot", 70, "mV", src="load-release overshoot on VCC2, the order of the droop")
asm("pulse_overshoot_module", 23, "mV", src="Q_pulse / C on the module's capacitance alone")
asm("burst_freq_low", 9, "kHz", src="the low end of the power-save burst rate at the dark-phase "
    "load")
asm("further_drop", 17, "mV", src="one step of further cable drop past the bound")
asm("cable_length", 5, "cm", src="the run from the module to J-PWR the bound is taken at")
dec("one_meter", 1, "m", src="the length awg28_per_m is quoted per")
asm("rds_hypothetical", 1, "Ω", src="the R_DS(on) the IRL540N estimate is tested against, an order "
    "of magnitude above it")
asm("esr_electrolytic", 2, "Ω", src="the ESR an aluminum electrolytic would bring in place of C5")

# -- measured on the stock machine --------------------------------------------
dec("r_col_new", 0, "kΩ", src="the new boards take the collector straight to the rail. The stock "
    "board's 1.585 kΩ limits the collector to 549 µA where the part allows 20 mA, so it limits "
    "nothing any candidate needs limited, and it costs a third of the node's range")
msr("r_col", 1.585, "kΩ", src="the collector load on the stock P33 sensor board, read with a "
    "meter, research/Rokr/2_ir-reflective-sensor-p33.md")
msr("dark_reading", 3.8, "mV", src="bench, measurement B at the installed position with the "
    "emitter dark and the track clear, research/Rokr/2_ir-reflective-sensor-p33.md")
msr("tau_sensor", 70, "µs", src="bench, falling edge at the 4.7 kΩ pull-down with a ball on the "
    "track: 53 µs read, rounded up to the next ten because a larger τ is the safe side")
asm("dwell", 3, "ms", src="estimated from the stock machine's measured emitter period: it needs "
    "both windows of that period, so a ball it catches dwells at least one full period")
msr("sag_three_coils", 3.75, "V", src="the machine's 5 V with three bumper coils firing and a poor "
    "supply cable, research/Rokr/3_bumper-control.md")
msr("vf_diode_range", 1.082, "V", src="the stock board read in a meter's diode range, which sits "
    "on the 25 °C curve at the meter's test current")
msr("i_meter", 0.86, "mA", src="the meter's diode-range test current, from the 1.365 V it showed "
    "across the measured 1.585 kΩ")

# -- datasheet: Pololu D24V5F3 and the ISL85415 on it -------------------------
ds("mod_tol", 4, "%", group="V_OUT", section=BASE, stated=True, src="Pololu D24V5F3 product page "
   "2842, output voltage 3.3 V within 4 %")
ds("i_module", 500, "mA", src="Pololu D24V5F3, maximum output current")
ds("v_mod_in_min", 3.4, "V", src="Pololu D24V5F3, minimum input voltage")
ds("l_mod", 22, "µH", src="D24V5F3 inductor", sheet=POLOLU)
ds("f_sw", 500, "kHz", src="D24V5F3 switching frequency", sheet=POLOLU)
ds("c_module_printed", 10, "µF", src="two 10 µF on VOUT", sheet=POLOLU)
ds("psm_band", 1, "%", src="FN8373.2, the power-save comparator holds the output in a band of 1 %",
   sheet=REG)
ds("psm_ripple", 40, "mV", src="FN8373.2 Figure 45, about 40 mV peak to peak at 20 mA", sheet=REG)
ds("psm_burst_ripple", 44, "mV", src="FN8373.2, the burst band at the dark-phase load", sheet=REG)
ds("psm_burst_current", 300, "mA", src="FN8373.2, the pulses a burst is made of", sheet=REG)
ds("f_c_example", 75, "kHz", src="FN8373.2, the worked example for external compensation reaches "
   "75 kHz", sheet=REG)
ds("f_c_ceiling", 100, "kHz", src="FN8373.2 keeps the crossover under 100 kHz", sheet=REG)
ds("transient_psm", 160, "mV", src="FN8373.2 Figures 47 and 48, a 500 mA step from power-save",
   sheet=REG)
ds("transient_pwm", 95, "mV", src="FN8373.2 Figures 47 and 48, the same step from PWM", sheet=REG)
ds("transient_step", 500, "mA", src="FN8373.2 Figures 47 and 48, the step size", sheet=REG)
ds("transient_freq", 800, "kHz", src="FN8373.2 Figures 47 and 48, the switching frequency of those "
   "figures", sheet=REG)
ds("recovery", 200, "µs", src="FN8373.2 Figures 47 and 48, recovered within 200 µs", sheet=REG)
ds("psm_burst_period_ds", 80, "µs", src="FN8373.2 Figure 45, a burst every 80 µs at 20 mA",
   sheet=REG)
ds("v_in_machine", 5, "V", src="the machine's rail, the ROKR EG01 5 V adapter input")

# -- datasheet: Sharp GP2S700HCP, D3-A02201EN ---------------------------------
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
ds("t_curve_25", 25, "°C", src="GP2S700HCP D3-A02201EN Figure 3, the lower curve's temperature",
   sheet=SENSOR)
ds("t_curve_75", 75, "°C", src="GP2S700HCP D3-A02201EN Figure 3, the upper curve's temperature",
   sheet=SENSOR)
gr("vf_nominal_read", 1.24, "V", src="GP2S700HCP D3-A02201EN Figure 3, the 25 °C curve read at the "
   "9.3 mA the nominal case settles at", sheet=SENSOR)
ds("vf_table_max", 1.4, "V", src="GP2S700HCP D3-A02201EN, V_F maximum at I_F = 20 mA", sheet=SENSOR)
ds("vf_table_typ", 1.2, "V", src="GP2S700HCP D3-A02201EN, V_F typical at I_F = 20 mA", sheet=SENSOR)
ds("i_f_max", 50, "mA", src="GP2S700HCP D3-A02201EN, absolute maximum forward current",
   sheet=SENSOR)

# Figure 1 holds 50 mA flat to 25 °C and falls straight to 10 mA at 85 °C, so the
# rating the ratings table prints is the one that applies below the knee only.
_F1 = "GP2S700HCP D3-A02201EN Figure 1, I_F against ambient temperature"
gr("i_f_knee", 50, "mA", src=_F1 + ", where the flat part ends", sheet=SENSOR)
gr("i_f_knee_at", 25, "°C", src=_F1 + ", the temperature axis", sheet=SENSOR)
gr("i_f_end", 10, "mA", src=_F1 + ", where the falling line ends", sheet=SENSOR)
gr("i_f_end_at", 85, "°C", src=_F1 + ", the temperature axis", sheet=SENSOR)
ds("i_c_abs_max", 20, "mA", src="GP2S700HCP D3-A02201EN, absolute maximum collector current",
   sheet=SENSOR)
ds("i_f_char", 4, "mA", src="GP2S700HCP D3-A02201EN, the forward current I_C is characterized at",
   sheet=SENSOR)
ds("i_c_ds_min", 60, "µA", src="GP2S700HCP D3-A02201EN, I_C minimum at I_F = 4 mA, V_CE = 2 V, d = "
   "4 mm against an aluminum mirror", sheet=SENSOR)
ds("i_c_ds_max", 410, "µA", src="GP2S700HCP D3-A02201EN, I_C maximum at the same condition",
   sheet=SENSOR)
ds("v_ce_char", 2, "V", src="GP2S700HCP D3-A02201EN, the V_CE I_C is characterized at",
   sheet=SENSOR)
ds("t_rf_max_1k", 100, "µs", src="GP2S700HCP D3-A02201EN, t_r and t_f maximum at R_L = 1 kΩ",
   sheet=SENSOR)
ds("t_rf_typ_1k", 20, "µs", src="GP2S700HCP D3-A02201EN, t_r and t_f typical at the same load",
   sheet=SENSOR)
ds("rise_low", 10, "%", src="GP2S700HCP D3-A02201EN, the lower end of the rise-time band",
   sheet=SENSOR)
ds("rise_high", 90, "%", src="GP2S700HCP D3-A02201EN, the upper end", sheet=SENSOR)
gr("fig6_1k", 52, "µs", src="GP2S700HCP D3-A02201EN Figure 6, response time at 1 kΩ", sheet=SENSOR)
gr("fig6_4k7", 71, "µs", src="GP2S700HCP D3-A02201EN Figure 6, read at 4.7 kΩ", sheet=SENSOR)
gr("fig6_10k", 94, "µs", src="GP2S700HCP D3-A02201EN Figure 6, at 10 kΩ", sheet=SENSOR)
gr("i_c_at_40c", 92, "%", src="GP2S700HCP D3-A02201EN Figure 4, collector current against ambient "
   "temperature", sheet=SENSOR)
gr("i_c_at_25c", 100, "%", src="GP2S700HCP D3-A02201EN Figure 4 normalizes I_C to its value at "
   "25 °C, which is the curve's anchor", sheet=SENSOR)
gr("fig6_1k_at", 1, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis", sheet=SENSOR)
gr("fig6_4k7_at", 4.7, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis", sheet=SENSOR)
gr("fig6_10k_at", 10, "kΩ", src="GP2S700HCP D3-A02201EN Figure 6, the load axis", sheet=SENSOR)

# -- datasheet: IRL540N -------------------------------------------------------
gr("rds_irl_typ", 56, "mΩ", sheet=IRL, src="IRL540N, Fig. 1, typical output characteristics at T_J "
   "= 25 °C: the 3.0 V curve passes 1.8 A at V_DS = 0.1 V, below the gate Q1 gets", group="V_DS",
   section=BASE, stated=True)
ds("qg_irl", 74, "nC", src="IRL540N, Q_g maximum at V_GS = 5.0 V and I_D = 18 A")
ds("vgs_th_irl_max", 2.0, "V", src="IRL540N, V_GS(th) maximum")
ds("vgs_th_irl_min", 1.0, "V", src="IRL540N, V_GS(th) minimum")
ds("vgs_rating_irl", 16, "V", src="IRL540N, the V_GS I_GSS is specified at")
ds("i_gss", 100, "nA", src="IRL540N, I_GSS maximum at V_GS = ±16 V")
ds("r_leak_test", 1, "MΩ", src="the resistance the gate-leakage bound is taken at")
ds("t_j_max", 175, "°C", sheet=IRL, src="IRL540N, Absolute Maximum Ratings, operating junction "
   "temperature, maximum", group="Q1", section=DISS, stated=True)
ds("r_th_ja", 62, "°C/W", sheet=IRL, src="IRL540N, Thermal Resistance, junction-to-ambient, "
   "maximum", group="Q1", section=DISS, stated=True)
ds("p_kit", 0.25, "W", src="the same kit and page: power rating 0.25 W")

# -- datasheet: MCP3008, DS21295D ---------------------------------------------
ds("f_clk", 1.35, "MHz", src="DS21295D, the clock at VDD = 2.7 V", sheet=ADC)
ds("f_clk_5v", 3.6, "MHz", src="DS21295D, the clock at VDD = 5 V", sheet=ADC)
ds("clocks_protocol", 17, "clocks", src="DS21295D section 5.0", sheet=ADC)
ds("clocks_frame", 24, "clocks", src="DS21295D section 6.1, three whole bytes", sheet=ADC)
ds("acq_clocks", 1.5, "clocks", src="DS21295D, the acquisition window", sheet=ADC)
ds("v_in_over_vdd", 0.6, "V", src="DS21295D Absolute Maximum Ratings, all inputs and outputs with "
   "respect to VSS, −0.6 V to VDD + 0.6 V", sheet=ADC)
ds("c_pin", 7, "pF", src="DS21295D Figure 4-1, the pad capacitance", sheet=ADC)
ds("c_sample", 20, "pF", src="DS21295D Figure 4-1, the sample capacitor", sheet=ADC)
ds("r_switch", 1, "kΩ", src="DS21295D Figure 4-1, the switch resistance, typical", sheet=ADC)
ds("t_do_adc", 200, "ns", src="DS21295D, t_DO at VDD = 2.7 V", sheet=ADC)
ds("t_dis_adc", 100, "ns", src="DS21295D, t_DIS maximum", sheet=ADC)
ds("t_setup_adc", 50, "ns", src="DS21295D, setup and hold", sheet=ADC)
ds("i_adc_idd", 550, "µA", src="DS21295D, IDD maximum", sheet=ADC)
ds("i_adc_vref", 150, "µA", src="DS21295D, the VREF drain", sheet=ADC)
ds("v_ol_adc", 0.4, "V", src="DS21295D, V_OL maximum at I_OL = 1 mA and VDD = 4.5 V", sheet=ADC)
ds("i_ol_adc", 1, "mA", src="DS21295D, the I_OL that V_OL is specified at", sheet=ADC)
ds("vdd_adc_at_vol", 4.5, "V", src="DS21295D, the VDD that V_OL is specified at", sheet=ADC)
ds("vdd_floor", 2.7, "V", src="DS21295D, minimum VDD", sheet=ADC)
ds("vdd_ceiling", 5.5, "V", src="DS21295D, maximum VDD", sheet=ADC)
ds("vref_floor", 0.25, "V", src="DS21295D, minimum VREF", sheet=ADC)
ds("adc_abs_margin", 0.6, "V", src="DS21295D, inputs and outputs rated to VDD + 0.6 V", sheet=ADC)

# -- datasheet: ISO7761F, SLLSER1H --------------------------------------------
ds("i_iso_s1", 10.3, "mA", src="SLLSER1H, the 3.3 V DC row, side 1, every input at its own supply",
   sheet=ISO)
ds("i_iso_s2", 6.9, "mA", src="SLLSER1H, the same row, side 2", sheet=ISO)
ds("i_iso_ac_s1_1m", 6.5, "mA", src="SLLSER1H, side 1 at 1 Mbps", sheet=ISO)
ds("i_iso_ac_s2_1m", 6.3, "mA", src="SLLSER1H, side 2 at 1 Mbps", sheet=ISO)
ds("i_iso_ac_s1_10m", 7.5, "mA", src="SLLSER1H, side 1 at 10 Mbps", sheet=ISO)
ds("i_iso_ac_s2_10m", 8.6, "mA", src="SLLSER1H, side 2 at 10 Mbps", sheet=ISO)
ds("i_out_iso", 2, "mA", src="SLLSER1H, the I_OH the outputs are specified at", sheet=ISO)
ds("t_pd_iso", 18.5, "ns", src="SLLSER1H, propagation delay maximum at 3.3 V ± 10 % over the full "
   "temperature range", sheet=ISO)
ds("skew_iso", 4.4, "ns", src="SLLSER1H, channel-to-channel skew maximum", sheet=ISO)
ds("r_in_iso", 1.5, "MΩ", src="SLLSER1H Figure 7-3, the internal input pull-down", sheet=ISO)
ds("uvlo_lo", 1.7, "V", src="SLLSER1H, the lower undervoltage lockout threshold", sheet=ISO)
ds("uvlo_hi", 2.25, "V", src="SLLSER1H, the upper threshold", sheet=ISO)
ds("v_il_factor", 30, "%", src="SLLSER1H, V_IL guaranteed at 0.3 × VCC", sheet=ISO)
ds("v_ih_factor", 70, "%", src="SLLSER1H, V_IH guaranteed at 0.7 × VCC", sheet=ISO)
ds("iso_rate_lo", 1, "MHz", src="SLLSER1H, the lower AC row", sheet=ISO)
ds("iso_rate_hi", 10, "MHz", src="SLLSER1H, the upper AC row", sheet=ISO)

# -- base quantities ----------------------------------------------------------
with MODEL.block("V_OUT", section=BASE):
    fig("v_mod_min", "V", "v_mod_nom * (1 - mod_tol.raw)")
    fig("v_mod_max", "V", "v_mod_nom * (1 + mod_tol.raw)")
    fig("cable_drop", "mV", "cable_r * i_tot", moves="+cable_r +i_led_worst")
    fig("v_out_min", "V", "v_mod_min - cable_drop", moves="+v_mod_nom -mod_tol -cable_r")
    fig("v_out_max", "V", "v_mod_max * (1 + working_margin.raw)",
        moves="+v_mod_nom +mod_tol +working_margin")


def _vf75(vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, i):
    """The 75 °C curve of Figure 3, between the two points it publishes."""
    return interp_log([(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)], i)


fig("v_f_75c_at_t_a", "V",
    "_vf75(vf75_10ma, vf75_10ma_at, vf75_50ma, vf75_50ma_at, i_f_max_at_t_a)", stated=False)
fig("v_ds_irl_typ", "mV", "i_bus * rds_irl_typ", group="V_DS", section=BASE)
fig("v_ds_irl", "mV", "i_bus * rds_irl_est", group="V_DS", section=BASE)
with MODEL.block("U_R", section=BASE):
    # the appendix carries V_DS at the tenth of a millivolt it prints
    fig("u_r_nominal", "V", "v_mod_nom - vf_nominal_read - _to_printed(v_ds_irl_typ, 'mV', 1)",
        moves="+v_mod_nom -vf_nominal_read")
    fig("u_r_most", "V", "v_out_max - vf75_10ma", moves="+v_out_max -vf75_10ma")
    # the appendix carries V_DS at the millivolt it prints
    fig("u_r_least", "V", "v_out_min - vf_table_max - _to_printed(v_ds_irl, 'mV', 0)",
        moves="+v_out_min -vf_table_max -rds_irl_est")
with MODEL.block("I_LED", section=BASE):
    fig("i_led_nominal", "mA", "u_r_nominal / r_led", moves="+v_mod_nom -vf_nominal_read -r_led")
    fig("i_led_worst", "mA", "u_r_most / r_led", moves="+v_out_max -vf75_10ma -r_led")
    fig("i_led_worst_sharp", "mA", "u_r_most / r_led_sharp",
        moves="+v_out_max -vf75_10ma -r_led_sharp")
    fig("i_led_least", "mA", "u_r_least / r_led", moves="+v_out_min -vf_table_max -r_led")

with MODEL.block(section=BASE):
    # Taken on a new board, where nothing sits between the collector and the rail. The three stock
    # channels draw less through their own 1.585 kΩ, so counting every channel at this figure is the
    # safe side of the budget.
    fig("i_c", "mA", "v_out_max / (r_col_new + r_pd)", moves="+v_out_max -r_pd", group="I_C")
    fig("i_adc_one", "mA", "i_adc_idd + i_adc_vref", group="I_ADC")
    fig("i_adc_both", "mA", "2 * i_adc_one", group="I_ADC")
    fig("i_bus", "mA",
        "(n_channels.raw - n_stock.raw) * i_led_worst + n_stock.raw * i_led_worst_sharp",
        moves="+n_channels +v_out_max +n_stock -r_led -r_led_sharp", group="I_BUS")
with MODEL.block("I_TOT", section=BASE):
    fig("i_tot", "mA", "i_bus + n_channels.raw * i_c + i_adc_both + i_iso_s2",
        moves="+i_led_worst +n_channels +i_adc_idd -r_led -r_led_sharp")
    fig("i_tot_eight", "mA", "n_eight.raw * i_led_worst + n_eight.raw * i_c + i_adc_one + i_iso_s2")
    fig("i_tot_bench", "mA", "i_tot + i_iso_s1", moves="+i_iso_s1 -r_led")

fig("i_dark", "mA", "n_channels.raw * i_c + i_adc_both + i_iso_s2", stated=False)

# -- I_fixed ------------------------------------------------------------------
fig("i_fixed_module", "mA", "i_adc_both + i_iso_s2",
    group="from the module, side 1 still on the Teensy", section=IFIXED)
fig("i_fixed_teensy", "mA", "i_adc_both + i_iso_s1 + i_iso_s2",
    group="from the Teensy's 3V3 pin, both isolator sides out of it", section=IFIXED)
with MODEL.block(IFIXED, stated="loose"):
    fig("left_teensy", "mA", "i_teensy_3v3 - i_fixed_teensy - n_channels.raw * i_c")
    fig("left_module", "mA", "i_module - i_fixed_module - n_channels.raw * i_c")

# -- dissipation, one line per part -------------------------------------------
with MODEL.block("Q1", section=DISS):
    fig("p_q1", "mW", "i_bus ** 2 * rds_irl_est", moves="+i_led_worst +rds_irl_est -r_led")
    fig("t_j_q1", "°C", "t_a + p_q1 * r_th_ja", "up",
        moves="+t_a +i_led_worst +rds_irl_est +r_th_ja")
with MODEL.block("220 Ω", section=DISS):
    fig("p_r_led", "mW", "i_led_worst ** 2 * r_led", moves="+v_out_max -r_led")
    fig("p_r_led_pct", "%", "p_r_led / p_kit")

fig("p_r_led_sharp", "mW", "i_led_worst_sharp ** 2 * r_led_sharp", moves="+v_out_max -r_led_sharp",
    group="100 Ω", section=DISS)
fig("p_r_led_sharp_pct", "%", "p_r_led_sharp / p_kit", group="100 Ω", section=DISS)

with MODEL.block("4.7 kΩ", section=DISS):
    # A new board's node reaches the rail, a stock board's only the divider's share of it.
    fig("u_node_max", "V", "v_out_max * r_pd / (r_col_new + r_pd)", moves="+v_out_max")
    fig("p_r_pd", "mW", "u_node_max ** 2 / r_pd")
    fig("p_r_pd_pct", "%", "p_r_pd / p_kit")

# Each resistor of the DOUT path at the end of its tolerance that hurts the figure it enters.
fig("r_dout_lo", "kΩ", "r_dout * (1 - kit_tol)", moves="+r_dout -kit_tol", stated=False)
fig("r_dout_hi", "kΩ", "r_dout * (1 + kit_tol)", moves="+r_dout +kit_tol", stated=False)
fig("r_dout_pu_lo", "kΩ", "r_dout_pu * (1 - kit_tol)", moves="+r_dout_pu -kit_tol", stated=False)
fig("r_miso_hi", "kΩ", "r_miso * (1 + kit_tol)", moves="+r_miso +kit_tol", stated=False)

# The most a converter sinks while it pulls DOUT low, both resistors at their low end.
fig("i_dout_low", "mA", "v_out_max / (r_dout_lo + r_dout_pu_lo)", "up",
    moves="+v_out_max +kit_tol -r_dout -r_dout_pu", stated=False)

with MODEL.block(section=DISS):
    # R36 dissipates most at that current with its own low end.
    fig("p_r36", "mW", "i_dout_low ** 2 * r_dout_pu_lo", group="R36")
    fig("p_r36_pct", "%", "p_r36 / p_kit", group="R36")
    fig("v_gate_max", "V", "v_out_max * r_gate_pd / (r_gate_pd + r_gate)",
        moves="+v_out_max +r_gate_pd -r_gate", group="R35")
    fig("p_r35", "µW", "v_gate_max ** 2 / r_gate_pd", group="R35")
    fig("p_r37", "µW", "i_dout_low ** 2 * r_dout_hi", "up", group="R37, R38")
    fig("p_r39", "µW", "i_adc_both ** 2 * r_ref", group="R39")
    fig("i_gate_divider", "µA", "v_mod_nom / r_gate_pd", group="R33")
    fig("p_r33", "µW", "i_gate_divider ** 2 * r_gate", group="R33")

# -- 220 Ω, the emitter series resistor ---------------------------------------
with MODEL.block("P", section=R220):
    fig("p_r_led_rounded", "mW", "p_r_led")
    fig("p_kit_over_p_r_led", "×", "p_kit / p_r_led")
    fig("i_at_e12_below", "mA", "u_r_at_i_f_max / r_led_e12_below")
    fig("p_at_e12_below", "mW", "i_at_e12_below ** 2 * r_led_e12_below")
    fig("p_at_e12_below_pct", "%", "p_at_e12_below / p_kit")


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


with MODEL.block(R220, stated="loose"):
    def _i_f_max_at_t_a(i_f_knee, i_f_end, i_f_knee_at, i_f_end_at, t_a):
        span = (t_a - i_f_knee_at) / (i_f_end_at - i_f_knee_at)
        return i_f_knee - (i_f_knee - i_f_end) * span.raw
    fig("i_f_max_at_t_a", "mA", _i_f_max_at_t_a, "down", moves="+i_f_knee -t_a")

    fig("i_f_max_over_worst", "×", "i_f_max_at_t_a / i_led_worst")
    fig("i_f_max_over_sharp", "×", "i_f_max_at_t_a / i_led_worst_sharp", moves="-t_a")

# -- Q1 and the IRL540N -------------------------------------------------------
_IRL = "Q1 is an IRL540N"
with MODEL.block(_IRL, stated="loose"):
    fig("v_gate_min", "V", "v_out_min * r_gate_pd / (r_gate_pd + r_gate)")
    fig("v_ds_at_1ohm", "mV", "i_bus * rds_hypothetical")
    fig("v_ds_at_1ohm_pct", "%", "v_ds_at_1ohm / u_r_most")
    fig("gate_over_th_irl", "V", "v_mod_nom - vgs_th_irl_max")

# -- phase-start droop --------------------------------------------------------
with MODEL.block(section=DROOP):
    fig("delta_i", "mA", "i_tot - i_dark", moves="+v_out_max +n_channels -r_led", group="ΔI")
    fig("c_module_derated", "µF", "2 * c_module_printed * derate.raw", group="C_module")
    fig("c5_derated", "µF", "c_entry * derate.raw", group="C5")
    fig("c_total_derated", "µF", "c_module_derated + c5_derated", group="Droop")


def _droop(di, f, c):
    return di / (2 * PI * f * c)


with MODEL.block("Droop", section=DROOP):
    fig("droop_75k", "mV", "_droop(delta_i, f_c_example, c_total_derated)")
    fig("droop_20k", "mV", "_droop(delta_i, f_c_pess, c_total_derated)",
        moves="+delta_i -c_entry -f_c_pess")
    fig("droop_module_20k", "mV", "_droop(delta_i, f_c_pess, c_module_derated)")
    fig("droop_module_10k", "mV", "_droop(delta_i, f_c_worst, c_module_derated)")
with MODEL.block(DROOP, stated="loose"):
    fig("transient_scaled_pwm", "mV", "transient_pwm * (delta_i / transient_step)")
    fig("transient_scaled_psm", "mV", "transient_psm * (delta_i / transient_step)")
    fig("floor_margin_c5", "mV", "v_out_min - droop_20k - vdd_floor", tol_units=5)
    fig("floor_margin_module", "mV", "v_out_min - droop_module_10k - vdd_floor", tol_units=5)
    fig("droop_of_u_r", "%", "droop_20k / u_r_most")

_C5S = "What C5 does"
with MODEL.block(_C5S, stated="loose"):
    fig("c5_esr_bound", "Ω", "droop_20k / delta_i")
    fig("esr_at_2ohm", "mV", "delta_i * esr_electrolytic")
    fig("psm_band_mv", "mV", "psm_band.raw * v_mod_nom")
    fig("pulse_overshoot_c5", "mV", "pulse_overshoot_module * (c_module_derated / c_total_derated)")

fig("vcc2_excursion", "V", "v_mod_max + vcc2_overshoot",
    group="Load release overshoots by the same order", stated="loose")

# -- the supply cable ---------------------------------------------------------
fig("sag_margin", "V", "sag_three_coils - v_mod_in_min",
    group="The 5 V the module runs from sags while the bumper solenoids fire", stated="loose")

_CABLE = "How close the module has to sit to J-PWR"
with MODEL.block(_CABLE, stated="loose"):
    fig("cable_bound_drop", "mV", "cable_r * i_tot")
    fig("four_contacts", "mΩ", "n_contacts.raw * contact_r")
    fig("wire_5cm", "mΩ", "2 * awg28_per_m * (cable_length / one_meter).raw")
    fig("further_drop_of_least", "%", "further_drop / u_r_least")

# -- R39 and C6 at the reference ----------------------------------------------
_FILTER = "What R39 and C6 take off the reference"
with MODEL.block(_FILTER, stated="loose"):
    fig("z_c6_at_fsw", "Ω", "1 / (2 * PI * f_sw * c_ref * derate.raw)")
    fig("filter_attenuation", "dB", "db(r_ref / z_c6_at_fsw)")
    fig("filter_corner", "Hz", "1 / (2 * PI * r_ref * c_ref * derate.raw)", moves="-r_ref -c_ref",
        tol_units=5)
    fig("ripple_left", "mV", "psm_burst_ripple * (filter_corner / burst_freq_low)",
        moves="+psm_burst_ripple -r_ref -c_ref -burst_freq_low")
    fig("r39_offset", "mV", "i_adc_both * r_ref", moves="+r_ref +i_adc_idd")
with MODEL.block("Supply and reference", section=CONV):
    fig("v_ref_min", "V", "v_out_min - r39_offset")
    fig("v_ref_max", "V", "v_out_max - r39_offset", moves="+v_out_max -r_ref")
with MODEL.block(_FILTER, stated="loose"):
    fig("ripple_of_v_ref", "%", "ripple_left / v_ref_max")
    fig("ripple_steps_full", "step", "Q(2 ** bits.raw) * ripple_of_v_ref.raw")

_BEAD = _FILTER  # the bead paragraph carries no bold lead

fig("z_10uh_at_fsw", "Ω", "2 * PI * f_sw * l_bead_alt", group=_BEAD, stated="loose")
fig("z_10uh_at_burst", "Ω", "2 * PI * burst_freq_low * l_bead_alt", group=_BEAD, stated="loose")

# -- the reference correction -------------------------------------------------
with MODEL.block(CONST, section=CONST, stated="loose"):
    fig("dark_steps", "steps", "dark_reading / step_max", moves="+dark_reading -v_ref_max")

    # What the correction can move is (k - 1) times the dark reading. Inverting it
    # gives the k - 1 at which it would move one converter step, which needs no
    # figure for the module's unpublished load regulation.
    fig("correction_worth_at", "%", "Q(1) / dark_steps", moves="+v_ref_max -dark_reading")

fig("one_step_of_rail", "mV", "v_mod_nom / Q(2 ** bits.raw)", group="bound", section=CONST)

_STEP165 = CONST

fig("dcm_boundary", "mA",
    "(v_in_machine - v_mod_nom) * v_mod_nom / (v_in_machine * l_mod * f_sw) / 2",
    moves="-l_mod -f_sw", group=_STEP165, stated="loose")

_FOLLOWS = CONST

fig("signal_follows_rail", "×", "v_mod_nom / u_r_nominal", stated=False)
fig("reading_over_window", "%", "(signal_follows_rail - Q(1)) * mod_tol.raw", group=_FOLLOWS,
    stated="loose")

# -- acquisition --------------------------------------------------------------
with MODEL.block("τ_adc", section=ACQ):
    fig("c_node", "pF", "c_pin + c_sample")
    fig("tau_adc", "ns", "r_pd * c_node + r_switch * c_sample", moves="+r_pd +c_sample +r_switch")
with MODEL.block("window", section=ACQ):
    fig("acq_window", "µs", "acq_clocks.raw / f_clk", moves="+acq_clocks -f_clk")
    fig("window_in_tau", "τ_adc", "acq_window / tau_adc",
        moves="+acq_clocks -f_clk -r_pd -c_sample")

fig("tau_needed", "τ_adc", "ln(Q(2 ** bits.raw))", group="needed", section=ACQ)
fig("window_over_needed", "×", "window_in_tau / tau_needed", group="needed", section=ACQ)
with MODEL.block(ACQ, stated="loose"):
    fig("window_at_3x_switch", "τ_adc",
        "acq_window / (r_pd * c_node + switch_r_factor.raw * r_switch * c_sample)")
    fig("previous_channel_left", "steps", "exp(-window_in_tau) * Q(2 ** bits.raw)")

# -- R37, R38, R36, R34, U3 ---------------------------------------------------
_R37 = "R37 and R38, 1.2 kΩ at each converter's DOUT pin"
_R36 = "R36, 10 kΩ from ADC-DOUT to the board's rail"
_R34 = "R34, 2.2 kΩ in the MISO line at U3's output"
_U3 = "U3 costs up to 10.3 mA on the Teensy's rail and 6.9 mA on the board's"
with MODEL.block(_R37, stated="loose"):
    fig("dout_contention", "mA", "v_out_max / (2 * r_dout_lo)", "up",
        moves="+v_out_max +kit_tol -r_dout")
    fig("dout_low_level", "V",
        "v_ol_adc + (v_out_max - v_ol_adc) * r_dout_hi / (r_dout_hi + r_dout_pu_lo)", "up",
        moves="+v_out_max +r_dout +v_ol_adc +kit_tol -r_dout_pu")
    fig("guaranteed_low", "V", "v_il_factor.raw * v_mod_max")
with MODEL.block(stated="loose"):
    fig("dout_sink", "mA", "i_dout_low", "up", moves="+v_out_max +kit_tol -r_dout -r_dout_pu",
        group=_R36)

    # MISO sits on side 1, which runs from the Teensy's own 3.3 V.
    fig("miso_contention", "mA", "v_3v3_max / (r_miso * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol -r_miso", group=_R34)

    def _i_iso_ac_s1(i_iso_ac_s1_1m, i_iso_ac_s1_10m, f_clk, iso_rate_lo, iso_rate_hi):
        f = (f_clk - iso_rate_lo) / (iso_rate_hi - iso_rate_lo)
        return i_iso_ac_s1_1m + (i_iso_ac_s1_10m - i_iso_ac_s1_1m) * f.raw
    fig("i_iso_ac_s1", "mA", _i_iso_ac_s1, stated=False)

    def _i_iso_ac_s2(i_iso_ac_s2_1m, i_iso_ac_s2_10m, f_clk, iso_rate_lo, iso_rate_hi):
        f = (f_clk - iso_rate_lo) / (iso_rate_hi - iso_rate_lo)
        return i_iso_ac_s2_1m + (i_iso_ac_s2_10m - i_iso_ac_s2_1m) * f.raw
    fig("i_iso_ac_s2", "mA", _i_iso_ac_s2, group=_U3)
with MODEL.block(_U3):
    fig("t_r37_net", "ns", "r_dout_hi * stray_dout")
    fig("t_r34_net", "ns", "r_miso_hi * stray_miso")
    fig("half_period", "ns", "1 / (2 * f_clk)", moves="-f_clk")
    fig("dout_round_trip", "ns", "2 * t_pd_iso + t_do_adc + t_r37_net + t_r34_net",
        moves="+t_pd_iso +t_do_adc +r_dout +r_miso")

# -- the read block and the phase ---------------------------------------------
with MODEL.block(section=READ):
    fig("t_conversion", "µs", "clocks_frame.raw / f_clk", moves="+clocks_frame -f_clk",
        group="per conversion")
    fig("block_16", "µs", "n_channels.raw * t_conversion", group="block")
    fig("block_8", "µs", "n_eight.raw * t_conversion", group="block")
    fig("budget_16", "µs",
        "ceil_to(n_channels.raw * (t_conversion + t_ovh), 'µs', budget_grain.to('µs'))",
        moves="+n_channels +t_ovh +clocks_frame -f_clk", group="budget")
with MODEL.block("first read", section=READ):
    fig("first_read_16", "µs", "t_phase - budget_16 - t_jitter",
        moves="+t_phase +f_clk -n_channels -t_ovh -t_jitter")
    fig("tau_ln2", "µs", "tau_sensor * ln(Q(2)).raw", moves="+tau_sensor")

def _channels_the_phase_holds(t_phase, t_conversion, t_ovh, tau_ln2, t_jitter, budget_grain):
    grain, n = budget_grain.to("µs"), 1
    while ceil_to((n + 1) * (t_conversion + t_ovh), "µs", grain) \
            + tau_ln2 + t_jitter <= t_phase:
        n += 1
    return Q(n)
fig("channels_the_phase_holds", "", _channels_the_phase_holds, stated=False)

# -- every resistor's window: the least and the most a fitted value may be, its
# tolerance and the rail's ends taken where they hurt
# The emitter resistors between the single-channel bound and the characterizing point.
fig("r_led_min", "Ω", "r_min_single / (1 - kit_tol)", "up",
    moves="+v_out_max +kit_tol +t_a -i_f_knee -vf75_50ma", stated=False)
fig("r_led_max", "Ω", "knob_r_upper / (1 + kit_tol)", "down",
    moves="+v_out_min -i_f_char -vf_table_max -kit_tol", stated=False)

# The pull-down between its resolution floor and the acquisition wall.
fig("r_pd_min", "kΩ", "r_pd_floor / (1 - kit_tol)", "up", moves="+r_pd_floor +kit_tol",
    stated=False)
fig("r_pd_max", "kΩ", "knob_pd_acq_wall / (1 + kit_tol)", "down",
    moves="+acq_clocks -f_clk -c_sample -kit_tol", stated=False)

with MODEL.block("gate", section=KNOB):
    # The gate edge the phase tolerates. The LED bus has its current by the end of the edge, and
    # from there a channel needs the sign inversion's τ · ln 2 before it carries more of this
    # phase than of the last, so the edge may last until the first read less that.
    fig("gate_charge_limit", "µs", "first_read_16 - sign_inversion", "down",
        moves="+t_phase +f_clk -n_channels -t_ovh -t_jitter -tau_sensor")

    # R33: at its low end the gate's charging current stays inside the isolator output's 2 mA on
    # the rail's high end, and at its high end the IRL540N's charge still lands within the gate
    # edge the phase tolerates on the rail's low end.
    fig("r_gate_min", "kΩ", "v_out_max / (i_out_iso * (1 - kit_tol))", "up",
        moves="+v_out_max +kit_tol -i_out_iso")
    fig("r_gate_max", "kΩ", "gate_charge_limit * v_out_min / (2 * qg_irl * (1 + kit_tol))", "down",
        moves="+gate_charge_limit +v_out_min -qg_irl -kit_tol")

# R34: at its low end the Teensy and U3 drive against each other inside the isolator's 2 mA,
# and at its high end a DOUT bit still comes home inside the half period.
fig("r_miso_min", "kΩ", "v_3v3_max / (i_out_iso * (1 - kit_tol))", "up",
    moves="+v_3v3_max +kit_tol -i_out_iso", stated=False)
fig("r_miso_max", "kΩ",
    "(half_period - 2 * t_pd_iso - t_do_adc - t_r37_net) / (stray_miso * (1 + kit_tol))", "down",
    moves="-f_clk -t_pd_iso -t_do_adc -r_dout -stray_dout -stray_miso -kit_tol", stated=False)

# R35: at its low end it still leaves the gate above the IRL540N's highest threshold behind
# R33 at its high end, and at its high end the gate's leakage still holds the IRL540N's
# lowest threshold off.
fig("r_gate_pd_min", "kΩ",
    "r_gate * (1 + kit_tol) * vgs_th_irl_max / ((v_out_min - vgs_th_irl_max) * (1 - kit_tol))",
    "up", moves="+r_gate +kit_tol +vgs_th_irl_max -v_out_min", stated=False)
fig("r_gate_pd_max", "kΩ", "vgs_th_irl_min / (i_gss * (1 + kit_tol))", "down",
    moves="+vgs_th_irl_min -i_gss -kit_tol", stated=False)

# The share of the rail R37 may take of the swing above V_OL and still leave a guaranteed low.
# The pull-up and the threshold both follow the rail, and the margin shrinks with it, so the
# rail's low end is the one that governs.
fig("dout_share_max", "", "(v_il_factor * v_out_min - v_ol_adc) / (v_out_min - v_ol_adc)",
    moves="+v_il_factor +v_out_min -v_ol_adc", stated=False)

# R36: at its low end, against R37 at its high end, it still leaves that low; at its high end
# it still holds a high against the isolator's own pull-down at the input.
fig("r_dout_pu_min", "kΩ", "r_dout_hi * (1 - dout_share_max) / (dout_share_max * (1 - kit_tol))",
    "up", moves="+r_dout +kit_tol +v_ol_adc -v_il_factor -v_out_min", stated=False)
fig("r_dout_pu_max", "kΩ", "r_in_iso * (1 - v_ih_factor) / (v_ih_factor * (1 + kit_tol))", "down",
    moves="+r_in_iso -v_ih_factor -kit_tol", stated=False)

# R37 and R38: at their low end two converters fighting stay inside the current the design
# allows them, and at their high end, against R36 at its low end, the low still holds.
fig("r_dout_min", "kΩ", "v_out_max / (2 * i_dout_contention_max * (1 - kit_tol))", "up",
    moves="+v_out_max +kit_tol -i_dout_contention_max", stated=False)
fig("r_dout_max", "kΩ", "r_dout_pu_lo * dout_share_max / ((1 - dout_share_max) * (1 + kit_tol))",
    "down", moves="+r_dout_pu +v_il_factor +v_out_min -kit_tol -v_ol_adc", stated=False)

# R39: at its low end the reference ripple a full-scale reading carries stays under its
# ceiling, and at its high end the converters behind it, with the phase-start droop, stay
# above their floor.
fig("r_ref_min", "Ω", "psm_burst_ripple * Q(2 ** bits.raw) / (ripple_steps_max.raw * v_ref_max * "
    "2 * PI * c_ref * derate.raw * burst_freq_low * (1 - kit_tol))", "up",
    moves="+psm_burst_ripple +bits +kit_tol -ripple_steps_max -c_ref -derate -burst_freq_low",
    stated=False)
fig("r_ref_max", "Ω", "(v_out_min - droop_20k - vdd_floor) / (i_adc_both * (1 + kit_tol))", "down",
    moves="+v_mod_nom -vdd_floor -i_adc_idd -kit_tol", stated=False)

# -- the gate resistor --------------------------------------------------------
fig("gate_i_peak", "mA", "v_mod_nom / r_gate", moves="+v_mod_nom -r_gate", group="I_peak",
    section=GATE)
with MODEL.block("t", section=GATE):
    fig("gate_time_irl", "µs", "qg_irl / (gate_i_peak / 2)", moves="+qg_irl +r_gate -v_mod_nom")
    fig("gate_time_of_phase", "%", "gate_time_irl / t_phase")
    fig("gate_peak_at_100r", "mA", "v_mod_nom / r_led_sharp")
    fig("gate_peak_over_iso", "×", "gate_peak_at_100r / i_out_iso")

fig("led_settled_before_read", "µs", "first_read_16 - gate_time_irl", group=GATE, stated="loose")

# -- the signal pull-down -----------------------------------------------------
with MODEL.block("Headroom", section=PD):
    fig("headroom", "V", "u_node_max")

    # A new board's node reaches the rail, which is above VREF but far inside what the part tolerates:
    # a reading there clips at full scale, and a clipped channel is the flooded one the design rejects
    # anyway. What has to hold is the absolute maximum, and that is a different figure.
    fig("v_in_abs_max", "V", "v_out_max + v_in_over_vdd", "down", moves="+v_out_max +v_in_over_vdd")
with MODEL.block("Resolution", section=PD):
    fig("signal_settled", "V", "i_photo * r_pd", moves="+i_photo +r_pd")
    fig("step_nominal", "mV", "(v_mod_nom - r39_offset) / Q(2 ** bits.raw)", stated=False)
    fig("signal_steps_nominal", "steps", "signal_settled / step_nominal")
    fig("signal_steps_max", "steps", "signal_settled / step_max", moves="+i_photo +r_pd -v_out_max")

def _swing_sensor(first_read_16, tau_sensor, t_phase):
    x = exp(-(t_phase / tau_sensor))
    return Q(1) - 2 * exp(-(first_read_16 / tau_sensor)) / (1 + x)
fig("swing_sensor", "%", _swing_sensor, moves="+t_phase -tau_sensor -n_channels", group="Settling",
    section=PD)

fig("signal_steps_first_read", "steps", "signal_steps_max * swing_sensor.raw", group="Resolution",
    section=PD)

# The load costs headroom rather than buying it, so the three stock channels are the worst case
# for every ambient figure below and the new boards have more room than these say.
# What the three stock boards do differently, kept so the rebuilt board's reason is checkable.
fig("stock_node_loss", "%", "Q(1) - r_pd / (r_col + r_pd)", moves="+r_col -r_pd",
    group="The new sensor boards get no collector resistor", stated="loose")
with MODEL.block("Ambient headroom", section=PD):
    fig("r_node_total", "kΩ", "r_col + r_pd")
    fig("ambient_headroom", "µA", "v_out_min / r_node_total", moves="+v_out_min -r_col -r_pd")
    fig("ambient_headroom_10k", "µA", "v_out_min / (r_col + r_pd_alt_high)")
    fig("headroom_ratio", "×", "ambient_headroom / ambient_headroom_10k")
with MODEL.block("Settling", section=PD):
    fig("t_rf_scaled", "µs", "t_rf_max_1k * (r_pd / r_switch).raw")
    fig("tau_from_max", "µs", "t_rf_max_1k * (r_pd / r_switch).raw / rise_to_tau.raw",
        moves="+t_rf_max_1k +r_pd")
    fig("tau_from_typ", "µs", "t_rf_typ_1k * (r_pd / r_switch).raw / rise_to_tau.raw")
    fig("tau_fig6", "µs", "t_rf_max_1k * (fig6_4k7 / fig6_1k).raw / rise_to_tau.raw")


def _swing(t, tau, T):
    x = exp(-(T / tau))
    return Q(1) - 2 * exp(-(t / tau)) / (1 + x)


with MODEL.block("Settling", section=PD):
    fig("swing_from_max", "%", "_swing(first_read_16, tau_from_max, t_phase)")
    fig("swing_fig6", "%", "_swing(first_read_16, tau_fig6, t_phase)")
    fig("swing_typ", "%", "_swing(first_read_16, tau_from_typ, t_phase)")

    def _sign_inversion(tau_sensor, t_phase):
        x = exp(-(t_phase / tau_sensor))
        return tau_sensor * -ln((1 + x) / Q(2)).raw
    fig("sign_inversion", "µs", _sign_inversion)

    fig("first_read_over_inversion", "×", "first_read_16 / sign_inversion")

# -- Figure 6, the swing, and where the ball signal comes from ----------------
_FIG6 = "Figure 6 of the sensor datasheet does not support that linear scaling"
_SWING = "The swing is the periodic one, not a single step"
_50UA = "Where the ball signal comes from"
_SMALLER = _50UA

fig("fig6_rise_1_to_10k", "×", "fig6_10k / fig6_1k", stated=False)
fig("fig6_4k7_over_1k", "×", "fig6_4k7 / fig6_1k", stated=False)

with MODEL.block("at T", section=_SWING):
    # the swing block writes its intermediate steps without units, and a bare
    # number is addressable even though an expression full of them is not required
    fig("swing_x", "", "exp(-(t_phase / tau_sensor))")
    fig("swing_exp_term", "", "exp(-(first_read_16 / tau_sensor))")
    fig("swing_denominator", "", "1 + swing_x")
    fig("swing_result", "", "Q(swing_sensor.raw)")

fig("gate_edge_in_tau", "τ", "gate_time_irl / tau_sensor",
    group="The swing is the periodic one, not a single step", stated="loose")
with MODEL.block(_50UA, stated="loose"):
    fig("i_led_nominal_sharp", "mA", "u_r_nominal / r_led_sharp",
        moves="+v_mod_nom -vf_nominal_read -r_led_sharp")
    fig("i_photo_scaled_to_char", "µA", "i_photo * (i_f_char / i_led_nominal_sharp)")
    fig("emitter_over_char", "×", "i_led_nominal_sharp / i_f_char")

fig("signal_at_first_read", "mV", "signal_settled * swing_sensor.raw", group=_SMALLER,
    stated="loose")

# -- the swing table ----------------------------------------------------------
# Each row writes its own keys into the formulas, so one loop body serves every row.
for _n, _count, _label in ((3, "n_stock", "3, the stock sensors"),
                           (8, "n_eight", "8")):
    fig(f"budget_{_n}", "µs",
        f"ceil_to({_count}.raw * (t_conversion + t_ovh), 'µs', budget_grain.to('µs'))",
        group=_label, section=PCT96)

for _n, _label in ((3, "3, the stock sensors"), (8, "8"), (16, "16, the design case")):
    with MODEL.block(_label, section=PCT96):
        fig(f"row{_n}_first_read", "µs", f"t_phase - budget_{_n} - t_jitter")
        fig(f"row{_n}_swing", "%", f"_swing(row{_n}_first_read, tau_sensor, t_phase)")
        fig(f"row{_n}_signal", "mV", f"signal_settled * row{_n}_swing.raw")
        fig(f"row{_n}_steps", "steps", f"row{_n}_signal / step_max")

fig("budget_16_row", "µs", "budget_16", group="16, the design case", section=PCT96)

# -- why Pin 1 needs no external resistor -------------------------------------
# Pin 1 of a rebuilt board, where nothing sits between the collector and the rail.
fig("i_c_max_pin1", "mA", "v_out_max / (r_col_new + r_pd)", moves="+v_out_max -r_pd",
    group="I_C,max", section=PIN1)

# -- bounds on the adjustment knobs -------------------------------------------
with MODEL.block(KNOB, section=KNOB):
    fig("u_r_at_i_f_max", "V", "v_out_max - v_f_75c_at_t_a", moves="+v_out_max -vf75_50ma")
    fig("r_min_single", "Ω", "u_r_at_i_f_max / i_f_max_at_t_a", "up",
        moves="+v_out_max +t_a -i_f_knee -vf75_50ma")


def _r_for_supply(i_supply, i_fixed, n, i_c, v_rail, curve):
    """The emitter resistor at which n channels exactly fill i_supply."""
    per = (i_supply - i_fixed - n * i_c) / n
    return (v_rail - interp_log(curve, per)) / per


with MODEL.block(KNOB, section=KNOB):
    fig("knob_r_lower_8_teensy", "Ω", "_r_for_supply(i_teensy_3v3, i_fixed_teensy, n_eight.raw, "
        "i_c, v_out_max, [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])", "up")
    fig("knob_r_lower_16_teensy", "Ω", "_r_for_supply(i_teensy_3v3, i_fixed_teensy, "
        "n_channels.raw, i_c, v_out_max, [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])",
        "up", label="at N = 16, I_supply 250 mA")
    fig("knob_r_lower_16_module", "Ω", "_r_for_supply(i_module, i_fixed_module, n_channels.raw, "
        "i_c, v_out_max, [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])", "up",
        label="the module alone would reach")
    fig("knob_r_upper", "Ω", "u_r_least / i_f_char", "down",
        moves="+v_out_min -i_f_char -vf_table_max")

_PDKNOB = "4.7 kΩ lower"
with MODEL.block(_PDKNOB, section=KNOB):
    fig("knob_pd_50mv", "mV", "i_photo * r_switch")
    fig("step_max", "mV", "v_ref_max / Q(2 ** bits.raw)")
    fig("knob_pd_steps_1k", "steps", "knob_pd_50mv / step_max")
    fig("t_switch_sample", "ns", "r_switch * c_sample")
    fig("knob_pd_acq_wall", "kΩ", "(acq_window / tau_needed.raw - t_switch_sample) / c_node",
        "down", moves="+acq_clocks -f_clk -c_sample")
    fig("knob_pd_ambient_at_wall", "µA", "v_out_min / (r_col + knob_pd_acq_wall)")
    fig("knob_pd_i_c_1k", "mA", "v_out_max / (r_col + r_switch)")
    fig("knob_pd_i_tot_1k", "mA", "i_bus + n_channels.raw * knob_pd_i_c_1k + i_adc_both + i_iso_s2")

fig("knob_gate_lower", "kΩ", "v_mod_nom / i_out_iso", "up", group="R33", section=KNOB)
fig("knob_gate_upper_irl", "kΩ", "gate_charge_limit * v_mod_nom / (2 * qg_irl)", "down",
    group="gate", section=KNOB)
with MODEL.block("100 kΩ lower", section=KNOB):
    fig("knob_gate_at_3k", "V", "v_mod_nom * r_gate_pd_low / (r_gate_pd_low + r_gate)")
    fig("knob_gate_at_10k", "V", "v_mod_nom * r_gate_pd_mid / (r_gate_pd_mid + r_gate)")
    fig("knob_gate_leakage", "mV", "i_gss * r_leak_test")
with MODEL.block("phase", section=KNOB):
    fig("knob_phase_lower", "µs", "budget_16 + tau_ln2", "up",
        moves="+n_channels +t_ovh +tau_sensor -f_clk", label="t_budget plus")
    fig("phases_per_two_values", "", "2 * k_confirm.raw + 1", moves="+k_confirm", stated=False)
    fig("v_hit", "m/s", "ball_diameter / (phases_per_two_values * t_phase)",
        moves="+ball_diameter -t_phase -k_confirm", stated=False)
    fig("v_hit_fallback", "m/s", "ball_diameter / (phases_per_two_values * t_phase_fallback)",
        moves="+ball_diameter -t_phase_fallback -k_confirm", stated=False)

    # how long QuadTimer3 counts before its counter wraps, at the prescaler the driver sets
    fig("t_qt_span", "ms", "qt_range * qt_prescale / f_bus", "down",
        moves="+qt_range +qt_prescale -f_bus", stated=False)
    fig("knob_firmware_bound", "µs", "floor_to(t_phase - tau_ln2, 'µs', budget_grain.to('µs'))",
        "down")
    fig("knob_t_ovh_ceiling", "µs", "knob_firmware_bound / n_channels.raw - t_conversion", "down",
        moves="+t_phase +f_clk -n_channels -tau_sensor", label="t_ovh")
    fig("knob_five_at_750", "ms", "phases_per_two_values * phase_alt")

with MODEL.block("100 Ω", section=KNOB):
    # one standard value inside each of those walls
    fig("i_at_100r", "mA",
        "_i_led_at(r_led_sharp, v_out_max, [(vf75_10ma_at, vf75_10ma), (vf75_50ma_at, vf75_50ma)])")
    fig("p_at_100r", "mW", "i_at_100r ** 2 * r_led_sharp")
with MODEL.block(section=KNOB):
    fig("i_at_390r", "mA", "u_r_least / r_led_alt_high", group="390 Ω")
    fig("signal_at_2k2_std", "mV", "sweep_r_pd_alt_signal", group="2.2 kΩ")
    fig("steps_at_2k2", "steps", "sweep_r_pd_alt_signal / step_max", group="2.2 kΩ")
    fig("window_at_4k7", "τ", "Q(window_in_tau.raw)", group="4.7 kΩ")
    fig("ambient_at_4k7", "µA", "ambient_headroom", group="4.7 kΩ")

# -- the pull-down's upper bound, one row per value ---------------------------
# A single-pole step crosses the sheet's 10 % and 90 % marks ln(9) time
# constants apart, which is how a 10-to-90 % rise time becomes a τ.
fig("rise_to_tau", "", "ln((1 - rise_low.raw) / (1 - rise_high.raw))", stated=False)

# Each row names its resistor by the key of the input that declares it, so no
# resistance is written into a formula.
for _key, _row in (("r_switch", "1 kΩ"), ("r_pd_alt", "2.2 kΩ"),
                   ("r_pd", "4.7 kΩ"), ("r_pd_alt_high", "10 kΩ")):
    with MODEL.block(_row, section=SWEEP):
        fig(f"sweep_{_key}_tau", "µs", f"tau_sensor * ({_key} / r_pd).raw")
        fig(f"sweep_{_key}_swing", "%", f"_swing(first_read_16, sweep_{_key}_tau, t_phase)")
        fig(f"sweep_{_key}_signal", "mV", f"i_photo * {_key} * sweep_{_key}_swing.raw")
        fig(f"sweep_{_key}_ambient", "µA", f"v_out_min / (r_col + {_key})")
        fig(f"sweep_{_key}_window", "τ", f"acq_window / ({_key} * c_node + r_switch * c_sample)")

# -- what the main body states without the appendix ---------------------------
# These sit outside the appendix: the objections table, the troubleshooting
# table and the connector tables. They are stated in prose, so each is located
# by the section that holds it.
_PLACE = "Placement distances"
with MODEL.block(_PLACE, stated="loose"):
    asm("place_decoupling", 10, "mm", src="how close a 100 nF sits to the pin it decouples, a "
        "distance perfboard allows; no datasheet states one")
    asm("place_local", 10, "mm", src="how close a part sits to the node it serves, J-PWR, Q1's "
        "gate and the converters' VDD pins alike; no datasheet states one")
    asm("place_channel", 25, "mm", src="how far a channel's pull-down may sit from its converter "
        "input, so the stub adds little to the source resistance")
asm("ball_diameter", 9, "mm", src="the steel ball the EG01 kit supplies, taken as 9 mm; no "
    "measurement of it is recorded")
dec("v_ball", 3, "m/s", src="the fastest ball this build assumes, which the break beam and the "
    "solenoids' sense lines take as well")
msr("stock_pulse_rate", 333, "Hz", src="the stock mainboard's emitter drive, 1.5 ms on and 1.5 ms "
    "off at the P33 sensor board, docs/research/Rokr/2_ir-reflective-sensor-p33.md")
dec("r_led_brighter", 150, "Ω", src="the E12 value below 220 Ω, for a channel that returns too "
    "little light")
dec("c_bulk_rating", 10, "V", src="the voltage rating asked of C5 and C6, three times the rail")

# A resistor change moves the operating point, so V_F has to be taken at the
# new current. These two are the forward voltages the stated currents imply.
# They are not fresh readings: Figure 3 is only checked for bracketing them.
asm("vf25_at_150", 1.29, "V", src="the forward voltage 13.4 mA at 150 Ω implies. It lies between "
    "Figure 3's 25 °C points at 10 mA and 20 mA, and was not read off the curve")
asm("vf75_at_150", 1.21, "V", src="the forward voltage 14.9 mA at 150 Ω implies. It lies between "
    "Figure 3's 75 °C points at 10 mA and 50 mA, and was not read off the curve")
with MODEL.block("Adjustment knobs", stated="loose"):
    fig("i_led_nominal_150", "mA", "(v_mod_nom - vf25_at_150) / r_led_brighter")
    fig("i_led_worst_150", "mA", "(v_out_max - vf75_at_150) / r_led_brighter")

# The channel ceiling is the divider at V_OUT max, so a different collector load
# moves it. Both alternatives to the stock 1.585 kΩ are quoted by that shift.
# -- the plotted curves, as the points read off them --------------------------
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

MODEL.curve("Figure 1, forward current against ambient temperature",
            [("i_f_knee_at", "i_f_knee"), ("i_f_end_at", "i_f_end")],
            rises=False,
            on=[("t_a", "i_f_max_at_t_a")],
            bounded=[("i_f_knee", "i_f_max", "i_f_max")])

MODEL.curve("Figure 4, collector current against ambient temperature",
            [("t_curve_25", "i_c_at_25c"), ("t_a", "i_c_at_40c")],
            rises=False)

# -- what has to hold whatever the formulas are -------------------------------
# Each of these is a requirement of the design, written without reference to
# any formula above. A slip that satisfies the arithmetic still has to satisfy
# these, and several of them are the safety claims of the whole subsystem.
_I = MODEL.invariant
_I("the phase this design runs still confirms the fastest ball the build assumes",
   "v_hit >= v_ball")
_I("the three emitter cases are ordered", "i_led_least < i_led_nominal < i_led_worst")
_I("the rail brackets its nominal", "v_out_min < v_mod_nom < v_out_max")
_I("the reference brackets, and sits under the rail", "v_ref_min < v_ref_max < v_out_max")
_I("the board fits the module", "i_tot < i_module")
_I("the board alone fits the Teensy's 3V3 pin on the bench", "i_tot_bench < i_teensy_3v3")
_I("both emitter groups survive the maximum their ambient allows",
   "i_led_worst < i_f_max_at_t_a and i_led_worst_sharp < i_f_max_at_t_a")
_I("the phototransistor survives its absolute maximum", "i_c < i_c_abs_max")
_I("the channel node stays inside what the converter tolerates", "u_node_max < v_in_abs_max")
_I("signal and measured ambient together stay inside what the node can hold",
   "i_photo + dark_reading / r_pd < ambient_at_4k7")
_I("the swing is a fraction of the settled signal", "0 < swing_sensor.raw < 1")
_I("the acquisition window covers what ten bits need", "window_in_tau > tau_needed")
_I("the first read sits past the sign inversion", "first_read_16 > sign_inversion")
_I("the phase clears its own lower bound", "knob_phase_lower <= t_phase")
_I("QuadTimer3's counter spans the longest phase the driver may run",
   "t_phase < t_qt_span and t_phase_fallback < t_qt_span")
_I("a DOUT bit is home inside the half period", "dout_round_trip < half_period")
_I("both emitter resistors are above the single-channel bound",
   "r_min_single < r_led_sharp and r_min_single < r_led")
_I("both emitter resistors are below the characterizing point",
   "r_led < knob_r_upper and r_led_sharp < knob_r_upper")
_I("both emitter resistors stay inside the kit's rating",
   "p_r_led < p_kit and p_r_led_sharp < p_kit")
_I("Q1 stays inside its dissipation rating", "t_j_q1 < t_j_max")
_I("the gate draws no more than the isolator output gives", "gate_i_peak <= i_out_iso")
_I("the gate clears the worse of the two thresholds", "v_gate_min > vgs_th_irl_max")
_I("a converter's low is inside the isolator's guaranteed low", "dout_low_level < guaranteed_low")
_I("the rail with the bus on stays above the converter's floor",
   "v_out_min - droop_20k > vdd_floor")
_I("the stated cable allowance is the drop it stands for, to the millivolt",
   "abs(cable_drop - cable_drop_bound) <= Q.of(1, 'mV')")
_I("the module's input survives the worst solenoid sag", "sag_three_coils > v_mod_in_min")
_I("the pull-down is under the acquisition wall", "r_pd < knob_pd_acq_wall")
_I("R1 to R16 sit inside their window, at either value",
   "r_led_min <= r_led_sharp <= r_led_max and r_led_min <= r_led <= r_led_max")
_I("R17 to R32 sit inside their window", "r_pd_min <= r_pd <= r_pd_max")
_I("R33 sits inside its window", "r_gate_min <= r_gate <= r_gate_max")
_I("R34 sits inside its window", "r_miso_min <= r_miso <= r_miso_max")
_I("R35 sits inside its window", "r_gate_pd_min <= r_gate_pd <= r_gate_pd_max")
_I("R36 sits inside its window", "r_dout_pu_min <= r_dout_pu <= r_dout_pu_max")
_I("R37 and R38 sit inside their window", "r_dout_min <= r_dout <= r_dout_max")
_I("R39 sits inside its window", "r_ref_min <= r_ref <= r_ref_max")

_I("the hot forward-voltage curve sits under the cold one at 10 mA", "vf75_10ma < vf25_10ma")
_I("and at 50 mA", "vf75_50ma < vf25_50ma")

# -- what the prose says that the model does not compute ----------------------
# Every other number in the documents has to be a declared quantity. These are
# named so the rest can be checked, and each one says why it is not a figure.
# The ones marked "derived in prose" are a debt against rule 15: they belong in
# the model, and until they are there nothing recomputes them.
for _text, _why in [
    # quoted from a datasheet, at a condition this design does not run
    ("102.2 kΩ", "R33 and R35 in series, the gate divider's total"),
    ("18 A", "the IRL540N row at 25 °C"),
    ("15 A", "the IRL540N row at 100 °C"),
    ("0.044 Ω", "IRL540N R_DS(on) maximum at V_GS = 10 V"),
    ("0.053 Ω", "IRL540N R_DS(on) maximum at V_GS = 5.0 V"),
    ("0.063 Ω", "IRL540N R_DS(on) maximum at V_GS = 4.0 V"),
    ("600 Ω", "the impedance class of a ferrite bead, as a part is specified"),
    ("4 mm", "the distance the GP2S700HCP datasheet characterizes I_C at, against a mirror"),

    # a package, a pitch, a count: not a measured quantity at all
    ("2.54 mm", "the connector pitch"),
    ("5 %", "the share of a phase the startup discards, a plain fraction"),

    # a figure the model holds at another unit or another rounding
    ("3.146", "V_OUT min, restated without its unit"),
    ("3.449", "V_OUT max, restated without its unit"),
    ("0.020", "the IRL540N estimate's drop in volts, restated without its unit"),
    ("1.35", "V_F at 40 mA on the 75 °C curve, restated without its unit"),
    ("53 µs", "the τ reading before it was rounded up to the 70 µs the design runs"),

    # derived in prose, which rule 15 says they should not be
    ("222.6 mA", "derived in prose: what the Teensy case leaves for the emitters"),
    ("482.9 mA", "derived in prose: what the module case leaves for them"),
    ("99.0 mΩ", "derived in prose: the cable resistance the 22 mV allowance reaches"),
    ("0.2 mV", "derived in prose: what 0.1 Ω costs beyond that allowance"),
    ("54 µs", "derived in prose: the excursion C5 leaves R39 and C6 to settle"),
    ("17 mA", "derived in prose: the dark-phase load the reference filter sees"),
    ("111 µs", "derived in prose: the reference filter's time constant"),
    ("7.35 Ω", "derived in prose: the source resistance behind the machine's 5 V"),
    ("2.98 V", "derived in prose: the node ceiling of the rejected Design A"),
    ("2.6 ms", "derived in prose: five time constants of the reference filter"),
]:
    MODEL.aside(_text, _why)

# -- wiring: every net at the Teensy, and the isolator's board-side nets ------
_SVG = "ir-sensor-mainboard.svg"
_ISO_S1 = "design.md, U3: VCC1 from the Teensy's 3V3 over J-T pin 7, so U3 never drives MISO above the Teensy's own rail"
_ISO_S2 = "design.md, U3: VCC2 from the board's rail"

# side 1 of U3 reads the five signals the Teensy drives
for _sig, _ch in (("SCK", "A"), ("MOSI", "B"), ("CS-A", "C"), ("CS-B", "D"), ("CLOCK", "E")):
    _parts = [reads(f"U3 IN{_ch}", src=f"{_SVG}, channel {_ch} of the isolator")]
    if _sig == "SCK":
        _parts.append(reads("the Teensy's LED and its series resistor",
                            src="PJRC schematic41.png, the LED on the SCK pin's net"))
    MODEL.net(f"{_sig}, Teensy side", *_parts, teensy=_sig)

# side 1 of U3 drives MISO: it has no enable pin, so it drives the line whenever the Teensy is powered
MODEL.net("MISO, Teensy side",
          drives("U3 OUTF through R34", "push-pull", rail=TEENSY_RAIL,
                 src=_ISO_S1 + "; SLLSER1H Table 4-1 lists no enable pin"),
          teensy="MISO")

# side 2 of U3 drives the converters and Q1
for _sig, _ch, _to in (("SCK", "A", "U1 and U2 CLK"), ("MOSI", "B", "U1 and U2 DIN"),
                       ("CS-A", "C", "U1 CS"), ("CS-B", "D", "U2 CS")):
    MODEL.net(f"{_sig}, board side", drives(f"U3 OUT{_ch}", "push-pull", rail="the board's 3.3 V", src=_ISO_S2),
              reads(_to, src="design.md, the MCP3008 pin table"))
MODEL.net("CLOCK, board side",
          drives("U3 OUTE", "push-pull", rail="the board's 3.3 V", src=_ISO_S2),
          reads("Q1 gate through R33", src="design.md, R33 limits the current into Q1's gate"),
          pull("R35", "gnd", src="design.md, R35 holds the LED bus off while the board's rail comes up"))

# the converters share DOUT, each driving it only while its own chip select is low
MODEL.net("ADC-DOUT",
          drives("U1 DOUT through R37", "tri-state", rail="3V3_ADC", select="CS-A",
                 src="design.md, both converters release DOUT whenever neither is selected"),
          drives("U2 DOUT through R38", "tri-state", rail="3V3_ADC", select="CS-B",
                 src="design.md, both converters release DOUT whenever neither is selected"),
          pull("R36", "the board's 3.3 V", src="design.md, R36 pulls ADC-DOUT to the board's rail"),
          reads("U3 INF", src=f"{_SVG}, channel F of the isolator"))

MODEL.owns("SPI", "U3 drives MISO whenever the Teensy is powered, so a second device on the bus "
                  "will not work reliably, design.md")
MODEL.owns("QuadTimer3", "channel 2 counts the phases and drives CLOCK, channel 3 starts the read "
                         "block, and all four channels share one interrupt, docs/firmware/drivers/ir-sensing.md")

MODEL.draws("i_iso_s1", pool=TEENSY_RAIL)   # U3's side 1; the board itself runs from the D24V5F3
MODEL.draws(pool="device monitor")   # the sixteen channels, docs/firmware/drivers/ir-sensing.md
