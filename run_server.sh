#!/usr/bin/env bash

cd "$(dirname "$0")"

if [ -f "run_pyenv.sh" ]; then
    . ./run_pyenv.sh
    python -m cuwo.server
else
    /usr/bin/env python3.6 -m cuwo.server
fi
