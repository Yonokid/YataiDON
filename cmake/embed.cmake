file(READ ${IN} hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," bytes "${hex}")
file(WRITE ${OUT} "#include <cstddef>\n"
  "extern const unsigned char ${NAME}[] = {${bytes}};\n"
  "extern const size_t ${NAME}_size = sizeof(${NAME});\n")
