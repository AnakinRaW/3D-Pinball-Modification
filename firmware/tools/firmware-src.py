# Picks the files of firmware/src that a project takes when it uses the firmware's code as the
# library firmware, as the test image and the bench programs do. Without an option it takes every
# file but main.cpp, whose setup() and loop() the project brings itself. A project that needs only
# some of the code names its files in custom_firmware_src, in the form of build_src_filter, such as
# +<core/clock.cpp>, so a bench program builds without the libraries the rest of the firmware needs.
Import("env")

_filter = env.GetProjectOption("custom_firmware_src", "")
env.Replace(SRC_FILTER=_filter if _filter.strip() else ["+<*>", "-<main.cpp>"])
