#!/bin/bash

set -xe

cd ci

rm -fr build buildhost

export CC=gcc-12
export CXX=g++-12

./setup_conan.sh

mkdir buildhost
cd buildhost
poetry run conan install ../.. --build missing -s build_type=RelWithDebInfo -s compiler=gcc -s compiler.version=12 -s compiler.libcxx=libstdc++11 --output-folder .
poetry run conan build ../.. -s build_type=RelWithDebInfo -s compiler=gcc -s compiler.version=12 -s compiler.libcxx=libstdc++11 --output-folder .
cd ..

mkdir build
cd build
poetry run conan install ../.. --build missing -s build_type=RelWithDebInfo --profile:host vita --output-folder .
poetry run conan build ../.. -s build_type=RelWithDebInfo --profile:host vita --output-folder .
cp pkgj pkgj.elf
cd ..
