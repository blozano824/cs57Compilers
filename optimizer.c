#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/BitWriter.h>
#include <vector> 
#include <set> 
#include <map> 
#include <string> 
#include <algorithm>
#include <iterator>
  
using namespace std;

#define prt(x) if(x) { printf("%s\n", x); }

void printSet(set<LLVMValueRef>* S);

map<LLVMBasicBlockRef, string> buildBlockRefToStringMap(LLVMValueRef function);
map<string, vector<string>*> buildPredecesorMap(LLVMValueRef function, map<LLVMBasicBlockRef, string> br2s_map);
int commonSubexpressionElimination(LLVMValueRef function);
int constantFolding(LLVMValueRef function);
int deadCodeElimination(LLVMValueRef function);
int constantPropogation(LLVMValueRef function, map<string, set<LLVMValueRef>*> IN);
map<string, set<LLVMValueRef>*> computeGEN(LLVMValueRef function);
map<string, set<LLVMValueRef>*> computeKILL(LLVMValueRef function);
tuple<map<string, set<LLVMValueRef>*>, map<string, set<LLVMValueRef>*>> computeINandOUT(LLVMValueRef function, map<string, set<LLVMValueRef>*> GEN, map<string, set<LLVMValueRef>*> KILL, map<LLVMBasicBlockRef, string> br2s_map, map<string, vector<string>*> PMap);
set<LLVMValueRef> computeStoreInstructionSet(LLVMValueRef function);

LLVMModuleRef createLLVMModel(char * filename){
	char *err = 0;

	LLVMMemoryBufferRef ll_f = 0;
	LLVMModuleRef m = 0;

	LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);

	if (err != NULL) { 
		prt(err);
		return NULL;
	}
	
	LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);

	if (err != NULL) {
		prt(err);
	}

	return m;
}

void optimize(LLVMModuleRef module){
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
		const char* funcName = LLVMGetValueName(function);	
		printf("Function Name: %s\n", funcName);
	
		map<LLVMBasicBlockRef, string> br2s_map = buildBlockRefToStringMap(function);
		map<string, vector<string>*> PMap = buildPredecesorMap(function, br2s_map);
		
		int changeOccurred = 1;
		while (changeOccurred) {
			changeOccurred = 0;
			map<string, set<LLVMValueRef>*> GEN = computeGEN(function);
			map<string, set<LLVMValueRef>*> KILL = computeKILL(function);
			map<string, set<LLVMValueRef>*> IN;
			map<string, set<LLVMValueRef>*> OUT;
			tie(IN, OUT) = computeINandOUT(function, GEN, KILL, br2s_map, PMap);
			changeOccurred += constantPropogation(function, IN);
			changeOccurred += constantFolding(function);
			changeOccurred += deadCodeElimination(function);
			changeOccurred += commonSubexpressionElimination(function);
		}
 	}
}

map<LLVMBasicBlockRef, string> buildBlockRefToStringMap(LLVMValueRef function){
	int blockNum = 0;
	map< LLVMBasicBlockRef, string> br2s_map;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		br2s_map[basicBlock] = blockName;
		blockNum++;
	}
	return br2s_map;
}

