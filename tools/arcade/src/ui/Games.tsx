// The games of the three sources: the memory card the browser opens, the firmware and the repository.
// The Games view shows them as covers below the picked game; the studio lists them to pick the game it
// edits.
import { useState } from "preact/hooks";
import { readCardGames, type DirectoryHandle, type GameEntry, type GameSource } from "../model/games";
import type { State } from "../state";

const SOURCES: { source: GameSource; label: string }[] = [
  { source: "card", label: "Memory card" },
  { source: "firmware", label: "Firmware" },
  { source: "repo", label: "Repository" },
];

// the browser's folder picker, which Chrome and Edge offer
type Picker = () => Promise<DirectoryHandle>;
const picker = (): Picker | undefined => (globalThis as { showDirectoryPicker?: Picker }).showDirectoryPicker;

async function openCard(state: State) {
  const pick = picker();
  if (!pick) return;
  try {
    state.cardGames.value = await readCardGames(await pick());
  } catch {
    // the picker was closed without a folder
  }
}

// A game's picture: its thumbnail, or its initials on a color its id picks.
export function GameThumb({ game, size = "large" }: { game: GameEntry; size?: "large" | "small" }) {
  if (game.thumbnail) {
    return <img class={`game-thumb ${size}`} src={game.thumbnail} alt="" />;
  }
  const hash = gameHue(game);
  const initials = game.title
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((w) => w[0]!.toUpperCase())
    .join("");
  return (
    <span
      class={`game-thumb placeholder ${size}`}
      style={{ background: `hsl(${hash} 55% 40%)` }}
    >
      {initials}
    </span>
  );
}

// the games of every source as tiles, the picked one marked; a click picks a game
export function GameList({ state, compact = false, onPick }: { state: State; compact?: boolean; onPick?: () => void }) {
  const games = state.games.value;
  const selected = state.selectedGame.value;
  return (
    <div class={`game-list${compact ? " compact" : ""}`}>
      {SOURCES.map(({ source, label }) => {
        const mine = games.filter((g) => g.source === source);
        return (
          <section key={source} data-source={source}>
            <h3>
              {label}
              {source === "card" && (
                <button class="small" data-action="open-card" disabled={!picker()} onClick={() => void openCard(state)}>
                  Open…
                </button>
              )}
            </h3>
            {mine.length === 0 ? (
              <p class="muted none">No games</p>
            ) : (
              <div class="game-tiles">
                {mine.map((g) => (
                  <button
                    key={g.key}
                    class={`game-tile${g.key === selected ? " selected" : ""}`}
                    data-game={g.key}
                    onClick={() => {
                      state.selectedGame.value = g.key;
                      onPick?.();
                    }}
                  >
                    <GameThumb game={g} size={compact ? "small" : "large"} />
                    <span class="game-title">{g.title}</span>
                    {g.error && <span class="game-error">unreadable</span>}
                  </button>
                ))}
              </div>
            )}
          </section>
        );
      })}
    </div>
  );
}

// the color a game's placeholder and glow take, picked by its id
export function gameHue(game: GameEntry): number {
  let hash = 0;
  for (const ch of game.id) hash = (hash * 31 + ch.charCodeAt(0)) % 360;
  return hash;
}

type Filter = "all" | GameSource;
const FILTERS: { value: Filter; label: string }[] = [
  { value: "all", label: "All" },
  { value: "card", label: "Memory card" },
  { value: "firmware", label: "Firmware" },
  { value: "repo", label: "Repository" },
];

// The Games view over the whole screen: the picked game large on top, every game of the chosen source as
// a cover below it, the sources to choose from in a bar above.
export function GamesView({ state }: { state: State }) {
  const [filter, setFilter] = useState<Filter>("all");
  const games = state.games.value;
  const shown = filter === "all" ? games : games.filter((g) => g.source === filter);
  const game = state.game.value ?? shown[0] ?? null;
  return (
    <main class="games-view">
      <div class="games-bar">
        <nav class="filter-chips">
          {FILTERS.map((f) => {
            const n = f.value === "all" ? games.length : games.filter((g) => g.source === f.value).length;
            return (
              <button
                key={f.value}
                class={`filter-chip${filter === f.value ? " on" : ""}`}
                data-filter={f.value}
                onClick={() => setFilter(f.value)}
              >
                {f.label}
                <span class="count">{n}</span>
              </button>
            );
          })}
        </nav>
        <button class="open-card" data-action="open-card" disabled={!picker()} onClick={() => void openCard(state)}>
          Open memory card
        </button>
      </div>
      <section class="games-stage">
        <GameHero game={game} state={state} />
        {shown.length > 0 && (
          <div class="cover-grid">
            {shown.map((g) => (
              <button
                key={g.key}
                class={`cover${g.key === game?.key ? " selected" : ""}`}
                data-game={g.key}
                style={{ "--hue": String(gameHue(g)) }}
                onClick={() => (state.selectedGame.value = g.key)}
              >
                <GameThumb game={g} />
                <span class={`badge cover-badge ${g.source}`}>{SOURCE_NAMES[g.source]}</span>
                <span class="cover-info">
                  <span class="game-title">{g.title}</span>
                </span>
                {g.error && <span class="game-error">unreadable</span>}
              </button>
            ))}
          </div>
        )}
      </section>
    </main>
  );
}

const SOURCE_NAMES: Record<GameSource, string> = { card: "Memory card", firmware: "Firmware", repo: "Repository" };

// The picked game across the top of the Games view, or the way to a card while no game is found.
function GameHero({ game, state }: { game: GameEntry | null; state: State }) {
  if (!game) {
    return (
      <div class="game-hero empty">
        <div class="hero-body">
          <p class="hero-kicker">Pinball Arcade</p>
          <h1>No games yet</h1>
          <button class="hero-play" data-action="open-card" disabled={!picker()} onClick={() => void openCard(state)}>
            Open memory card
          </button>
        </div>
      </div>
    );
  }
  const hue = gameHue(game);
  return (
    <div class="game-hero" data-hero={game.key} style={{ "--hue": String(hue) }}>
      <div class="hero-body">
        <span class={`badge ${game.source}`}>{SOURCE_NAMES[game.source]}</span>
        <h1>{game.title}</h1>
        <p class="hero-path">
          <code>{game.path}</code>
        </p>
        {game.error ? (
          <p class="errors">{game.error}</p>
        ) : (
          <div class="chips">
            {game.sections.map((s) => (
              <span key={s} class="chip">
                {s}
              </span>
            ))}
          </div>
        )}
        <div class="hero-actions">
          <button class="hero-play" disabled title="Comes with the game loader">
            ▶ Play
          </button>
          <button class="hero-rules" disabled title="Comes with the game loader">
            Rules
          </button>
        </div>
      </div>
      <div class="hero-art">
        <GameThumb game={game} />
      </div>
    </div>
  );
}
