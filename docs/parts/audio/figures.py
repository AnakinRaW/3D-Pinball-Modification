"""The figures of the audio output, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "audio", drawings=["audio-schematic.svg"],
                 documents=["../../firmware/drivers/audio.md", "../../firmware/drivers/storage.md",
                            "../../firmware/testing.md", "../../firmware/file-channel.md",
                            "../../firmware/components/sounds.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

AMP = "MAX98357A-MAX98357B-Maxim.pdf"
LEVEL = "Speaker level"
INPUTS = "Digital inputs"
SUPPLY = "Supply"

# -- the speaker level --------------------------------------------------------
with MODEL.block("LEVEL", section=LEVEL):
    ds("dac_fs", 2.1, "dB", sheet=AMP, src="Gain Selection: the full-scale output of the DAC, in "
       "dB above 1 V rms, which the selected gain is added to")
    dec("gain", 3, "dB", src="the lowest of the five steps in Table 8 of "
        "MAX98357A-MAX98357B-Maxim.pdf, GAIN_SLOT connected to VDD through 100 kΩ")
    ds("r_gain_table", 100, "kΩ", sheet=AMP, src="Table 8, the resistor it names for the 3 dB step",
       stated=False)
    ds("r_gain_tol", 5, "%", sheet=AMP, src="Table 8, the tolerance asked of that resistor",
       stated=False)
    dec("r_gain", 100, "kΩ", src="R4, from GAIN to VCC: the 100 kΩ of the resistor kit",
        stated=False)
    ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, "
       "Botland product page: tolerance 5 %", stated=False)
    msr("r_spk", 8, "Ω", src="the stock speaker's impedance, measured, "
        "docs/research/Rokr/5_sound.md")
    asm("p_spk", 0.5, "W", src="the lower end of the 0.5 W to 1 W that "
        "docs/research/Rokr/5_sound.md estimates from the speaker's size; the rating read off the "
        "speaker replaces it")
    ds("n_emi_in", 12, "", sheet=AMP, src="Figure 14, EMI with 12in of Speaker Cable and No Output "
       "Filtering: the speaker cable of Maxim's emission measurement, in inches", stated=False)
    dec("cm_per_in", 2.54, "cm", src="the inch, defined as 2.54 cm", stated=False)

    # the longest speaker cable the emission measurement covers, printed down
    fig("l_spk_max", "cm", "n_emi_in * cm_per_in", "down", moves="+n_emi_in +cm_per_in",
        stated=False)
    fig("v_fs", "V", "Q.of(1, 'V') * from_db(dac_fs + gain)", moves="+dac_fs +gain")
    fig("p_fs", "W", "v_fs * v_fs / r_spk", moves="+dac_fs +gain -r_spk")

    # A square wave at full scale carries its peak as its rms value, twice the power
    # of a sine at the same peak. It is the loudest a digital signal can be.
    fig("p_square", "W", "2 * p_fs", moves="+dac_fs +gain -r_spk")
    fig("g_cap", "", "sqrt(p_spk / p_square)", "down", moves="+p_spk +r_spk -dac_fs -gain")

# -- the digital inputs and the rail ------------------------------------------
with MODEL.block("INPUTS", section=INPUTS):
    MODEL.uses("v_3v3", of="teensy-4.1")
    ds("v_ih", 1.3, "V", sheet=AMP, src="Electrical Characteristics, Input High Voltage of the "
       "digital audio inputs")
    ds("v_in_max", 6, "V", sheet=AMP, src="Absolute Maximum Ratings, VDD, LRCLK, BCLK and DIN to "
       "GND, which does not depend on VDD")
dec("v_dd", 5, "V", src="the machine's 5 V from the power distribution, the rail the stock "
    "amplifier drove the speaker from", group="SUPPLY", section=SUPPLY, stated=True)
ds("gain_frac_max", 0.85, "", sheet=AMP, src="GAIN_SLOT comparator trip points, the upper edge of "
   "the 3 dB window, 0.85 x VDD")

# Table 8 asks for its resistor within r_gain_tol, so R4's own band has to sit inside that
# band around the value Table 8 names.
fig("r_gain_min", "kΩ", "r_gain_table * (1 - r_gain_tol) / (1 - kit_tol)", "up",
    moves="+r_gain_table +kit_tol -r_gain_tol", stated=False)
fig("r_gain_max", "kΩ", "r_gain_table * (1 + r_gain_tol) / (1 + kit_tol)", "down",
    moves="+r_gain_table +r_gain_tol -kit_tol", stated=False)
fig("v_gain_max", "V", "gain_frac_max * v_dd", moves="+gain_frac_max +v_dd", group="INPUTS",
    section=INPUTS)
with MODEL.block("SUPPLY", section=SUPPLY):
    fig("v_peak", "V", "v_fs * sqrt(2)", moves="+dac_fs +gain")
    ds("i_q", 3.35, "mA", sheet=AMP, src="Electrical Characteristics, Quiescent Current, the "
       "maximum at VDD = 5 V")
    asm("eta", 80, "%", src="the amplifier's efficiency at the capped output. The datasheet gives "
        "92 % into 8 Ω at 1 W, a class D stage loses efficiency toward lower output, and 80 % is "
        "taken under that without reading the curve")
    fig("i_amp", "mA", "p_spk / (eta * v_dd) + i_q", "up", moves="+p_spk +i_q -eta -v_dd")

# -- the audio stream, as PJRC's Audio library sets it ------------------------
STREAM = "Audio stream"
with MODEL.block("STREAM", section=STREAM):
    dec("f_s", 44.1, "kHz", src="the Audio library's sample rate on the Teensy 4.x, "
        "AUDIO_SAMPLE_RATE_EXACT in AudioStream.h")
    dec("n_block", 128, "samples", src="the Audio library's block, AUDIO_BLOCK_SAMPLES in "
        "AudioStream.h")
    dec("n_bclk", 64, "bits", src="the bits AudioOutputI2S2 clocks per frame, two slots of "
        "32 bits, I2S_TCR4 and I2S_TCR5 in output_i2s2.cpp")
    ds("n_bclk_amp", 64, "bits", sheet=AMP, src="Electrical Characteristics, BCLK must be 32, 48 "
       "or 64 times LRCLK; the largest of the three", stated=False)
    ds("f_bclk_min", 0.2432, "MHz", sheet=AMP, src="Electrical Characteristics, BCLK Frequency "
       "Range, minimum")
    ds("f_bclk_max", 25.804, "MHz", sheet=AMP, src="Electrical Characteristics, BCLK Frequency "
       "Range, maximum")
    fig("t_block", "ms", "n_block / f_s", moves="+n_block -f_s")
    fig("f_bclk", "MHz", "f_s * n_bclk", moves="+f_s +n_bclk")

# -- the stream buffers -------------------------------------------------------
BUFFERS = "Stream buffers"
with MODEL.block("BUFFERS", section=BUFFERS):
    MODEL.uses("ram2", of="teensy-4.1")
    dec("n_streams", 4, "", src="kChannels in docs/firmware/drivers/audio.md, the four inputs of "
        "the driver's mixer", stated=False)
    dec("buf", 32, "KB", src="the ring buffer each channel gets in RAM2")
    dec("chunk", 4, "KB", src="what one card read brings into a channel's ring, kChunk in "
        "docs/firmware/drivers/audio.md")
    MODEL.as_written("chunk_bytes", of="chunk", unit="bytes")
    MODEL.as_written("buf_bytes", of="buf", unit="bytes")
    dec("b_sample", 2, "bytes", src="one 16-bit mono sample, the PCM form the channels carry")

    # A chunk is read once it has been played out of the ring, so a channel always
    # holds at least the rest of its buffer.
    fig("t_ahead", "ms", "(buf - chunk) / b_sample / f_s", "down",
        moves="+buf -chunk -b_sample -f_s")

    # The channels' reads go before every other request on the card, so when all channels wait
    # at once the last one is served after the other three.
    fig("t_read_max", "ms", "t_ahead / n_streams", "down",
        moves="+buf -chunk -b_sample -f_s -n_streams")
    ds("n_out_held", 2, "", src="block_left_1st and block_left_2nd in PJRC's "
       "Audio/output_i2s2.cpp: the block the output is sending and the next one; left and right "
       "share one block, both fed from the master gain", stated=False)
    ds("b_block", 260, "bytes", src="audio_block_t in PJRC's cores/teensy4/AudioStream.h: "
       "128 samples of 2 bytes behind a header of 4 bytes; AudioMemory() places the blocks in "
       "DMAMEM, RAM2", stated=False)

    # the audio blocks AudioMemory() reserves, kBlocks in docs/firmware/drivers/audio.md: each channel
    # fills one per round, the mixer and the master gain write into the block they receive, and the
    # output holds the blocks it still sends
    fig("n_blocks", "", "n_streams + n_out_held", moves="+n_streams +n_out_held", stated=False)
    fig("ram_blocks", "KB", "n_blocks * b_block", moves="+n_blocks +b_block", stated=False)
    fig("ram_streams", "KB", "buf * n_streams", "up", moves="+buf +n_streams")

fig("n_chunks", "", "buf / chunk", moves="+buf -chunk", stated=False)

# -- the card, stated in docs/firmware/drivers/storage.md ---------------------
SD = "SD Specifications Part 1, Physical Layer Simplified Specification, version 3.01"
dec("card_app_class", 1, "", src="the lowest SD Application Performance Class the build accepts, "
    "A1, a decision: the card load in docs/parts/display/design.md rests on the random reads of "
    "4 KB an A1 card guarantees. An A2 card is accepted as well. SdFat uses neither its command "
    "queue nor its cache, and the SD Association states that an A2 card then still delivers at "
    "least A1 performance, www.sdcard.org/developers/overview/application/")
ds("line", 32, "bytes", src="the data cache line of the Cortex-M7, the unit arm_dcache_delete() in "
   "PJRC's imxrt.h clears the cache in, and the alignment of a channel's ring")
ds("sector", 512, "bytes", src=f"{SD}, 5.3.3: READ_BL_LEN and WRITE_BL_LEN of the CSD version "
   "2.0 fixed at 9, so an SDHC or SDXC card reads and writes blocks of 512 bytes, the sector the "
   "storage driver moves")
ds("t_rd_block", 100, "ms", src=f"{SD}, 4.6.2.1: the read timeout of SDHC and SDXC cards, the "
   "longest a card may take for a block it reads")
ds("t_wr_block", 500, "ms", src=f"{SD}, 4.6.2.2: the longest busy an SDXC card may show after a "
   "block it writes, 250 ms for an SDHC card; the application note asks a host to wait longer than "
   "500 ms")
dec("n_open", 16, "", src="kOpen in docs/firmware/drivers/storage.md, the files the storage task "
    "keeps open at once")
dec("t_probe", 1, "s", src="how often the storage task asks a failed card for its status, kProbeMs "
    "in docs/firmware/drivers/storage.md")
MODEL.as_written("t_probe_ms", of="t_probe", unit="ms")
dec("bounce", 4, "KB", src="kBounce in docs/firmware/drivers/storage.md, the read buffer in RAM1 "
    "that a read lands in when it may not land straight in its buffer")
dec("stack_storage", 8, "KB", src="kStack in docs/firmware/drivers/storage.md, the storage task's "
    "stack; the size freertos-teensy's example/sdfat gives the task that runs SdFat, 2048 words")
MODEL.as_written("stack_storage_bytes", of="stack_storage", unit="bytes")
msr("stack_storage_need", 1.66, "KB", src="the storage task's deepest call chain in a test build "
    "of the documents' code with Teensyduino 1.62.0: gcc's -fcallgraph-info=su frames summed along "
    "the linked ELF's calls, with the hardware and PendSV frames on top")
dec("n_path", 64, "bytes", src="kPath in docs/firmware/drivers/storage.md, the longest path with "
    "its end")
dec("t_pause", 50, "ms", src="kPauseMs in docs/firmware/drivers/storage.md: the pause before a "
    "transfer that follows a failed one, which lets the main loop make a pass between two of "
    "SdFat's waits")
dec("t_land", 100, "ms", src="kLandMs in docs/firmware/drivers/storage.md: the longest the storage "
    "task lets a transfer that still moves data finish before it resets the data line, the wait "
    "Linux's drivers/mmc/host/sdhci-esdhc-imx.c gives this controller")
MODEL.as_written("t_pause_ms", of="t_pause", unit="ms")
MODEL.as_written("t_land_ms", of="t_land", unit="ms")
MODEL.uses("prio_kernel", of="firmware")
MODEL.uses("t_movie", of="display")      # a video frame, which the video player tests in testing.md name

# the end of the path takes one byte of kPath
fig("n_path_chars", "", "n_path / Q.of(1, 'bytes') - 1", moves="+n_path", stated=False)
ds("t_sdfat", 1, "s", src="BUSY_TIMEOUT_MICROS in SdFat's src/SdCard/SdioTeensy.cpp, PJRC's copy: "
   "the longest SdFat waits for the card before the call fails")

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("a full-scale sine stays under the power the speaker is assumed to take", "p_fs < p_spk")
_I("the cap holds the loudest digital signal to that power", "g_cap * g_cap * p_square <= p_spk")
_I("the rail does not clip a full-scale signal", "v_peak < v_dd")
_I("R4's tolerance band sits inside the band Table 8 asks of the resistor it names", "r_gain * (1 "
   "- kit_tol) >= r_gain_table * (1 - r_gain_tol) and r_gain * (1 + kit_tol) <= r_gain_table * (1 "
   "+ r_gain_tol)")
_I("the Teensy's high level switches the amplifier's inputs", "v_3v3 > v_ih")
_I("and stays inside their rating, powered or not", "v_3v3 < v_in_max")
_I("the port's bit clock lies in the amplifier's range", "f_bclk_min < f_bclk < f_bclk_max")
_I("and its frame length is one the amplifier accepts", "n_bclk <= n_bclk_amp <= n_bclk")
_I("the stream buffers leave most of RAM2 to the rest of the firmware",
   "ram_streams < ram2 / Q.of(2, '')")
_I("a buffer holds more than the chunk it is refilled with", "chunk < buf")
_I("the storage task's stack holds its deepest measured call chain",
   "stack_storage_need < stack_storage")
_I("a working card reads and writes every block before SdFat gives up waiting",
   "max(t_rd_block, t_wr_block) < t_sdfat")

# -- wiring -------------------------------------------------------------------
# the module's I²S inputs only read, and nothing on them pulls toward its 5 V: R1 on GAIN is
# the module's only resistor, and the MAX98357A draws at most 1 µA at DIN, BCLK and LRCLK
_MOD = ("DFR0954 schematic, R1 the module's only resistor; MAX98357A-MAX98357B-Maxim.pdf, "
        "input current within ±1 µA")
for _sig, _pin in (("DIN to the DFR0954", "DIN"), ("LRC to the DFR0954", "LRC"),
                   ("BCLK to the DFR0954", "BCLK")):
    MODEL.net(_pin, reads(f"DFR0954 {_pin}", src=_MOD), teensy=_sig)
MODEL.net("MCLK", teensy="MCLK")   # the port drives it, and nothing connects to it, docs/pin-assignment.md

dec("vol_music", 50, "%", src="the music's starting volume, audio.music_volume's default in docs/firmware/components/sounds.md, "
    "a decision: under the effects")
dec("vol_effects", 70, "%", src="the effects' starting volume, audio.effects_volume's default in "
    "docs/firmware/components/sounds.md, a decision")
_I("music and one effect at their starting volumes, squared by the component's curve, never "
   "overdrive the output together", "vol_music.raw ** 2 + vol_effects.raw ** 2 <= 1")

MODEL.aside("1 W", "the upper end of the rating docs/research/Rokr/5_sound.md estimates for the speaker")

MODEL.owns("I²S2", "PJRC's AudioOutputI2S2 drives the port by DMA, docs/firmware/drivers/audio.md")

MODEL.draws("ram_streams", pool="RAM2")
MODEL.draws("ram_blocks", pool="RAM2")
MODEL.draws(pool="device monitor")   # the SD card
