// The games the simulator lists: each is a folder with a game.yml, on the memory card as /games/<id>/,
// in the repository as games/<id>/, or built into the firmware. This file reads the folders' game.yml
// files far enough to name each game and its top-level sections; the rules themselves come later. A
// picture named thumbnail.png, .jpg or .webp beside the game.yml is the game's thumbnail.
import { parse } from "yaml";

export type GameSource = "card" | "repo" | "firmware";

export interface GameEntry {
  key: string; // source and id, unique across the sources
  id: string; // the folder's name
  source: GameSource;
  path: string; // where the game.yml lies, for the card from its root
  title: string; // the game's name, or its id where the file names none
  sections: string[]; // the file's top-level keys
  text: string;
  thumbnail?: string; // the URL of the game's thumbnail
  error?: string; // why the file could not be read
}

// the games built into the firmware; the firmware holds none yet
export const FIRMWARE_GAMES: GameEntry[] = [];

// a game file's path and a thumbnail's, each with the game's id as the first group
const GAME_FILE = /(?:^|\/)games\/([^/]+)\/game\.yml$/;
const THUMBNAIL = /(?:^|\/)games\/([^/]+)\/thumbnail\.(?:png|jpe?g|webp)$/i;
const slashes = (path: string) => path.split("\\").join("/");

// Reads one game.yml.
export function readGame(source: GameSource, id: string, path: string, text: string): GameEntry {
  const entry: GameEntry = { key: `${source}:${id}`, id, source, path, title: id, sections: [], text };
  try {
    const doc = parse(text);
    if (doc === null || typeof doc !== "object" || Array.isArray(doc)) {
      return { ...entry, error: "game.yml holds no mapping" };
    }
    const d = doc as Record<string, unknown>;
    const name = d.name ?? d.title;
    return { ...entry, title: typeof name === "string" && name ? name : id, sections: Object.keys(d) };
  } catch (e) {
    return { ...entry, error: (e as Error).message.split("\n")[0] };
  }
}

// The games of a set of files, by their paths such as "games/demo/game.yml": every file at
// games/<id>/game.yml is a game, sorted by id. thumbnails holds the URLs of the thumbnails by their
// paths, such as "games/demo/thumbnail.png".
export function gamesFromFiles(
  source: GameSource,
  files: Record<string, string>,
  thumbnails: Record<string, string> = {},
): GameEntry[] {
  const pictures = new Map<string, string>();
  for (const [path, url] of Object.entries(thumbnails)) {
    const m = THUMBNAIL.exec(slashes(path));
    if (m) pictures.set(m[1], url);
  }
  const out: GameEntry[] = [];
  for (const [path, text] of Object.entries(files)) {
    const m = GAME_FILE.exec(slashes(path));
    if (!m) continue;
    const game = readGame(source, m[1], `games/${m[1]}/game.yml`, text);
    out.push(pictures.has(m[1]) ? { ...game, thumbnail: pictures.get(m[1]) } : game);
  }
  return out.sort((a, b) => a.id.localeCompare(b.id));
}

// The part of the browser's File System Access API the card reader uses.
export interface DirectoryHandle {
  kind: "directory";
  name: string;
  values(): AsyncIterable<DirectoryHandle | FileHandle>;
  getDirectoryHandle(name: string): Promise<DirectoryHandle>;
}
export interface FileHandle {
  kind: "file";
  name: string;
  getFile(): Promise<Blob>;
}

// Reads the games of a memory card from the folder picked in the browser: the card's root with its
// games folder, or the games folder itself.
export async function readCardGames(picked: DirectoryHandle): Promise<GameEntry[]> {
  let games = picked;
  if (picked.name !== "games") {
    try {
      games = await picked.getDirectoryHandle("games");
    } catch {
      return [];
    }
  }
  const files: Record<string, string> = {};
  const thumbnails: Record<string, string> = {};
  for await (const folder of games.values()) {
    if (folder.kind !== "directory") continue;
    for await (const file of folder.values()) {
      if (file.kind !== "file") continue;
      const path = `games/${folder.name}/${file.name}`;
      if (file.name === "game.yml") files[path] = await (await file.getFile()).text();
      else if (THUMBNAIL.test(path)) thumbnails[path] = URL.createObjectURL(await file.getFile());
    }
  }
  return gamesFromFiles("card", files, thumbnails);
}
