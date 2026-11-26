# Fails a Teensy 4.1 build whose RAM1 keeps less than RAM1_FREE_MIN_BYTES free for local
# variables, the figure teensy_size prints. That room lies between the static variables and the top
# of RAM1, and the stacks grow into it: the interrupts' stack of configMAIN_STACK_DEPTH, and in the
# firmware the main task's stack below it. An image past the floor still links, and a deep
# nesting of interrupts then runs into the guard behind the variables and faults.
#
# The room is _estack minus _ebss, the two symbols PJRC's linker script sets: _ebss ends the
# static variables, and _estack sits below the 32 KB banks of RAM1 that ITCM takes for code.
import os
import shutil
import subprocess

Import("env")

# the floor: the interrupts' stack of 8 KB that firmware/include/freertos_config_override.h sets
# as configMAIN_STACK_DEPTH, and as much again for what a branch adds before it merges
RAM1_FREE_MIN_BYTES = 16 * 1024
KB = 1024


def symbols(elf):
    """Gets the values of _ebss and _estack from the image."""
    # OBJCOPY names the toolchain's arm-none-eabi-objcopy, which nm stands beside, on the path
    # PlatformIO gives the build.
    objcopy = env.subst("$OBJCOPY")
    nm = shutil.which(objcopy[: -len("objcopy")] + "nm", path=env["ENV"]["PATH"])
    out = subprocess.run([nm, elf], capture_output=True, text=True, check=True).stdout
    found = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in ("_ebss", "_estack"):
            found[parts[2]] = int(parts[0], 16)
    return found


def check(target, source, env):
    elf = target[0].get_abspath()
    found = symbols(elf)
    if "_ebss" not in found or "_estack" not in found:
        print(f"ram1-floor: {os.path.basename(elf)} names no _ebss or _estack")
        return 1
    free = found["_estack"] - found["_ebss"]
    if free < RAM1_FREE_MIN_BYTES:
        print(f"ram1-floor: RAM1 keeps {free} bytes ({free / KB:.1f} KB) free for local "
              f"variables, below the floor of {RAM1_FREE_MIN_BYTES} bytes "
              f"({RAM1_FREE_MIN_BYTES // KB} KB)")
        return 1
    print(f"ram1-floor: RAM1 keeps {free} bytes free for local variables, the floor is "
          f"{RAM1_FREE_MIN_BYTES}")
    return 0


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf",
                  env.Action(check, "Checking the room RAM1 keeps for local variables"))
