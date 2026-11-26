# Converts the host screens' fonts before a build that compiles the host's views, through
# tools/fonts.py, which converts only when a font in firmware/fonts/ or the script changed since the
# last conversion. A build that has to convert without Node.js stops with what is missing.
#
# A pre script, so the fonts stand in firmware/src/host/views/fonts/ before PlatformIO collects the
# files it compiles.
import sys
from inspect import currentframe, getframeinfo
from os.path import abspath, dirname, join

Import("env")

sys.path.insert(0, abspath(join(dirname(getframeinfo(currentframe()).filename), "..", "..", "tools")))
import fonts  # noqa: E402

try:
    if fonts.ensure():
        print("generate-fonts.py: converted the host screens' fonts")
except fonts.MissingTool as e:
    sys.stderr.write("generate-fonts.py: " + str(e) + "\n")
    env.Exit(1)
