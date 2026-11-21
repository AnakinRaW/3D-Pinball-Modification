import { execSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";
import preact from "@preact/preset-vite";

// The app reads the drawing, the feature texts and the README from the repository, two folders up.
const repoRoot = fileURLToPath(new URL("../..", import.meta.url));

function commit(): string {
  try {
    return execSync("git rev-parse --short HEAD", { cwd: repoRoot }).toString().trim();
  } catch {
    return "unknown";
  }
}

export default defineConfig({
  plugins: [preact()],
  define: { __COMMIT__: JSON.stringify(commit()) },
  // relative paths, so the built page opens from any folder
  base: "./",
  // the drawing is bundled into the page as text, about 0.4 MB of it
  build: { chunkSizeWarningLimit: 1500 },
  server: { port: 5173, strictPort: true, fs: { allow: [repoRoot] } },
  test: {
    include: ["tests/**/*.test.{ts,tsx}"],
    environment: "node",
  },
});
