# Clear only generated asset trees so removed source assets do not ship in
# incremental builds. The app's writable Documents directory is unrelated.
file(REMOVE_RECURSE "${DEST_DIR}/Skins" "${DEST_DIR}/Songs" "${DEST_DIR}/shader")
file(MAKE_DIRECTORY "${DEST_DIR}")
file(COPY "${SOURCE_DIR}/shader" DESTINATION "${DEST_DIR}")
file(COPY "${SKINS_DIR}/" DESTINATION "${DEST_DIR}/Skins"
  PATTERN ".git" EXCLUDE PATTERN ".git*" EXCLUDE)
file(COPY "${SONGS_DIR}/" DESTINATION "${DEST_DIR}/Songs" PATTERN ".git*" EXCLUDE)
file(READ "${SOURCE_DIR}/config.toml" _config)
string(REPLACE "touch_input = false" "touch_input = true" _config "${_config}")
string(REPLACE "vsync = false" "vsync = true" _config "${_config}")
file(WRITE "${DEST_DIR}/config.toml" "${_config}")
