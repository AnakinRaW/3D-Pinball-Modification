// The repository files the app reads, bundled as text by Vite.
import playfieldSvg from "../../../assets/playfield/playfield.svg?raw";
import playfieldJson from "../../../assets/playfield/playfield.json?raw";
import featuresMd from "../../../docs/features.md?raw";
import readmeMd from "../../../README.md?raw";
import configYml from "../config/config.yml?raw";
import machineYml from "../config/machine.yml?raw";

// every game of the repository, games/<id>/game.yml, by its path
const repoGames = import.meta.glob<string>("../../../games/*/game.yml", { query: "?raw", import: "default", eager: true });

// their thumbnails, by path, as the URLs the page loads them from
const repoThumbnails = import.meta.glob<string>("../../../games/*/thumbnail.{png,jpg,jpeg,webp}", {
  query: "?url",
  import: "default",
  eager: true,
});

export const sources = { playfieldSvg, playfieldJson, featuresMd, readmeMd, configYml, machineYml, repoGames, repoThumbnails };
export type Sources = typeof sources;
