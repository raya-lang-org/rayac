#ifndef RAYA_CODEGEN_C_H
#define RAYA_CODEGEN_C_H

#include "ast.h"
#include <stdio.h>

void codegen_c_emit(AstNode *module, FILE *out);

#endif
