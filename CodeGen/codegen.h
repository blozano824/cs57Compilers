#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include<vector>
#include <map>
#include <utility>

using namespace std;

int localMem = 0;

// Local maps
map<LLVMValueRef,int> inst_index;
map<LLVMValueRef,pair<int, int>> live_range;

// Global maps to be used in codegeneration
map<LLVMBasicBlockRef, char*> bb_labels;
map<LLVMValueRef, int> reg_map;
map<LLVMValueRef, int> offset_map;

LLVMModuleRef createLLVMModel(char *);
void printDirectives(LLVMValueRef);
void printFunctionEnd();
void createBBLabels(LLVMValueRef);
void create_inst_index(LLVMBasicBlockRef);
void compute_liveness(LLVMBasicBlockRef);
void getOffsetMap(LLVMValueRef);
void clear_data();
#endif
