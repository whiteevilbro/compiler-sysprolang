#include "input.h"

#include "memory/managment.h"

#include <stdio.h>
#include <stdlib.h>

const char* read_file(const char* file, SizeVec* newlines) {
  FILE* input = fopen(file, "rb");
  if (!input)
    exit(-3);
  fseek(input, 0, SEEK_END);
  size_t size = ftell(input);
  fseek(input, 0, SEEK_SET);

  char* buffer = smalloc(size + 1);
  fread(buffer, 1, size, input);
  fclose(input);
  buffer[size] = '\0';

  size_t distance = 0;
  vecPush(newlines, distance);

  char* p = buffer;
  while (*p != '\0') {
    if (*p == '\n') {
      size_t distance = (size_t) (p - buffer) + 1;
      vecPush(newlines, distance);
    }
    p++;
  }
  distance = (size_t) (p - buffer) + 1;
  vecPush(newlines, distance);
  return buffer;
}
