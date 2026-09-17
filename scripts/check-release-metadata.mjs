import { existsSync, readFileSync } from 'node:fs';

function fail(message) {
  console.error(`[release-metadata] ${message}`);
  process.exit(1);
}

const packageJson = JSON.parse(readFileSync('package.json', 'utf8'));
const version = String(packageJson.version || '').trim();

if (!/^\d+\.\d+\.\d+$/.test(version)) {
  fail(`package.json version must be a Chrome-compatible x.y.z version, received: ${version || '<empty>'}`);
}

const cmake = readFileSync('desktop/qt/CMakeLists.txt', 'utf8');
const cmakeChecks = [
  'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../package.json" SUPERPOWER_PACKAGE_JSON)',
  'string(JSON SUPERPOWER_DESKTOP_VERSION GET "${SUPERPOWER_PACKAGE_JSON}" version)',
  'project(SuperpowerDesktop VERSION ${SUPERPOWER_DESKTOP_VERSION} LANGUAGES CXX)',
];

for (const expected of cmakeChecks) {
  if (!cmake.includes(expected)) {
    fail(`desktop/qt/CMakeLists.txt no longer derives the desktop version from package.json: missing ${expected}`);
  }
}

for (const manifestPath of ['chrome-extension/manifest.ts', 'chrome-extension/manifest.js']) {
  const manifestSource = readFileSync(manifestPath, 'utf8');
  if (!manifestSource.includes('version: packageJson.version')) {
    fail(`${manifestPath} must derive version from package.json`);
  }
  if (!manifestSource.includes('version_name: `V${packageJson.version}`')) {
    fail(`${manifestPath} must derive version_name from package.json`);
  }
  if (manifestSource.includes('mcpsuperassistant.ai')) {
    fail(`${manifestPath} still contains the legacy MCP SuperAssistant identity`);
  }
}

if (existsSync('dist/manifest.json')) {
  const builtManifest = JSON.parse(readFileSync('dist/manifest.json', 'utf8'));
  if (builtManifest.version !== version) {
    fail(`built extension version ${builtManifest.version} does not match package.json ${version}`);
  }
  if (builtManifest.version_name !== `V${version}`) {
    fail(`built extension version_name ${builtManifest.version_name} does not match V${version}`);
  }
}

console.log(`[release-metadata] Superpower ${version} metadata is consistent.`);
