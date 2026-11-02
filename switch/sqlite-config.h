// FAT on libnx has no Unix ownership, symlinks or shared-memory mappings.
#undef HAVE_LSTAT
#undef HAVE_READLINK
#undef HAVE_FCHOWN
#define HAVE_FCHMOD 0
#define SQLITE_OMIT_WAL 1
#define SQLITE_MAX_MMAP_SIZE 0
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_DISABLE_DIRSYNC 1
#define SQLITE_DEFAULT_UNIX_VFS "unix-dotfile"
