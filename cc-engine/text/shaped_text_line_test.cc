#include "text/shaped_text_line.h"

#include <cassert>

int main() {
  assert(text::NormalizeSingleLineText("  Love\nis  \r\n sudden\tturn  ") ==
         "Love is sudden turn");
  assert(text::NormalizeSingleLineText("Public Sans") == "Public Sans");
  assert(text::NormalizeSingleLineText(" \n\t ").empty());
  assert(text::NormalizeSingleLineText("café 曲線") == "café 曲線");
  return 0;
}
