import { copyFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';

const root = process.cwd();
const envPath = resolve(root, '.env');
const examplePath = resolve(root, '.env.example');

if (!existsSync(envPath) && existsSync(examplePath)) {
  copyFileSync(examplePath, envPath);
  console.log('.env.example has been copied to .env');
} else if (existsSync(envPath)) {
  console.log('.env already exists; leaving it unchanged');
} else {
  console.log('No .env.example found; skipping .env creation');
}
