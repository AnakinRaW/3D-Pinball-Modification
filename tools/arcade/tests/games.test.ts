// Reads games from game.yml files and from a memory card's folder.
import { describe, expect, it } from "vitest";
import { gamesFromFiles, readCardGames, readGame, type DirectoryHandle, type FileHandle } from "../src/model/games";

const file = (name: string, text: string): FileHandle => ({ kind: "file", name, getFile: async () => new Blob([text]) });
const dir = (name: string, entries: (DirectoryHandle | FileHandle)[]): DirectoryHandle => ({
  kind: "directory",
  name,
  values: async function* () {
    yield* entries;
  },
  getDirectoryHandle: async (n: string) => {
    const d = entries.find((e) => e.kind === "directory" && e.name === n);
    if (!d) throw new Error("not found");
    return d as DirectoryHandle;
  },
});

describe("a game.yml", () => {
  it("names the game and its sections", () => {
    const g = readGame("repo", "circus", "games/circus/game.yml", "name: Circus\nvariables: {}\nmodes: {}\n");
    expect(g).toMatchObject({ key: "repo:circus", title: "Circus", sections: ["name", "variables", "modes"] });
    expect(g.error).toBeUndefined();
  });

  it("takes the folder's name where the file names no game", () => {
    expect(readGame("repo", "circus", "games/circus/game.yml", "modes: {}\n").title).toBe("circus");
  });

  it("reports a file it cannot read", () => {
    expect(readGame("repo", "a", "games/a/game.yml", "modes: [unclosed\n").error).toBeTruthy();
    expect(readGame("repo", "b", "games/b/game.yml", "- just\n- a list\n").error).toBe("game.yml holds no mapping");
  });

  it("takes only files at games/<id>/game.yml, sorted by id", () => {
    const games = gamesFromFiles("repo", {
      "../../../games/zoo/game.yml": "name: Zoo",
      "../../../games/arena/game.yml": "name: Arena",
      "../../../games/arena/lights/attract.lights": "steps: []",
      "../../../other/game.yml": "name: Other",
    });
    expect(games.map((g) => g.id)).toEqual(["arena", "zoo"]);
    expect(games[0].path).toBe("games/arena/game.yml");
  });

  it("gives a game the thumbnail of its folder", () => {
    const games = gamesFromFiles(
      "repo",
      { "../../../games/arena/game.yml": "name: Arena", "../../../games/zoo/game.yml": "name: Zoo" },
      { "../../../games/arena/thumbnail.webp": "/assets/arena.webp", "../../../games/zoo/cover.png": "/assets/zoo.png" },
    );
    expect(games.map((g) => g.thumbnail)).toEqual(["/assets/arena.webp", undefined]);
  });
});

describe("a memory card", () => {
  const card = dir("SDCARD", [
    file("config.yml", "lighting: {}"),
    dir("games", [dir("circus", [file("game.yml", "name: Circus")]), dir("empty", [file("notes.txt", "")])]),
  ]);

  it("lists the games of the card's games folder", async () => {
    const games = await readCardGames(card);
    expect(games.map((g) => [g.key, g.title])).toEqual([["card:circus", "Circus"]]);
  });

  it("takes the games folder itself as well", async () => {
    const games = await readCardGames(await card.getDirectoryHandle("games"));
    expect(games.map((g) => g.id)).toEqual(["circus"]);
  });

  it("takes a thumbnail beside a game.yml as the game's picture", async () => {
    const withPicture = dir("games", [dir("circus", [file("game.yml", "name: Circus"), file("thumbnail.png", "png")])]);
    const [game] = await readCardGames(withPicture);
    expect(game.thumbnail).toMatch(/^blob:/);
  });

  it("finds no games on a card without a games folder", async () => {
    expect(await readCardGames(dir("USB", [file("readme.txt", "")]))).toEqual([]);
  });
});
