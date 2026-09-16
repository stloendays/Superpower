#!/bin/bash

# Legacy shell helper retained for contributors who prefer Bash.
# The cross-platform project setup uses scripts/copy-env.mjs.
if [ ! -f ".env" ] && [ -f ".env.example" ]; then
  cp .env.example .env
  echo ".env.example has been copied to .env"
fi
