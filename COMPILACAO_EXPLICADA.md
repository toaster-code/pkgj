# Guia de Compilação — Para quem está aprendendo C++

Este documento explica, passo a passo e em linguagem simples, **o que acontece quando você compila o PKGj**. Nenhum conhecimento prévio de build systems é necessário.

---

## Índice

1. [O que é "compilar"?](#1-o-que-é-compilar)
2. [Os dois estágios: compilar e linkar](#2-os-dois-estágios-compilar-e-linkar)
3. [O que são headers (.hpp) e arquivos fonte (.cpp)?](#3-o-que-são-headers-hpp-e-arquivos-fonte-cpp)
4. [O que é CMake?](#4-o-que-é-cmake)
5. [O que é Conan?](#5-o-que-é-conan)
6. [O que é Poetry?](#6-o-que-é-poetry)
7. [O que é VitaSDK?](#7-o-que-é-vitasdk)
8. [O que é cross-compilation?](#8-o-que-é-cross-compilation)
9. [O que é uma biblioteca estática (.a)?](#9-o-que-é-uma-biblioteca-estática-a)
10. [O que é um ELF, SELF e eboot.bin?](#10-o-que-é-um-elf-self-e-ebootbin)
11. [O que é um VPK?](#11-o-que-é-um-vpk)
12. [O fluxo completo, do código ao VPK](#12-o-fluxo-completo-do-código-ao-vpk)
13. [Por que o build host (pkgj_cli) existe?](#13-por-que-o-build-host-pkgj_cli-existe)
14. [Glossário rápido](#14-glossário-rápido)

---

## 1. O que é "compilar"?

Você escreve código em C++ porque é legível para humanos:

```cpp
std::string nome = "PKGj";
fmt::print("Olá, {}!\n", nome);
```

O processador do seu computador (ou do PS Vita) **não entende isso**. Ele só entende sequências de bytes em binário — zeros e uns. **Compilar** é o processo de traduzir o código C++ legível para esse binário.

```
Código C++ (texto)  →  [compilador]  →  Binário (bytes)
```

O programa que faz essa tradução chama-se **compilador**. Neste projeto usamos dois compiladores:

| Compilador | Gera binário para... |
|---|---|
| `g++-12` (GCC 12) | Linux x86_64 (o seu PC) |
| `arm-vita-eabi-g++` (GCC 10.3) | PS Vita (ARM) |

---

## 2. Os dois estágios: compilar e linkar

A compilação acontece em **duas fases**:

### Fase 1 — Compilação (`.cpp` → `.o`)

Cada arquivo `.cpp` é compilado **separadamente** para um arquivo **objeto** (`.o`). O arquivo objeto contém o código em binário daquele arquivo, mas ainda com "buracos" — referências a funções definidas em outros arquivos.

```
src/pkgi.cpp      →  pkgi.cpp.obj
src/db.cpp        →  db.cpp.obj
src/gameview.cpp  →  gameview.cpp.obj
src/annotationdb.cpp → annotationdb.cpp.obj
... (mais 30+ arquivos)
```

### Fase 2 — Linkagem (`.o` + `.a` → executável)

O **linker** pega todos os arquivos objeto e "cola" tudo junto, preenchendo os buracos — conectando cada chamada de função à sua implementação real. O resultado é o executável final.

```
pkgi.cpp.obj
db.cpp.obj        →  [linker]  →  eboot.bin (ou pkgj_cli)
libfmt.a
libsqlite.a
...
```

> **Analogia:** Imagine que cada `.cpp` é um capítulo de um livro escrito por autores diferentes. A compilação é cada autor escrevendo seu capítulo. A linkagem é o editor que junta todos os capítulos em um livro só.

---

## 3. O que são headers (.hpp) e arquivos fonte (.cpp)?

Em C++ é convenção separar a **declaração** da **implementação**:

| Arquivo | Contém | Exemplo |
|---|---|---|
| `.hpp` (header) | "O que existe" — nomes, tipos, assinaturas de funções | `void pkgi_apply_annotations();` |
| `.cpp` (source) | "Como funciona" — o código real de cada função | `void pkgi_apply_annotations() { for(...) ... }` |

O header é como uma **ementa de restaurante** (lista o que existe). O `.cpp` é a **cozinha** (onde de facto está feito).

Quando um arquivo `.cpp` escreve `#include "annotationdb.hpp"`, ele está dizendo: *"quero usar as coisas declaradas aqui"*.

---

## 4. O que é CMake?

Imagine que você tem 40 arquivos `.cpp` e precisa compilar cada um com os parâmetros certos, na ordem certa, e depois linkar tudo. Fazer isso manualmente na linha de comando seria impossível.

**CMake** é um gerador de build system. Você descreve o projeto no arquivo `CMakeLists.txt`:

```cmake
add_executable(pkgj
    src/pkgi.cpp
    src/db.cpp
    src/annotationdb.cpp
    # ... 30+ arquivos
)
target_link_libraries(pkgj fmt::fmt libzip::zip ...)
```

CMake lê esse arquivo e gera um sistema de build (neste projeto usa **Ninja**), que sabe exatamente quais comandos rodar, em que ordem, e quais arquivos precisam ser recompilados quando você muda alguma coisa.

```
CMakeLists.txt  →  [cmake]  →  build.ninja  →  [ninja]  →  eboot.bin
```

---

## 5. O que é Conan?

O projeto PKGj depende de várias **bibliotecas externas** — código pronto escrito por outras pessoas:

- `fmt` — formatação de strings (como printf mas moderno)
- `libzip` — leitura de arquivos ZIP
- `boost` — utilitários gerais
- `imgui` — interface gráfica
- `cereal` — serialização de dados
- `vitasqlite` — banco de dados SQLite para o Vita

Baixar, compilar e configurar cada uma dessas bibliotecas manualmente seria dias de trabalho. **Conan** é o gerenciador de pacotes do C++ — faz exatamente o que o `npm` faz para JavaScript ou o `pip` para Python.

```bash
conan install ..   # baixa e compila todas as dependências
conan build ..     # compila o projeto principal usando as dependências
```

Cada dependência é descrita no arquivo `conanfile.py` na raiz do projeto.

---

## 6. O que é Poetry?

**Poetry** é um gerenciador de ambientes Python. O Conan é uma ferramenta Python, e para garantir que todos usem a mesma versão do Conan (sem conflitos com outros projetos Python no sistema), ele roda dentro de um ambiente virtual gerenciado pelo Poetry.

```
Poetry  →  cria ambiente virtual Python  →  instala Conan dentro dele
```

É por isso que todos os comandos Conan são prefixados com `poetry run`:

```bash
poetry run conan install ..   # roda o conan do ambiente virtual, não o do sistema
```

---

## 7. O que é VitaSDK?

O PS Vita tem um processador **ARM** (arquitetura completamente diferente do x86_64 do seu PC). O VitaSDK é um pacote que contém:

| Componente | O que faz |
|---|---|
| `arm-vita-eabi-gcc` | Compilador C para ARM (Vita) |
| `arm-vita-eabi-g++` | Compilador C++ para ARM (Vita) |
| `arm-vita-eabi-ld` | Linker para ARM |
| Headers do sistema Vita | Arquivos `.h` com as funções do sistema operacional do Vita (`SceIo`, `SceNet`, etc.) |
| Bibliotecas do Vita | `libSceGxm.a`, `libvita2d.a`, etc. — código da Sony |
| `vita-elf-create` | Converte ELF para o formato que o Vita entende |
| `vita-pack-vpk` | Empacota tudo em `.vpk` |

Instalado em: `/root/vitasdk/`

---

## 8. O que é cross-compilation?

**Cross-compilation** = compilar em uma máquina para rodar em outra máquina diferente.

```
Máquina de desenvolvimento       Máquina alvo
  Linux x86_64 (PC)    →    PS Vita ARM Cortex-A9
```

O compilador `g++-12` gera binários para **x86_64** (roda no seu PC).  
O compilador `arm-vita-eabi-g++` também roda no seu PC, mas gera binários para **ARM** (roda no Vita).

Por isso o Conan tem dois perfis (profiles):

- **Perfil `default`** — descreve o PC onde você compila (Linux, x86_64, GCC 12)
- **Perfil `vita`** — descreve o alvo (PSVita, ARM, GCC 10.3)

Quando você passa `--profile:host vita`, o Conan usa o cross-compiler do VitaSDK para compilar **tudo** — inclusive as bibliotecas dependentes (fmt, libzip, etc.) — para ARM.

---

## 9. O que é uma biblioteca estática (.a)?

Uma biblioteca estática é um arquivo que agrupa vários arquivos `.o` numa espécie de "arquivo compactado de código objeto". A extensão no Linux é `.a` (de *archive*).

Exemplos produzidos durante a build:

```
libfmt.a       (código de formatação de strings)
libz.a         (código de compressão zlib)
libzip.a       (código de leitura de ZIP)
libsqlite.a    (código do banco de dados SQLite)
```

Quando o linker cria o executável final, ele **copia** o código necessário de cada `.a` diretamente para dentro do binário. O resultado é um executável **autossuficiente** — não precisa de nada mais para rodar.

> **Contraste:** Bibliotecas dinâmicas (`.so` no Linux, `.dll` no Windows) não são copiadas para dentro do executável — ficam separadas. O Vita não suporta `.so`, então aqui tudo é `.a`.

---

## 10. O que é um ELF, SELF e eboot.bin?

### ELF — Executable and Linkable Format

É o formato padrão de executável no Linux e em muitos sistemas embarcados. O arquivo `pkgj` produzido pelo linker é um ELF ARM.

```
arm-vita-eabi-g++ ... -o pkgj   ← este é um ELF
```

O ELF ainda **não roda no Vita** — o sistema operacional do Vita (OrbisOS/HENkaku) exige um formato especial.

### SELF — Signed ELF

A Sony criou o formato **SELF** que é essencialmente um ELF com uma assinatura criptográfica por cima. A ferramenta `vita-elf-create` (parte do VitaSDK) converte o ELF para SELF.

### eboot.bin

É o nome que a Sony dá ao SELF principal de um aplicativo. É o arquivo que o Vita realmente executa quando você abre um app.

```
pkgj (ELF)  →  vita-elf-create  →  eboot.bin (SELF)
```

---

## 11. O que é um VPK?

Um **VPK** (*Vita Package*) é essencialmente um arquivo ZIP com uma estrutura específica. Contém tudo que é necessário para instalar um homebrew no Vita:

```
pkgj.vpk
├── eboot.bin          ← o executável (SELF)
├── sce_sys/
│   ├── icon0.png      ← ícone do app
│   ├── param.sfo      ← metadados (título, versão, Title ID)
│   └── livearea/      ← tela do LiveArea (o menu do Vita)
│       └── contents/
│           ├── bg0.png
│           ├── startup.png
│           └── template.xml
└── assets/            ← recursos do app (shaders, etc.)
    ├── imgui_v.cg
    └── imgui_f.cg
```

Para instalar: copie o `.vpk` para o Vita e abra com o VitaShell.

---

## 12. O fluxo completo, do código ao VPK

Aqui está **tudo** o que acontece quando você roda `conan build`:

```
┌─────────────────────────────────────────────────────────┐
│  1. CMake lê CMakeLists.txt e cross.cmake               │
│     → descobre todos os .cpp que precisam compilar      │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│  2. Ninja orquestra a compilação em paralelo (-j4)      │
│     arm-vita-eabi-g++ -c src/pkgi.cpp -o pkgi.cpp.obj   │
│     arm-vita-eabi-g++ -c src/db.cpp   -o db.cpp.obj     │
│     arm-vita-eabi-g++ -c src/annotationdb.cpp -o ...    │
│     ... (38 arquivos no total)                          │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│  3. Linkagem                                            │
│     arm-vita-eabi-g++ *.obj libfmt.a libzip.a ...       │
│     → pkgj  (ELF ARM, ~26 MB com símbolos de debug)     │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│  4. vita-elf-create pkgj eboot.bin                      │
│     → converte ELF → SELF (assina o executável)         │
│     → eboot.bin (~1.5 MB, sem símbolos de debug)        │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│  5. vita-pack-vpk pkgj.vpk                              │
│     → empacota eboot.bin + assets/ + sce_sys/           │
│     → pkgj.vpk (~1.5 MB, pronto para instalar)         │
└─────────────────────────────────────────────────────────┘
```

---

## 13. Por que o build host (pkgj_cli) existe?

Cross-compilar e transferir para o Vita para testar é lento. Por isso o projeto tem uma segunda "versão" do programa — o **simulador host** — que roda diretamente no Linux.

O arquivo `src/simulator.cpp` substitui todas as funções específicas do Vita (como `sceIoOpen` para abrir arquivos, `sceNetHttpRequest` para fazer downloads) por equivalentes POSIX padrão do Linux.

```
Vita:  src/vitafile.cpp  (usa sceIo*)
Host:  src/simulator.cpp (usa fopen, fread, ...)
```

O CMake escolhe qual usar baseado no perfil:

- **Perfil `vita`** → inclui `vitafile.cpp`, `vitahttp.cpp`, `vita.cpp` → gera `pkgj.vpk`
- **Perfil `default`** (host) → inclui `simulator.cpp`, `cli.cpp` → gera `pkgj_cli`

Com `pkgj_cli` você pode testar a lógica de parsing de TSV, extração de ZIP, etc., sem precisar do hardware do Vita:

```bash
./pkgj_cli refreshlist PSVGAMES jogos.tsv   # testa parsing de base de dados
./pkgj_cli extractzip  patch.zip            # testa extração de ZIP
```

---

## 14. Glossário rápido

| Termo | Significado |
|---|---|
| **Compilador** | Programa que traduz C++ para binário |
| **Linker** | Programa que junta os binários em um executável final |
| **CMake** | Gerador de build system — lê `CMakeLists.txt` e sabe como compilar tudo |
| **Ninja** | Build system rápido — executa os comandos que o CMake gerou |
| **Conan** | Gerenciador de pacotes/dependências para C++ |
| **Poetry** | Gerenciador de ambiente Python (usado para isolar o Conan) |
| **VitaSDK** | Toolchain de cross-compilação para PS Vita |
| **Cross-compilation** | Compilar em uma arquitetura para rodar em outra (x86 → ARM) |
| **ELF** | Formato padrão de executável no Linux/ARM |
| **SELF** | ELF assinado criptograficamente — formato que o Vita executa |
| **eboot.bin** | Nome do SELF principal de um app no Vita |
| **VPK** | Pacote ZIP instalável no Vita (contém eboot.bin + assets) |
| **`.a` (static lib)** | Arquivo com código objeto agrupado, incorporado no executável final |
| **`.hpp` header** | Arquivo com declarações — diz "o que existe" |
| **`.cpp` source** | Arquivo com implementações — diz "como funciona" |
| **Anonymous namespace** | `namespace { }` sem nome em C++ — torna os símbolos internos ao arquivo |
| **Forward declaration** | Declarar uma função antes de defini-la — diz ao compilador "ela existe, vem depois" |
| **`-j4`** | Flag do ninja/make — compila 4 arquivos em paralelo (usa 4 núcleos da CPU) |
| **Profile (Conan)** | Arquivo que descreve a plataforma alvo (OS, arquitetura, compilador) |
| **`arm-vita-eabi`** | Prefixo das ferramentas do VitaSDK — indica: ARM, Vita, EABI (convenção de chamada) |