int commonSubexpressionElimination(LLVMValueRef function) {
	printf("\nCOMMON SUBEXPRESSION ELIMINATION \n");
	int changeOccurred = 0;
	int blockNum = 0;
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
    	basicBlock; 
    	basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
    	// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// print block name
		printf("%s\n", blockName.c_str());
		// iterate through instructions
		for (LLVMValueRef instruction1 = LLVMGetFirstInstruction(basicBlock); 
			instruction1;
   			instruction1 = LLVMGetNextInstruction(instruction1)) {
   			for (LLVMValueRef instruction2 = instruction1; 
				instruction2;
   				instruction2 = LLVMGetNextInstruction(instruction2)) {
   				if (instruction1 != instruction2){
					LLVMOpcode opCode1 = LLVMGetInstructionOpcode(instruction1);
					LLVMOpcode opCode2 = LLVMGetInstructionOpcode(instruction2);
					if (opCode1 == opCode2){
						if (opCode1 == LLVMLoad){
							LLVMValueRef operand1 = LLVMGetOperand(instruction1, 0);
							LLVMValueRef operand2 = LLVMGetOperand(instruction2, 0);
							if (operand1 == operand2){
								printf("same load found\n");
								// print instruction
								printf(LLVMPrintValueToString(instruction1));
								printf("  <-->");
								printf(LLVMPrintValueToString(instruction2));
								printf("\n");
								LLVMValueRef curr = instruction1;
								LLVMValueRef end = instruction2;
								int storeFoundBetweenLoads = 0;
								while (curr != end){
 									if (LLVMStore == LLVMGetInstructionOpcode(curr)){
 										LLVMValueRef storeOperand = LLVMGetOperand(curr, 1);
 										if (operand1 == storeOperand){
 											storeFoundBetweenLoads = 1;
 										}
 									}
									curr = LLVMGetNextInstruction(curr);
								}
								if (!storeFoundBetweenLoads) {
									for (LLVMUseRef u = LLVMGetFirstUse(instruction2);
										u;
										u = LLVMGetNextUse(u)){
										LLVMValueRef user = LLVMGetUser(u);
										printf(LLVMPrintValueToString(user));
										printf("\n");
										if (LLVMGetOperand(user, 0) == instruction2){
											printf("first operand in user is redundant\n");
											LLVMSetOperand (user, 0, instruction1);
											changeOccurred = 1;
										}
										if (LLVMGetOperand(user, 1) == instruction2){
											printf("second operand in user is redundant\n");
											LLVMSetOperand (user, 1, instruction1);
											changeOccurred = 1;
										}
									}
								}
							}
						}
					}
   				}
   			}
   		}
		blockNum++;
	}
	return changeOccurred;	
}

int constantFolding(LLVMValueRef function){
	printf("\nCONSTANT FOLDING \n");
	int changeOccurred = 0;
	int blockNum = 0;
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
    	basicBlock; 
    	basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
    	// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// print block name
		printf("%s\n", blockName.c_str());
		
		// iterate through instructions
		LLVMValueRef lastInstruction = 0;
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;) {
			LLVMValueRef replacementValue = 0;

			if (LLVMIsABinaryOperator(instruction)) {
				printf("Binary operator found! \n");
				// print instruction
				printf(LLVMPrintValueToString(instruction));
				printf("\n");
				LLVMValueRef x = LLVMGetOperand(instruction, 0);
				LLVMValueRef y = LLVMGetOperand(instruction, 1);
			
				const int allConstant = LLVMIsAConstant(x) && LLVMIsAConstant(y);
	
				if (allConstant) {
					printf("all constants! \n");
					switch (LLVMGetInstructionOpcode(instruction)) {
						default:
							break;
						case LLVMAdd:
							replacementValue = LLVMConstAdd(x, y);
							break;
						case LLVMSub:
							replacementValue = LLVMConstSub(x, y);
							break;
						case LLVMMul:
							replacementValue = LLVMConstMul(x, y);
							break;
						case LLVMSDiv:
							replacementValue = LLVMConstSDiv(x, y);
							break;
					}
					printf("replacement value is %d! \n", replacementValue);
				}
			}
			
			// if a replacement value exists
			if (replacementValue) {
				changeOccurred = 1;
				// replace all uses of instruction with replacement value
				LLVMReplaceAllUsesWith(instruction, replacementValue);
				// erase instruction
				LLVMInstructionEraseFromParent(instruction);
				// if no previous instruction, get first instruction in block
				if (!lastInstruction) {
					instruction = LLVMGetFirstInstruction(basicBlock);
				} 
				// get next instruction in block
				else {
					instruction = LLVMGetNextInstruction(lastInstruction);
				}
			}
			else {
				// mark current instruction as last instruction
				lastInstruction = instruction;
				// set new current instruction as next instruction
				instruction = LLVMGetNextInstruction(instruction);
			}
		}
		blockNum++;
	}
	return changeOccurred;
}

