#ifndef INPUT_H
#define INPUT_H

#include "vector/vector.h"
VecDef(size_t) SizeVec;

const char* read_file(const char* file, SizeVec* newlines);

#endif
