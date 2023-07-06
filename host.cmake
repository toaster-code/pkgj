find_package(SQLite3 REQUIRED)

add_executable(pkgj_cli
  src/comppackdb.cpp
  src/db.cpp
  src/download.cpp
  src/extractzip.cpp
  src/filedownload.cpp
  src/patchinfo.cpp
  src/simulator.cpp
  src/aes128.cpp
  src/sfo.cpp
  src/sha256.cpp
  src/filehttp.cpp
  src/zrif.cpp
  src/puff.c
  src/cli.cpp
)

target_link_libraries(pkgj_cli
  fmt::fmt
  Boost::headers
  SQLite::SQLite3
  cereal::cereal
  libzip::zip
)
