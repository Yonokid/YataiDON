# Portable replacement for `sed -i "112a #include <ctype.h> ..." src/rcore.c || true`.
# Inserts the include after line 112 of src/rcore.c, exactly as the sed `112a`
# command did, but with plain CMake so it works where PATCH_COMMAND runs under
# cmd.exe (no sed, no `true`).
#
# Guarded on the include already being present, so re-running is a no-op --
# and if a future raylib master already includes <ctype.h> itself, this
# correctly does nothing at all.
set(_line "#include <ctype.h>                 // Required for: isalpha() [Used in IsPathFile()]")
file(READ src/rcore.c _content)
string(FIND "${_content}" "#include <ctype.h>" _already)
if(NOT _already EQUAL -1)
    return()
endif()
# Find the offset just past the 112th newline.
set(_offset 0)
foreach(_i RANGE 1 112)
    string(SUBSTRING "${_content}" ${_offset} -1 _rest)
    string(FIND "${_rest}" "\n" _nl)
    if(_nl EQUAL -1)
        message(WARNING "src/rcore.c has fewer than 112 lines; ctype.h patch skipped")
        return()
    endif()
    math(EXPR _offset "${_offset} + ${_nl} + 1")
endforeach()
string(SUBSTRING "${_content}" 0 ${_offset} _head)
string(SUBSTRING "${_content}" ${_offset} -1 _tail)
file(WRITE src/rcore.c "${_head}${_line}\n${_tail}")
