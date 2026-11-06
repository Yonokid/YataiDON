#pragma once

#include <string>
#include <string_view>

// Text read from charts and box.def files is UTF-8 (with or without a BOM) or, in files made for
// older simulators, Shift-JIS without a BOM.

bool is_valid_utf8(std::string_view text);

// `text` as UTF-8: returned as it is when it already is valid UTF-8, otherwise read as Shift-JIS
// (code page 932). Where no converter is available the bytes are returned unchanged.
std::string utf8_from_utf8_or_sjis(std::string_view text);