int deadCodeElimination(LLVMValueRef function){
	printf("\nDEAD CODE ELIMINATION \n");
	int changeOccurred = 0;
	int blockNum = 0;
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
    	basicBlock; 
    	basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
    	// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// print block name
		printf("%s\n", blockName.c_str());
		
		// iterate through instructions
		LLVMValueRef lastInstruction = 0;
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;) {
   			int deadCode = 0;
   			// if load instruction
    		if (LLVMIsALoadInst(instruction)) {
    			// if load instruction has no uses
				if (LLVMGetFirstUse(instruction) == NULL){
					deadCode = 1;
				}
   			}
   			
   			// if instruction is dead code
			if (deadCode) {
				changeOccurred = 1;
				// erase instruction
				LLVMInstructionEraseFromParent(instruction);
				// if no previous instruction, get first instruction in block
				if (!lastInstruction) {
					instruction = LLVMGetFirstInstruction(basicBlock);
				} 
				// get next instruction in block
				else {
					instruction = LLVMGetNextInstruction(lastInstruction);
				}
			}
			else {
				// mark current instruction as last instruction
				lastInstruction = instruction;
				// set new current instruction as next instruction
				instruction = LLVMGetNextInstruction(instruction);
			}
   		}
		blockNum++;
	}
	return changeOccurred;
}

int constantPropogation(LLVMValueRef function, map<string, set<LLVMValueRef>*> IN){
	printf("\nCONSTANT PROPOGATION \n");
	int changeOccurred = 0;
	int blockNum = 0;
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// R = IN[B]
		set<LLVMValueRef>* R = IN[blockName];
		
		// iterate through instructions
		LLVMValueRef lastInstruction = 0;
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;) {
			LLVMValueRef replacementValue = 0;
			// If instruction is a store instruction add instruction to R
			if (LLVMIsAStoreInst(instruction)) { R->insert(instruction);}
			
			// If instruction is a store instruction then remove all store 
			// instructions in R that are killed by the instruction
			if (LLVMIsAStoreInst(instruction)) {
				// get possible killer instruction second operand
				LLVMValueRef x = LLVMGetOperand(instruction, 1);
				set<LLVMValueRef>::iterator it=R->begin();
				while (it != R->end()){
					LLVMValueRef store_instruction = *it;
					// get possible victim instruction second operand
					LLVMValueRef y = LLVMGetOperand(store_instruction, 1);
					// same operand found between instructions
					if ((x == y) && (instruction != store_instruction)){
						it = R->erase(it);
					}
					else {
						++it;
					}
				}
			}

			// If instruction is a load instruction
			if (LLVMIsALoadInst(instruction)){
				// that loads from address represented by variable %t
				LLVMValueRef load_operand = LLVMGetOperand(instruction, 0);
				// find all the store instructions in R that write to address represented by %t.
				vector<LLVMValueRef> store_instructions_to_same_address;
				set<LLVMValueRef>::iterator it = R->begin();
				while (it != R->end()){
					LLVMValueRef store_instruction = *it;
					LLVMValueRef store_operand = LLVMGetOperand(store_instruction, 1);
					if (load_operand == store_operand) {
						store_instructions_to_same_address.push_back(store_instruction);
					} 
					++it;
				}
				
				// If all these store instructions are constant store instructions 
				// and write the same constant value into memory
				int allConstants = 0;
				int sameConstant = 0;
				int foundConstant = 0;
				int firstConstantFound = 0;
				vector<LLVMValueRef>::iterator it2 = store_instructions_to_same_address.begin();
				while (it2 != store_instructions_to_same_address.end()){
					LLVMValueRef store_instruction = *it2;
					LLVMValueRef stored_value = LLVMGetOperand(store_instruction, 0);
					if (LLVMIsAConstant(stored_value)){
						allConstants++;
						if (foundConstant == 0){
							char* operand_str = LLVMPrintValueToString(stored_value);
							operand_str += 3;
							firstConstantFound = atoi(operand_str);
							replacementValue = stored_value;
							sameConstant++;
							foundConstant = 1;
						}
						else {
							if (firstConstantFound == atoi(LLVMPrintValueToString(stored_value))) {
								sameConstant++;
							}
						}
					}
					++it2;
				}
				// if a replacement value exists
				if (replacementValue && allConstants && sameConstant && foundConstant) {
					changeOccurred = 1;
					// replace all uses of instruction with replacement value
					LLVMReplaceAllUsesWith(instruction, replacementValue);
					// erase instruction
					LLVMInstructionEraseFromParent(instruction);
					// if no previous instruction, get first instruction in block
					if (!lastInstruction) {
						instruction = LLVMGetFirstInstruction(basicBlock);
					} 
					// get next instruction in block
					else {
						instruction = LLVMGetNextInstruction(lastInstruction);
					}
				}
				else {
					// mark current instruction as last instruction
					lastInstruction = instruction;
					// set new current instruction as next instruction
					instruction = LLVMGetNextInstruction(instruction);
				}
			}
			else {
				// mark current instruction as last instruction
				lastInstruction = instruction;
				// set new current instruction as next instruction
				instruction = LLVMGetNextInstruction(instruction);
			}
		}
		blockNum++;
	}
	return changeOccurred;
}

