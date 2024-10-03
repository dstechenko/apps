#!/bin/bash
git ls-files | grep -E "\.(c|cpp|h|hpp)$" | xargs clang-format -style=file -i