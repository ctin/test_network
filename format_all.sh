#!/bin/sh

find . -not -path "./submodules/*" -regex '.*\.\(cpp\|hpp\|cc\|cxx\|c\)' -exec clang-format --verbose -style=file -i {} \;