map<string, set<LLVMValueRef>*> computeGEN(LLVMValueRef function){
	printf("\nCOMPUTE GEN \n");
	int blockNum = 0;
	map<string, set<LLVMValueRef>*> GEN;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// print block name
		printf("%s\n", blockName.c_str());
		// initialize GEN[B] to empty set
		GEN[blockName] = new set<LLVMValueRef>; 
		// iterate through instructions
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;
   			instruction = LLVMGetNextInstruction(instruction)) {
			// if instruction is store type
			if (LLVMIsAStoreInst(instruction)) {
				// add instruction to the set GEN[B]
				GEN[blockName]->insert(instruction);
   			}
   			// Check if there are any instructions in set GEN[B] 
   			// that are killed by instruction "I". If so remove 
   			// them from set GEN[B]. [Note: A store instruction 
   			// can be killed in two ways: (a) Another store 
   			// instruction to same memory location (b) by an 
   			// assignment to the temporary variable that holds 
   			// the memory location address. Due to SSA this will 
   			// not happen in LLVM and hence we ignore it.]
   			
   			// get possible killer instruction second operand
			LLVMValueRef x = LLVMGetOperand(instruction, 1);
			set<LLVMValueRef>::iterator it=GEN[blockName]->begin();
   			while(it != GEN[blockName]->end()){
				LLVMValueRef store_instruction = *it;
				// get possible victim instruction second operand
				LLVMValueRef y = LLVMGetOperand(store_instruction, 1);
				// same operand found between instructions
				if ((x == y) && (instruction != store_instruction)){
					it = GEN[blockName]->erase(it);
				}
				else {
					++it;
				}
			}
   			
   		}	
   		printf("  GENSET\n");
   		printSet(GEN[blockName]);	
		blockNum++;
	}
	return GEN;
}

map<string, set<LLVMValueRef>*> computeKILL(LLVMValueRef function){
	printf("\nCOMPUTE KILL \n");
	int blockNum = 0;
	// gather all stores
	set<LLVMValueRef> S = computeStoreInstructionSet(function);
	map<string, set<LLVMValueRef>*> KILL;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// print block name
		printf("%s\n", blockName.c_str());
		// initialize KILL[B] to empty set
		KILL[blockName] = new set<LLVMValueRef>; 
		// iterate through instructions
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;
   			instruction = LLVMGetNextInstruction(instruction)) {
   			// if instruction is store type
			if (LLVMIsAStoreInst(instruction)) {				
				// get killer instruction second operand
				LLVMValueRef x = LLVMGetOperand(instruction, 1);
				for (set<LLVMValueRef>::iterator it=S.begin(); it!=S.end(); ++it){
					LLVMValueRef store_instruction = *it;
					// get possible victim instruction second operand
					LLVMValueRef y = LLVMGetOperand(store_instruction, 1);
					// same operand found between instructions
					if ((x == y) && (instruction != store_instruction)){
						// add instruction to KILL[B]
						KILL[blockName]->insert(store_instruction);
					}
				}
   			}
   		}
   		printf("  KILLSET\n");
   		printSet(KILL[blockName]);
   		blockNum++;
	}
	return KILL;
}

