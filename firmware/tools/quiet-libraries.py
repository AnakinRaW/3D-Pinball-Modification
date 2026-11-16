# Keeps the libraries' warnings out of a build, so a build reports only the warnings of the
# firmware's own code. The Teensy platform compiles everything with -Wall.
#
# A file of a library or of the Teensy core compiles with -w. A file of the firmware's own source,
# tests or libraries takes every include folder outside them as a system folder as well, because
# GCC stays silent about a system header. A header of a library then raises no warning in the
# firmware's own files either.
#
# A file of the firmware's own libraries and tests also takes the flags build_src_flags gives the
# source, so DEBUG and the warning flags reach it. PlatformIO itself hands them to the source only,
# and DEBUG has to stay out of the other libraries: freertos-teensy names a constant DEBUG.
#
# A pre script, because the libraries take their build environment from the default one when the
# platform's builder runs, and a middleware added later would not reach them.
from os.path import normcase

Import("env")

# the firmware's own source, its tests and its libraries in firmware/lib, whose folder only the
# project configuration holds
OWN_DIRS = [normcase(env.subst(d)) for d in ("$PROJECT_SRC_DIR", "$PROJECT_TEST_DIR")]
OWN_DIRS.append(normcase(env.GetProjectConfig().get("platformio", "lib_dir")))
SRC_DIR = normcase(env.subst("$PROJECT_SRC_DIR"))

# the flags build_src_flags gives the source, split as SCons splits them
_src_flags = env.GetProjectOption("build_src_flags", "")
SRC_FLAGS = env.ParseFlags(" ".join(_src_flags) if isinstance(_src_flags, list) else _src_flags)


def own(path):
    path = normcase(path)
    return any(path.startswith(d) for d in OWN_DIRS)


def quiet(env, node):
    if not own(node.srcnode().get_abspath()):
        return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-w"])
    folders = [env.Dir(env.subst(d) if isinstance(d, str) else d).get_abspath()
               for d in env.get("CPPPATH", [])]
    system = [flag for d in folders if not own(d) for flag in ("-isystem", d)]
    if normcase(node.srcnode().get_abspath()).startswith(SRC_DIR):
        return env.Object(node, CCFLAGS=env["CCFLAGS"] + system)
    ccflags = list(env["CCFLAGS"]) + list(SRC_FLAGS.get("CCFLAGS", [])) + system
    defines = list(env.get("CPPDEFINES", [])) + list(SRC_FLAGS.get("CPPDEFINES", []))
    return env.Object(node, CCFLAGS=ccflags, CPPDEFINES=defines)


env.AddBuildMiddleware(quiet)
