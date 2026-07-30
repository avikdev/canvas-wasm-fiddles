// Run with:
// bazel run //:wasm_exporter -- ../canvas-worker/src/wasm

import fs from "node:fs/promises";
import path from "node:path";
import process from "node:process";

const pathParams = {
  runfiles: process.env.JS_BINARY__RUNFILES ?? "",
  workspace: process.env.JS_BINARY__WORKSPACE ?? "",
  buildWorkingDirectory: process.env.BUILD_WORKING_DIRECTORY ?? "",
  wasmTarget: "demo_wasm",
  ccTarget: "demo",
};

async function requireFile(filePath) {
  const stats = await fs.stat(filePath);
  if (!stats.isFile()) {
    throw new Error(`Expected a file at ${filePath}`);
  }
}

function resolveSourceFiles() {
  const { runfiles, workspace, wasmTarget, ccTarget } = pathParams;
  if (!runfiles || !workspace) {
    throw new Error("Bazel runfiles environment is unavailable.");
  }

  const outputDirectory = path.join(runfiles, workspace, wasmTarget);
  return [
    path.join(outputDirectory, `${ccTarget}.js`),
    path.join(outputDirectory, `${ccTarget}.wasm`),
  ];
}

async function resolveDestination() {
  const requestedDestination = process.argv[2];
  if (!requestedDestination) {
    throw new Error(
      "Missing destination. Usage: bazel run //:wasm_exporter -- <destination>",
    );
  }

  const destination = path.isAbsolute(requestedDestination)
    ? requestedDestination
    : path.resolve(pathParams.buildWorkingDirectory, requestedDestination);
  await fs.mkdir(destination, { recursive: true });
  return destination;
}

async function main() {
  const sourceFiles = resolveSourceFiles();
  const destination = await resolveDestination();

  await Promise.all(sourceFiles.map(requireFile));
  for (const sourceFile of sourceFiles) {
    const destinationFile = path.join(destination, path.basename(sourceFile));
    await fs.rm(destinationFile, { force: true });
    await fs.copyFile(sourceFile, destinationFile);
    console.log(`[wasm-export] ${destinationFile}`);
  }
}

await main();
