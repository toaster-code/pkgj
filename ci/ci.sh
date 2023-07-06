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

cd ..

if [[ -n "$TRAVIS_TAG" ]]; then
    SSH_FILE="$(mktemp -u $HOME/.ssh/XXXXX)"

    # Decrypt SSH key
    openssl aes-256-cbc \
        -K $encrypted_593a42f38d5b_key\
        -iv $encrypted_593a42f38d5b_iv\
        -in ".travis_deploy_key.enc" \
        -out "$SSH_FILE" -d

    # Enable SSH authentication
    chmod 600 "$SSH_FILE"
    printf "%s\n" \
        "Host github.com" \
        "  IdentityFile $SSH_FILE" \
        "  LogLevel ERROR" >> ~/.ssh/config

    git config --global user.name "Travis"
    git config --global user.email "travis"

    git remote set-url origin git@github.com:blastrock/pkgj.git

    git fetch origin last:refs/remotes/origin/last --depth 1
    git checkout -b last origin/last

    # skip the 'v' in the tag
    ./release.sh ${TRAVIS_TAG:1}
fi
