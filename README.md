# PFS — Protected File System

[![CI](https://github.com/asnikas/PFS/actions/workflows/ci.yml/badge.svg)](https://github.com/asnikas/PFS/actions/workflows/ci.yml)

PFS is a file system written in C++17 that operates inside a single binary image file (`disk.img`, minimum size 4096 bytes) and is managed through an interactive shell. It has users, access control lists on files and directories, optional file encryption, nested directories, and its own block allocator.

The goal of this project was to understand how a file system is structured on a disk, how space is allocated and freed and how access control works.

## Features

- **On-disk format** A packed binary layout: superblock, descriptors, tables and data blocks. The superblock carries a magic number that also identifies the format version, so a foreign or old-format image is refused instead of being misread.
- **Block allocator** Every block has a header and a footer. Allocation is first-fit with block splitting; freeing merges neighbouring free blocks, zeroes (replaces information with NUL bytes) the released data, and gives a free block at the end of the heap back to the untouched tail. Invalid or repeated `free` is rejected.
- **Users and access control** An administrator is created together with the image; more users are added with `register`. Files carry read/write ACLs, directories carry read/write/traverse ACLs. Rights are granted by user name with `set_acl`. Entering a directory needs the traverse right; listing it needs the read right.
- **Files** Regular files and encrypted files (each file has a data stream (MAIN) and, for encrypted files, a key stream (SYMKEY)). Encryption is a simple XOR cipher, see [Limitations](#limitations).
- **Directories and paths** Nested directories, `cd`, `cd ..`, `pwd`, a prompt that shows the user and the current path, and `copy` / `move` by path, including whole directory trees (a directory cannot be moved into itself).
- **Host files** `import` and `export` move any file (text, images, audio, etc.) between the image and the real disk.
- **Diagnostics** `stats` reports free and used blocks, the largest free block, service overhead and fragmentation. `get_info` shows size, ACL and dates of a file, and the ACL of a directory.
- **Failure handling** Operations that allocate several blocks roll back (free the already allocated memory) when something fails, for example when the image is full, so a failed operation does not leak space or damage existing data.
- **Shell** `help`, `login` / `logout`, a coloured prompt and coloured errors (only in a terminal, never in redirected output).

## Build

You need a C++17 compiler (GCC, Clang or MinGW). CMake is optional.

With CMake:

```
cmake -S . -B build
cmake --build build
```

Without CMake:

```
g++ -std=c++17 -o pfs *.cpp
```

## Run

```
./pfs
```

The image `disk.img` is created in the current directory. A session example:

```
mkfs 200000 admin          # create a 200000-byte image with administrator "admin"
login admin
register bob
mkdir docs 1 1 1           # name, read, write, traverse
cd docs
create note.txt 1 1        # name, read, write
write note.txt hello
read note.txt
set_acl bob docs 1 0 1     # bob may enter and list "docs", not modify it
set_acl bob note.txt 1 0   # bob may read the file, not write it
pwd
cd ..
stats
exit
```

Type `help` for the full command list. Things worth knowing:

- Names may contain letters, digits, `.`, `_` and `-` (up to 31 characters).
- `cd` works from the current directory. The `<path>` argument of `copy` and `move` is a full path from the root, for example `docs/secret`, or `/` for the root.
- `import <path> [name]` and `export <path> <name>` take a path on the real disk. Put it in quotes and use `/` inside: `"C:/My Files/a.jpg"`.
- `write` replaces the whole content of a file.
- Before `login` only `login`, `register`, `mkfs`, `help` and `exit` are available.
- Text is UTF-8. On Windows the program switches the console to UTF-8 itself.

## Tests

Tests are scenarios that run the system: a scenario is a list of commands plus a list of expected fragments of the output. The runner can restart the program in the middle of a scenario, which checks that data really survives on disk.

```
python tests/run_tests.py                 # build and run every scenario
python tests/run_tests.py -v <name>       # one scenario, with the program output
ctest --test-dir build                    # the same through CMake
```

The scenario format is described at the top of `tests/run_tests.py`. The scenarios in `tests/scenarios/` cover regular files with a restart, encrypted files, and access rights on directories. CI builds and runs them on Linux in two modes: a normal build and a build with AddressSanitizer and UndefinedBehaviorSanitizer.

## Project layout

All sources are in the repository root; `Name.*` stands for `Name.h` and `Name.cpp`.

```
├── main.cpp                           # entry point: mounts the image, starts the shell
├── Menu.*                             # interactive shell: command parsing, prompt, help, colors
├── FileSystem.*                       # file system operations (mkfs, login, create, copy, move, ...)
├── Resolver.*                         # path resolution with traverse-rights checks
├── Allocator.*                        # block allocator (header/footer, first-fit, split, merge)
├── Parser.*                           # raw reads and writes of the image file
├── Superblock.*                       # image header: root, users table, magic number, size
├── Descriptor.h                       # base of all on-disk object descriptors
├── CommonFileDescriptor.*             # regular file
├── EncryptedFileDescriptor.*          # encrypted file
├── FolderDescriptor.*                 # directory: listing, copy, move, remove
├── FileTable.*, StreamTable.*         # directory entries, data streams of a file
├── UsersTable.*, User.*               # users of the image
├── FileACList.*, FolderACList.*       # access control lists of files and directories
├── *AccessEntry.*, AuthorizedUsers.*, 
    TableNode.*, StreamDescriptor.*    # records stored in those tables
├── Encryption.*                       # XOR cipher and key generation
├── CMakeLists.txt                     # build (core library, executable, ctest, sanitizer option)
├── tests/
│   ├── run_tests.py                   # scenario runner
│   └── scenarios/                     # *.txt commands + *.expect expected output
├── .github/workflows/ci.yml           # Linux build, tests, sanitizer job
└── README.md
```

## Limitations

The project is educational and the scope was cut on purpose to finish it.

- **Not secure** Users have no passwords; anyone can `login` as anyone. User keys are stored in the image in plain text, and the XOR cipher is not cryptographically strong. Access control protects against mistakes inside the shell, not against someone who reads the image file directly.
- **No crash safety** A failure inside an operation is rolled back, but operations are not atomic: a crash or power loss in the middle of a write can leave the image inconsistent. There is no `fsck` and no journal.
- **Single process** Two programs working on the same image at the same time overwrite each other's changes.
- **Simple data model** `write` replaces the whole content; there is no append, partial write or truncate.
- **Terminal** No Tab completion and no command aliases.

## Use of AI assistance

I made the design decisions and wrote the core of the code myself: the allocator algorithm, the data structures and the on-disk layout, the shell commands, navigation and path resolution, copy and move, import and export, the rollback logic, and the fixes for most bugs. I make all commits myself.

I used Claude (Anthropic) throughout as a tutor and reviewer: it explained C++ and file system concepts, reviewed my code and pointed out bugs, and proposed design options for me to choose from. Claude also wrote some parts directly: the test runner, the CMake build and the CI workflow, moving the allocator into its own source files and the mechanical split of the code into headers and sources, `free` calls after table reallocation, the recursive release of a directory tree, applying my rollback pattern to the other operations, and the text of the `help` command.