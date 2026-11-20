// The repository files the app reads, bundled as text by Vite.
import playfieldSvg from "../../../assets/playfield/playfield.svg?raw";
import playfieldJson from "../../../assets/playfield/playfield.json?raw";
import featuresMd from "../../../docs/features.md?raw";
import readmeMd from "../../../README.md?raw";
import configYml from "../config/config.yml?raw";
import machineYml from "../config/machine.yml?raw";

export const sources = { playfieldSvg, playfieldJson, featuresMd, readmeMd, configYml, machineYml };
export type Sources = typeof sources;
