#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os

from conan import ConanFile
from conan.tools import files


class SqliteConan(ConanFile):
    name = "vitasqlite"
    version = "0.0.2"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "temp-support.patch"
    user = "blastrock"
    channel = "pkgj"

    def source(self):
        source_url = "https://github.com/VitaSmith/libsqlite"
        commit = "9d524c1186f0343735f027a22fb7ed864587a2d8"
        self.run("git clone %s" % source_url)
        self.run("git checkout %s" % commit, cwd="libsqlite")

        files.download(
            self,
            "https://sqlite.org/2023/sqlite-amalgamation-3420000.zip",
            "sqlite.zip",
        )
        files.unzip(self, "sqlite.zip")

    def build(self):
        self.run("make -C libsqlite/libsqlite")

    def package(self):
        files.copy(
            self,
            "*.h",
            src="libsqlite/libsqlite",
            dst=os.path.join(self.package_folder, "include/psp2"),
            keep_path=False,
        )
        files.copy(
            self,
            "sqlite3.h",
            src="sqlite-amalgamation-3420000",
            dst=os.path.join(self.package_folder, "include"),
            keep_path=False,
        )
        files.copy(
            self,
            "*.a",
            src=".",
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )

    def package_info(self):
        self.cpp_info.libs = ["sqlite"]
        self.cpp_info.system_libs = ["SceSqlite_stub"]
