#!/usr/bin/env bash
# Source this file in bash/zsh. Only this shell is changed; no profile edits.
# Override AI_PASSPORT_IDF_ROOT or IDF_TOOLS_PATH for a different installation.
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-${HOME}/.espressif}"
if [[ -d /opt/homebrew/opt/python@3.12/libexec/bin ]]; then
    export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:${PATH}"
fi
if [[ ! -f "${AI_PASSPORT_IDF_ROOT:-${HOME}/esp/esp-idf-v5.5.3}/export.sh" ]]; then
    printf '%s\n' "ESP-IDF 5.5.3 is missing; set AI_PASSPORT_IDF_ROOT to its installation." >&2
    return 1
fi
source "${AI_PASSPORT_IDF_ROOT:-${HOME}/esp/esp-idf-v5.5.3}/export.sh" || return 1
if [[ "$(idf.py --version)" != "ESP-IDF v5.5.3" ]]; then
    printf '%s\n' "Expected ESP-IDF v5.5.3; refusing to build with another version." >&2
    return 1
fi
