#!/bin/bash

set -xe

poetry install

# create home dirs
poetry run conan

if ! [[ -e $HOME/.conan2/profiles/vita ]] ; then
  mkdir -p $HOME/.conan2/profiles
  cp -f conan/profiles/vita $HOME/.conan2/profiles
fi

if [[ -e $HOME/.conan2/settings.yml ]]; then
  if ! grep PSVita $HOME/.conan2/settings.yml ; then
    echo "Your ~/.conan2/settings.yml does not contain PSVita, you must update it manually with ci/conan2/settings.yml"
    exit 1
  fi
else
  cp conan/settings.yml $HOME/.conan2/settings.yml
fi

poetry run conan export conan-vitasdk
poetry run conan export conan-fmt
poetry run conan export conan-libzip
poetry run conan export conan-vitasqlite
