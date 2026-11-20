// The app's light and dark theme. Without a choice the app follows the system's setting; a choice
// in the top bar sets data-theme on the page and is remembered in this browser.
export type Theme = "light" | "dark";

const KEY = "pinball-simulator-theme";

export function storedTheme(): Theme | null {
  try {
    const v = localStorage.getItem(KEY);
    return v === "light" || v === "dark" ? v : null;
  } catch {
    return null;
  }
}

export function systemTheme(): Theme {
  return globalThis.matchMedia?.("(prefers-color-scheme: light)").matches ? "light" : "dark";
}

// Sets the theme on the page and remembers it, or follows the system again for null.
export function applyTheme(theme: Theme | null) {
  const root = document.documentElement;
  if (theme) root.dataset.theme = theme;
  else delete root.dataset.theme;
  try {
    if (theme) localStorage.setItem(KEY, theme);
    else localStorage.removeItem(KEY);
  } catch {
    // a browser without storage keeps the choice until the page reloads
  }
}
