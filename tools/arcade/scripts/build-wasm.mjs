// Builds the Arcade's WebAssembly module from firmware/sim with Emscripten, CMake and Ninja, and
// copies it to src/wasm/, simulator.mjs and the simulator.wasm it loads, where the app and the tests
// take it.
//
// Emscripten comes from the emsdk in EMSDK, else in %LOCALAPPDATA%\emsdk or ~/emsdk, at the version
// firmware/sim/emsdk-version names. CMake and Ninja come from the PATH or from Python's cmake and
// ninja packages.
import { execFileSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { delimiter, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const sim = join(here, "..", "..", "..", "firmware", "sim");
const build = join(sim, "build");
const out = join(here, "..", "src", "wasm");
const exe = process.platform === "win32" ? ".exe" : "";

function fail(message) {
  console.error(`npm run wasm: ${message}`);
  process.exit(1);
}

const wanted = readFileSync(join(sim, "emsdk-version"), "utf8").trim();
const candidates = [process.env.EMSDK, process.env.LOCALAPPDATA && join(process.env.LOCALAPPDATA, "emsdk"), join(homedir(), "emsdk")];
const emsdk = candidates.find((d) => d && existsSync(join(d, "upstream", "emscripten")));
if (!emsdk) {
  fail(`no emsdk found. Install Emscripten ${wanted}: git clone https://github.com/emscripten-core/emsdk, then emsdk install ${wanted} and emsdk activate ${wanted}`);
}
const emscripten = join(emsdk, "upstream", "emscripten");
const version = readFileSync(join(emscripten, "emscripten-version.txt"), "utf8").trim().replace(/"/g, "");
if (version !== wanted) {
  fail(`the emsdk in ${emsdk} holds Emscripten ${version}, firmware/sim/emsdk-version names ${wanted}: emsdk install ${wanted} and emsdk activate ${wanted}`);
}

// CMake and Ninja from Python's packages, where the PATH lacks them
const paths = [];
for (const [module, attribute] of [["cmake", "CMAKE_BIN_DIR"], ["ninja", "BIN_DIR"]]) {
  try {
    paths.push(execFileSync("python", ["-c", `import ${module}; print(${module}.${attribute})`]).toString().trim());
  } catch {
    // the PATH has to hold it then
  }
}
const env = { ...process.env, PATH: [...paths, process.env.PATH].join(delimiter) };
const run = (file, args) => execFileSync(file, args, { stdio: "inherit", env, cwd: sim });

if (!existsSync(join(build, "build.ninja"))) {
  run(join(emscripten, `emcmake${exe}`), ["cmake", "-G", "Ninja", "-S", ".", "-B", "build"]);
}
run("cmake", ["--build", "build"]);
mkdirSync(out, { recursive: true });
for (const file of ["simulator.mjs", "simulator.wasm"]) copyFileSync(join(build, file), join(out, file));
console.log(`npm run wasm: src/wasm/simulator.mjs and simulator.wasm from Emscripten ${version}`);
