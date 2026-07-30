import { copyFile, mkdir } from "node:fs/promises";
import path from "node:path";

const packageDirectory = path.resolve(import.meta.dirname, "..");
const sourceDirectory = path.join(packageDirectory, "src", "wasm");
const outputDirectory = path.join(packageDirectory, "dist", "wasm");

await mkdir(outputDirectory, { recursive: true });
await Promise.all(
  ["demo.js", "demo.wasm"].map((fileName) =>
    copyFile(path.join(sourceDirectory, fileName), path.join(outputDirectory, fileName)),
  ),
);
