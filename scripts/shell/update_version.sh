#!/bin/bash
set -euo pipefail

# Usage: pnpm update-version <new_version>
# Product versions must remain Chrome-compatible x.y.z values.
new_version="${1:-}"

if [[ ! "$new_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Version format <${new_version}> isn't correct; expected <0.0.0>." >&2
  exit 1
fi

node --input-type=module - "$new_version" <<'NODE'
import { readFileSync, writeFileSync } from 'node:fs';

const newVersion = process.argv[2];
const packagePath = 'package.json';
const packageJson = JSON.parse(readFileSync(packagePath, 'utf8'));
const previousVersion = packageJson.version;

packageJson.version = newVersion;
writeFileSync(packagePath, `${JSON.stringify(packageJson, null, 2)}\n`);

console.log(`Updated Superpower product version: ${previousVersion} -> ${newVersion}`);
NODE

echo "Qt Desktop and extension manifests derive their release version from package.json."
echo "Run 'pnpm build && pnpm check:release-metadata' before publishing."
