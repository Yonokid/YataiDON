// Native title mounts support stat/open but not all Unix symlink/ownership calls.
#undef HAVE_LSTAT
#undef HAVE_READLINK
#undef HAVE_FCHOWN