tuple<map<string, set<LLVMValueRef>*>, map<string, set<LLVMValueRef>*>> computeINandOUT(LLVMValueRef function, map<string, set<LLVMValueRef>*> GEN, map<string, set<LLVMValueRef>*> KILL, map<LLVMBasicBlockRef, string> br2s_map, map<string, vector<string>*> PMap){
	printf("\nCOMPUTE IN & OUT\n");
	map<string, set<LLVMValueRef>*> IN;
	map<string, set<LLVMValueRef>*> OUT;
	int blockNum = 0;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// initialize IN[B] to empty set
		IN[blockName] = new set<LLVMValueRef>;
		// for each basic block B set OUT[B] = GEN[B]
		OUT[blockName] = GEN[blockName];
		blockNum++;
	}
	int change = 1;
	while (change) {
		change = 0;
		blockNum = 0;
		for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
			basicBlock; 
			basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
			// compute block name B
			string blockName = "BLOCK" + to_string(blockNum);
			
			// build empty set for IN[B]
			IN[blockName] = new set<LLVMValueRef>;
			// get predecessors of current block
			vector<string>* predecessors = PMap[blockName];
			// iterate through predecessors
			for (vector<string>::iterator it=predecessors->begin(); it!=predecessors->end(); ++it){
				string predBlockName = *it;
				// IN[B] = union(OUT[P1], OUT[P2], OUT[P3]...)
				IN[blockName]->insert(OUT[predBlockName]->cbegin(), OUT[predBlockName]->cend());
			}
			
			// old_out = OUT[B]
			set<LLVMValueRef>* old_out = OUT[blockName];
			set<LLVMValueRef> result1;
			// (in[B] - kill[B])
			set_difference(IN[blockName]->begin(), IN[blockName]->end(), 
				KILL[blockName]->begin(), KILL[blockName]->end(),
    			inserter(result1, result1.end()));
    		// GEN[B] union (in[B] - kill[B])
    		set<LLVMValueRef>* result2 = new set<LLVMValueRef>;
    		result2->insert(GEN[blockName]->cbegin(), GEN[blockName]->cend());
    		result2->insert(result1.cbegin(), result1.cend());
    		// OUT[B] = GEN[B] union (in[B] - kill[B])
    		OUT[blockName] = result2;
			
			set<LLVMValueRef> check;
			set_difference(OUT[blockName]->begin(), OUT[blockName]->end(), 
				old_out->begin(), old_out->end(),
    			inserter(check, check.end()));
    		// if (OUT[B] != old_out) change = True
    		if (!check.empty()){
    			change = 1;
    		}
    		blockNum++;
		}
	}
	
	blockNum = 0;
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
			basicBlock; 
			basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
			// compute block name B
			string blockName = "BLOCK" + to_string(blockNum);
			// print block name
			printf("%s\n", blockName.c_str());
			printf("  INSET\n");
   			printSet(IN[blockName]);
   			printf("  OUTSET\n");
   			printSet(OUT[blockName]);
   			blockNum++;
	}
	return make_tuple(IN, OUT);
}

map<string, vector<string>*> buildPredecesorMap(LLVMValueRef function, map<LLVMBasicBlockRef, string> br2s_map){
	map<string, vector<string>*> PMap;
	int blockNum = 0;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		// initialize Predecessors[B] to empty vector
		PMap[blockName] = new vector<string>;
		blockNum++;
	}
	blockNum = 0;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// compute block name B
		string blockName = "BLOCK" + to_string(blockNum);
		LLVMValueRef terminator = LLVMGetBasicBlockTerminator(basicBlock);
		int sNum = LLVMGetNumSuccessors(terminator);
		for (int i = 0; i < sNum; i++) {
			LLVMBasicBlockRef successorBlock = LLVMGetSuccessor(terminator, i);
 			// add current block name as predecessor to all successors blocks
 			PMap[br2s_map[successorBlock].c_str()]->push_back(blockName);
		}
		blockNum++;
	}
	return PMap;
}

void printSet(set<LLVMValueRef>* S){
	for (set<LLVMValueRef>::iterator it=S->begin(); it!=S->end(); ++it){
		LLVMValueRef instruction = *it;
		printf("%s\n", LLVMPrintValueToString(instruction));
	}
}

set<LLVMValueRef> computeStoreInstructionSet(LLVMValueRef function){
	set<LLVMValueRef> S;
	// iterate through blocks
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); 
		basicBlock; 
		basicBlock = LLVMGetNextBasicBlock(basicBlock)) {	
		// iterate through instructions
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
			instruction;
   			instruction = LLVMGetNextInstruction(instruction)) {
   			// if instruction is store type
			if (LLVMIsAStoreInst(instruction)) {
				// add instruction to the set
				S.insert(instruction);
   			}
   		}
	}
	return S;
}

int main(int argc, char** argv)
{
	LLVMModuleRef m;

	if (argc == 2){
		m = createLLVMModel(argv[1]);
	}
	else{
		m = NULL;
		return 1;
	}

	if (m != NULL){
 		optimize(m);
		LLVMDumpModule(m);
	}
	else {
	    printf("m is NULL\n");
	}
	
	return 0;
}