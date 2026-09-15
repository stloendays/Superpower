/**
 * Minimal build-time compatibility globals for browser content-script code.
 *
 * The content bundle historically uses Node-style names for timer handles,
 * NODE_ENV checks, and a few legacy require() calls. Pulling all of
 * @types/node into this browser TypeScript project also replaces DOM timer
 * return types with NodeJS.Timeout, which conflicts with code that correctly
 * stores browser timer IDs as numbers.
 *
 * Keep only the browser/bundler-facing surface used by this package. The Vite
 * config is type-checked separately with full Node types in tsconfig.vite.json.
 */
declare namespace NodeJS {
  type Timeout = number;
}

declare const process: {
  env: Record<string, string | undefined> & {
    NODE_ENV?: 'development' | 'production' | 'test' | string;
  };
};

declare function require<T = any>(moduleId: string): T;
