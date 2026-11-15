# Keeps the libraries' warnings out of a build, so a build reports only the warnings of the
# firmware's own code. The Teensy platform compiles everything with -Wall.
#
# A file of a library or of the Teensy core compiles with -w. A file of the firmware's own source,
# tests or libraries takes every include folder outside them as a system folder as well, because
# GCC stays silent about a system header. A header of a library then raises no warning in the
# firmware's own files either.
#
# A pre script, because the libraries take their build environment from the default one when the
# platform's builder runs, and a middleware added later would not reach them.
from os.path import normcase

Import("env")

# the firmware's own source, its tests and its libraries in firmware/lib, whose folder only the
# project configuration holds
OWN_DIRS = [normcase(env.subst(d)) for d in ("$PROJECT_SRC_DIR", "$PROJECT_TEST_DIR")]
OWN_DIRS.append(normcase(env.GetProjectConfig().get("platformio", "lib_dir")))


def own(path):
    path = normcase(path)
    return any(path.startswith(d) for d in OWN_DIRS)


def quiet(env, node):
    if not own(node.srcnode().get_abspath()):
        return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-w"])
    folders = [env.Dir(env.subst(d) if isinstance(d, str) else d).get_abspath()
               for d in env.get("CPPPATH", [])]
    system = [flag for d in folders if not own(d) for flag in ("-isystem", d)]
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + system)


env.AddBuildMiddleware(quiet)
