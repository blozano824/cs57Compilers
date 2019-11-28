#include"codegen.h"
#include <algorithm>

int num_digits(int n){ 
    int j = 0;
    while (n >= 1){ 
        n = n / 10; 
        j = j + 1;
    }   
    return j;
}

LLVMModuleRef createLLVMModel(char * filename){
	char *err = 0;

	LLVMMemoryBufferRef ll_f = 0;
	LLVMModuleRef m = 0;

	LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);

	if (err != NULL) { 
		return NULL;
	}
	
	LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);
	return m;
}

void printDirectives(LLVMValueRef func){
	const char* funcName = LLVMGetValueName(func);	
	printf("\t.globl %s\n", funcName);
	printf("\t.type %s, @function\n", funcName);
	printf("%s:\n",funcName);
	printf("\tpushl %%ebp\n");
	printf("\tmovl %%esp, %%ebp\n");
	
}

void printFunctionEnd(){
	printf("\tmovl %%ebp, %%esp\n");
	printf("\tpopl %%ebp\n");
	printf("\tret\n");
}

const char* get_register(int num){
	if (num == 0){
		return "%eax";
	}
	if (num == 1){
		return "%ebx";
	}
	if (num == 2){
		return "%ecx";
	}
	if (num == 3){
		return "%edx";
	}
	return "wrong";
}

// Go over all the basic blocks and create the labels
void createBBLabels(LLVMValueRef function){
	int i = 0;
	const char * base = "BB";

	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

			char * s = (char *) calloc(num_digits(i) + 3, sizeof(char));
			sprintf(s, "%d", i);
			char * label = (char *) calloc(strlen(base)+strlen(s)+1, sizeof(char));
			strncpy(label, base, 2);
			strcat(label, s);
			
			bb_labels.insert(pair<LLVMBasicBlockRef, char *> (basicBlock, label));
			free(s);
			i = i + 1;
			
	}
	
}

bool compare_first(const pair<int, int>&i, const pair<int, int>&j){
    return i.first < j.first;
}

bool compare_second(const pair<int, int>&i, const pair<int, int>&j){
    return i.second < j.second;
}

// Implementation of linear scan register allocation algorithm from the paper 
// Linear Scan Register Allocation by Massimiliano Poletto & Vivek Sarkar
map<pair<int, int>, int>* linear_scan_register_allocation(LLVMBasicBlockRef bb, map <LLVMValueRef, pair<int, int>> live_range){
// 	printf("linear scan register allocation algorithm\n");
	int R = 3;
	// initialize array of free registers
	vector<int>* free_registers = new vector<int>;
	// add in ebp register
	free_registers->push_back(1);
	// add in ecp register
	free_registers->push_back(2);
	// add in edp register
	free_registers->push_back(3);
	// initialize map of intervals to registers
	map<pair<int, int>, int>* registers = new map<pair<int, int>, int>;
	// initialize array of active intervals
	vector<pair<int, int>>* active = new vector<pair<int, int>>;
	// initialize array of all intervals
	vector<pair<int, int>>* all_intervals = new vector<pair<int, int>>;
	// add all intervals to array
	for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); 
		instruction;
        instruction = LLVMGetNextInstruction(instruction)) {
		if (LLVMIsAAllocaInst(instruction)) continue;
		pair<int, int> range = live_range[instruction];
		all_intervals->push_back(range);
	}
	// all intervals sorted by start point
	sort(all_intervals->begin(), all_intervals->end(), compare_first);
	// initialize i iterator
	vector<pair<int, int>>::iterator i = all_intervals->begin();
	// for each live interval i, in order of increasing start point
	while (i != all_intervals->end()){
		pair<int, int> interval = *i;
// 		printf("%d\t%d", interval.first, interval.second);
		// ExpireOldIntervals(i)
		// active sorted by increasing end point
		sort(active->begin(), active->end(), compare_second);
		// initialize j iterator
		vector<pair<int, int>>::iterator j = active->begin();
		while (j != active->end()){
			// if endpoint[j] ≥ startpoint[i]
			if (j->second >= i->first){
				// return
				break;
			}
			// add register to pool of free registers
			free_registers->push_back((*registers)[*j]);
// 			printf(" -> erasing");
			// remove j from active
			j = active->erase(j);
		}
		// if length(active) = R then
		if (active->size() == R){
// 			printf(" -> spillage!\n");
			// SpillAtInterval(i)
			pair<int, int> spill = active->back();
			if (spill.second > i->second){
				(*registers)[interval] = (*registers)[spill];
				(*registers)[spill] = -1;
				// remove spill from active
				active->pop_back();
				// add i to active
				active->push_back(interval);
				// active sorted by increasing end point
				sort(active->begin(), active->end(), compare_second);
			}
		}
		else{
		    // assign free register to interval
			(*registers)[interval] = free_registers->back();
			// remove register from free registers
			free_registers->pop_back();
			// add i to active
			active->push_back(interval);
			// active sorted by increasing end point
			sort(active->begin(), active->end(), compare_second);
// 			printf("\n");
		}
		++i;
	}
	map<pair<int, int>, int>::iterator reg = registers->begin();
	// for each live interval i, in order of increasing start point
	while (reg != registers->end()){
// 		printf("[%d,%d] -> %d\n", reg->first.first, reg->first.second, reg->second);
		++reg;
	}
	return registers;
}

//Populates the map from LLVMValueref (instruction) to index
void create_inst_index(LLVMBasicBlockRef bb){
		int n = 0;
        for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction;
                                instruction = LLVMGetNextInstruction(instruction)) {
			if (!LLVMIsAAllocaInst(instruction)){
				inst_index.insert(pair<LLVMValueRef, int> (instruction, n));
				n = n + 1;
			}
		}	
}

// Computes the liveness range of each instruction. start is the index of the def and end
// is the index of last use in this basic block.
void compute_liveness(LLVMBasicBlockRef bb){
		
		// Compute the index of all the instructions
		create_inst_index(bb);

        for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction;
                                instruction = LLVMGetNextInstruction(instruction)) {
			
			// Ignore the alloc instructions
			if (LLVMIsAAllocaInst(instruction)) continue;

			int start, end;
			
			start = inst_index[instruction];
			end = start;
			
			// Compute the last use of the instruction in this basic block
		 
			for (LLVMUseRef u = LLVMGetFirstUse(instruction); u;
					u = LLVMGetNextUse(u)){
				LLVMValueRef user = LLVMGetUser(u);
				
				// Check if use is in the same basic block (this should be the case)
				if (LLVMGetInstructionParent(instruction) == LLVMGetInstructionParent(user)){
					int index = inst_index[user];	
					if (index > end)
							end = index;
				}
				
			}
			
			pair<int, int> range (start, end);
			live_range.insert(pair<LLVMValueRef, pair<int, int>> (instruction, range));
		}
		// UNCOMMENT FOR PRINTING
//         for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction;
//                                 instruction = LLVMGetNextInstruction(instruction)) {
// 
// 			if (LLVMIsAAllocaInst(instruction)) continue;
// 
// 			pair<int, int> range = live_range[instruction];
// 			LLVMDumpValue(instruction);
// 			printf("\t%d\t%d\n", range.first, range.second);
// 		}
}

void clear_data(){
	inst_index.clear();
	live_range.clear();
	return;
}

void getOffsetMap(LLVMValueRef func){
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(func);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		
		// Loop to add all the instructions into offset_map with an offset 0
        for (LLVMValueRef instr = LLVMGetFirstInstruction(basicBlock); instr;
                                instr = LLVMGetNextInstruction(instr)) {
			// Ignore if it is not generating a value	
			if (LLVMIsATerminatorInst(instr) || LLVMIsAStoreInst(instr))
				continue;
			
			// Ignore a call instruction if it doesn't have any use
			if (LLVMIsACallInst(instr))
				if (LLVMGetFirstUse(instr) == NULL)
					continue;
			
			
			offset_map.insert(pair<LLVMValueRef, int> (instr, 0));
		}
		
		//Loop over instructions to update offset for temporary variables
		//holding addresses of local variables and temporary variables
		//corresponding to local variables.
        for (LLVMValueRef instr = LLVMGetFirstInstruction(basicBlock); instr;
                                instr = LLVMGetNextInstruction(instr)) {
				if (LLVMIsAStoreInst(instr)){
					LLVMValueRef op1 = LLVMGetOperand(instr, 0);
					LLVMValueRef op2 = LLVMGetOperand(instr, 1);

					// If store is from a parameter to the local memory
					// set both the offsets to 8. In miniC we have 
					// maximum of 1 parameter.
					
					if (LLVMIsAArgument(op1)) {
						offset_map[op1] = 8;
						offset_map[op2] = 8;
					}	
					else {
						// If the offset of the address variable is not
						// set then set it localMem and update localMem
						if (offset_map[op2] == 0){
							localMem = localMem - 4;
							offset_map[op2] = localMem;
						}
						
						// Link temporary variable in op1 to local variable
						if (! LLVMIsAConstantInt(op1)){
							offset_map[op1] = offset_map[op2];
						}
					}
				}

				if (LLVMIsALoadInst(instr)){
					LLVMValueRef op1 = LLVMGetOperand(instr, 0);

					if (offset_map[op1] == 0){
						localMem = localMem - 4;
						offset_map[op1] = localMem;
					}
					
					//Link this instruction to the memory address of local
					//variable.
					offset_map[instr] = offset_map[op1];
				}
		}

		//Loop over all instructions(values) in the offset_map and find 
		//those with offset 0. These temporary variables do not 
		//correspond to any local variable in the miniC code. 
		map<LLVMValueRef, int>::iterator it;
		for (it = offset_map.begin(); it != offset_map.end(); it++){
			if (it->second == 0) {
				localMem = localMem - 4;
				it->second = localMem;
			}
		}
		
		// UNCOMMENT FOR PRINTING OFFSET MAP
// 		map<LLVMValueRef, int>::iterator it1;
// 		printf("\n Local Memory size %d\n", localMem);
// 		for (it1 = offset_map.begin(); it1 != offset_map.end(); it1++){
// 			LLVMDumpValue(it1->first);
// 			printf("\t %d\n", it1->second);
// 		}
// 		
	}
	
}

void walkBasicblocks(LLVMValueRef function){
	//Go over all the basic blocks and create the labels

	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		compute_liveness(basicBlock);
		clear_data();
	}
}

void walkFunctions(LLVMModuleRef module){
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
		function; 
		function = LLVMGetNextFunction(function)) {

		//const char* funcName = LLVMGetValueName(function);	

		
		if (!LLVMIsDeclaration(function)){
			createBBLabels(function);
// 			walkBasicblocks(function);
// 			getOffsetMap(function);
			//printFunctionEnd();
		}
 	}
 	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
		function; 
		function = LLVMGetNextFunction(function)) {
		if (!LLVMIsDeclaration(function)){
			// create basicBlock
			createBBLabels(function);
			// call printDirectives
			printDirectives(function);
			// call getOffsetMap to initialize offset_map and localMem
			getOffsetMap(function);
			// for each basicBlock in the function
			int blockNum = 0;
			for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 				basicBlock;
  				basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
  				if (blockNum > 0) {
  					printf(".%s\n", bb_labels[basicBlock]);
  				}
  				blockNum++;
  				// compute liveness in basicBlock
  				// populates a C++ STL map (inst_index)
  				// populates a C++ STL map (live_range)
  				compute_liveness(basicBlock);		
				map<pair<int, int>, int> RangetoRegister_map = *linear_scan_register_allocation(basicBlock, live_range);
  				// for each instruction in basicBlock:
				for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock); 
					instruction;
                    instruction = LLVMGetNextInstruction(instruction)) {
                    // if instruction is a return: (ret i32 A)
                    if (LLVMGetInstructionOpcode(instruction) == LLVMRet) {
                    	LLVMValueRef A = LLVMGetOperand(instruction, 0);
                    	// if A is a constant
                    	if (LLVMIsConstant(A)) {
                    		// convert instruction to string
                    		char* A_str = LLVMPrintValueToString(A);
                    		// move string starting pointer past characters
                    		A_str += 3;
                    		// emit movl $A, %eax
                    		printf("\tmovl $%d, %%eax\n", atoi(A_str));
                    	}
                    	else {
							// If A is a temporary variable and is in memory
							if (RangetoRegister_map[live_range[A]] == -1){
								// get offset k of A
								int k = offset_map[A];
								// emit movl k(%ebp), %eax 
								printf("\tmovl %d(%%epb), %%eax\n", k);
							}
							// If A is a temporary variable and 
							// has a physical register %exx assigned to it
							else {
								// emit movl %exx, %eax
								printf("\tmovl %s, %%eax\n", get_register(RangetoRegister_map[live_range[A]]));
							}
                    	}
                    	// call printFunctionEnd
                    	printFunctionEnd();
                    }
                    // if instruction is a load: (%a = load i32, i32* %b)
                    if (LLVMGetInstructionOpcode(instruction) == LLVMLoad) {
                    	LLVMValueRef A = instruction;
                    	// if %a has been assigned a physical register %exx:
                    	if (RangetoRegister_map[live_range[A]] != -1){
                    		LLVMValueRef B = LLVMGetOperand(A, 0);
                    		// get offset c for %b
							int c = offset_map[B];
							// emit movl c(%ebp),%exx
                    		printf("\tmovl %d(%%ebp), %s\n", c, get_register(RangetoRegister_map[live_range[A]]));
                    	}	
                    }
                    // if instruction is a store: (store i32 A, i32* %b)
                    if (LLVMGetInstructionOpcode(instruction) == LLVMStore) {
                    	LLVMValueRef A = LLVMGetOperand(instruction, 0);
                    	LLVMValueRef B = LLVMGetOperand(instruction, 1);
                    	// if A is a parameter
                    	if (LLVMIsAArgument(A)){
                    		// ignore
                    	}
                    	else{
							// if A is a constant
							if (LLVMIsConstant(A)) {
								// get offset c for %b
								int c = offset_map[B];
								// convert instruction to string
								char* A_str = LLVMPrintValueToString(A);
								// move string starting pointer past characters
								A_str += 3;
								// emit movl $A, c(%ebp)
								printf("\tmovl $%d, %d(%%ebp)\n", atoi(A_str), c);
							}
							// if A is a temporary variable %a
							else{
								// if %a has a physical register %exx assigned to it
								if (RangetoRegister_map[live_range[A]] != -1){
									// get offset c of %b
									int c = offset_map[B];
									// emit movl %exx, c(%ebp)
									printf("\tmovl %s, %d(%%ebp)\n", get_register(RangetoRegister_map[live_range[A]]), c);
								}
								// else
								else {
									// get offset c1 of %a
									int c1 = offset_map[A];
									// emit movl c1(%ebp), %eax
									printf("\tmovl %d(%%ebp), %%eax\n", c1);
									// get offset c2 of %b
									int c2 = offset_map[A];
									// emit movl %eax, c2(%ebp)
									printf("\tmovl %%eax, %d(%%ebp)\n", c2);
								}
							}
                    	}
                    }
                    // if instruction is a call: ( %a = call A @func(P)) or (call A @func(P))
                    if (LLVMGetInstructionOpcode(instruction) == LLVMCall) {
                    	LLVMValueRef A = instruction;
                    	LLVMValueRef func = LLVMGetOperand(instruction, 1);
						// emit pushl %ebx
						printf("\tpushl %%ebx\n");
						// emit pushl %ecx
						printf("\tpushl %%cbx\n");
						// emit pushl %edx
						printf("\tpushl %%edx\n");
						// if func has a param P
						if (LLVMCountParams(func) > 0){
							LLVMValueRef P = LLVMGetFirstParam(func);
							// if P is constant
							if (LLVMIsConstant(P)) {
								// FIX
								// convert instruction to string
								char* P_str = LLVMPrintValueToString(P);
								printf("Failed to convert parameter to string %s\n", P_str);
								// move string starting pointer past characters
								P_str += 3;
								// emit pushl $P
								printf("\tpushl $%d\n", atoi(P_str));
							}
							// if P is a temporary variable %p
							else {
								// if %p has a physical register %exx assigned 
								if (RangetoRegister_map[live_range[P]] != -1){
									// emit pushl %exx
									printf("\tpushl %s\n", get_register(RangetoRegister_map[live_range[P]]));
								}
								// if %p is in memory
								else {
									// get offset k of %p
									int k = offset_map[P];
									// emit pushl k(%ebp)
									printf("\tpushl %d(%%ebp)\n", k);
								}
							}
						}
						// emit call func
						printf("\tcall func %s\n", LLVMGetValueName(func));
						// if func has a param P
							// emit addl $4, %esp (this is to undo the pushing of parameter)
						// emit popl %edx
						printf("\tpopl %%edx\n");
						// emit popl %ecx
						printf("\tpopl %%ecx\n");
						// emit popl %ebx
						printf("\tpopl %%ebx\n");
						// if instruction is of the form (%a = call A @func(P))
						if (LLVMGetFirstUse(instruction)){
							// if %a has a physical register %exx assigned to it
							if (RangetoRegister_map[live_range[A]] != -1){
								// emit movl %eax, %exx
								printf("\tmovl %%eax, %s\n", get_register(RangetoRegister_map[live_range[A]]));
							}
							// if %a is in memory
							else{
								// get offset k of %a
								int k = offset_map[A];
								// emit movl %eax, k(%ebp)
								printf("\tmovl %%eax, %d(%%ebp)", k);
							}
						}
                    }
                    // if instruction is a branch (br i1 %a, label %b, label %c) or (br label %b)
                    if (LLVMGetInstructionOpcode(instruction) == LLVMBr) {
						// if the branch on unconditional (br label %b)
						if (!LLVMIsConditional(instruction)){
							// get label L of %b from bb_labels
							LLVMBasicBlockRef b = LLVMValueAsBasicBlock(LLVMGetOperand(instruction, 0));
							// emit jmp L
							printf("\tjmp .%s\n", bb_labels[b]);
						}
						// if the branch is conditional (br i1 %a, label %b, label %c)
						else {
							LLVMBasicBlockRef b = LLVMValueAsBasicBlock(LLVMGetOperand(instruction, 1));
							LLVMBasicBlockRef c = LLVMValueAsBasicBlock(LLVMGetOperand(instruction, 2));
							// get labels L1 for %b and L2 for %c from bb_labels
							// get the type T of comparison from instruction %a (<, >, <=, >=, ==)
							LLVMValueRef a = LLVMGetOperand(instruction, 0);
							LLVMIntPredicate T = LLVMGetICmpPredicate(a);
							// based on value of T:
							// emit jxx L1 [replace jxx with conditional jump 
							// assembly instruction corresponding to T]
							if (T == LLVMIntEQ){
								printf("\tje .%s\n", bb_labels[b]);
							}
							if (T == LLVMIntNE){
								printf("\tjne .%s\n", bb_labels[b]);
							}
							if (T == LLVMIntSGT || T == LLVMIntUGT){
								printf("\tjg .%s\n", bb_labels[b]);
							}
							if (T == LLVMIntSGE || T == LLVMIntUGE){
								printf("\tjge .%s\n", bb_labels[b]);
							}
							if (T == LLVMIntSLT || T == LLVMIntULT){
								printf("\tjl .%s\n", bb_labels[b]);
							}
							if (T == LLVMIntSLE || T == LLVMIntULE){
								printf("\tjle .%s\n", bb_labels[b]);
							}
							// emit jmp L2
							printf("\tjmp .%s\n", bb_labels[c]);
						}
                    }
                    // if instruction is an alloc
                    if (LLVMGetInstructionOpcode(instruction) == LLVMBr) {
                    }
                    // if instruction is an arithmetic operation: (%a = add nsw A, B) or (%a = icmp slt A, B)
                    if (LLVMGetInstructionOpcode(instruction) == LLVMAdd ||
                    	LLVMGetInstructionOpcode(instruction) == LLVMSub ||
                    	LLVMGetInstructionOpcode(instruction) == LLVMICmp) {
                    	int a_is_in_memory = 0;
                    	const char * X = "%%e?x";
                    	LLVMValueRef A = LLVMGetOperand(instruction, 0);
						LLVMValueRef B = LLVMGetOperand(instruction, 1);
						LLVMOpcode opCode = LLVMGetInstructionOpcode(instruction);
						// if %a has a physical register %exx assigned to it
						if (RangetoRegister_map[live_range[instruction]] != -1){
							// X = %exx 
							X = get_register(RangetoRegister_map[live_range[instruction]]);
						}
						// else:
						else {
							// X = %eax 
							X = "%%eax";
						}
						// if A is a constant
						if (LLVMIsConstant(A)) {
							// convert instruction to string
							char* A_str = LLVMPrintValueToString(A);
							// move string starting pointer past characters
							A_str += 3;
							// emit movl $A, X
							printf("\tmovl $%d, %s\n", atoi(A_str), X);
						}
						// if A is temporary variable
						else {
							// if A has physical register %eyy assigned to it:
							if (RangetoRegister_map[live_range[A]] != -1){
								// emit movl %eyy, X 
								printf("\tmovl %s, %s\n", get_register(RangetoRegister_map[live_range[A]]), X);
							}
							// if A is in memory:
							else {
								a_is_in_memory = 1;
								// get offset n of A
								int n = offset_map[A];
								// emit movl n(%ebp), X;
								printf("\tmovl %d(%%ebp), %s\n", n, X);
							}
						}
						// if B is a constant
						if (LLVMIsConstant(B)) {
							// emit addl $B, X [Note: you can replace addl by cmpl and subl based on opcode]
							// convert instruction to string
							char* B_str = LLVMPrintValueToString(B);
							// move string starting pointer past characters
							B_str += 3;
							if (opCode == LLVMAdd){
								printf("\taddl $%d, %s\n", atoi(B_str), X);
							}
							if (opCode == LLVMSub){
								printf("\tsubl $%d, %s\n", atoi(B_str), X);
							}
							if (opCode == LLVMICmp){
								printf("\tcmpl $%d, %s\n", atoi(B_str), X);
							}
							
						}
						// if B is a temporary variable
						else {
							// if B has physical register %ezz assigned to it:
							if (RangetoRegister_map[live_range[B]] != -1){
								// emit addl %ezz, X [Note: you can replace addl by cmpl and subl based on opcode]
								// convert instruction to string
								char* B_str = LLVMPrintValueToString(B);
								// move string starting pointer past characters
								B_str += 3;
								if (opCode == LLVMAdd){
									printf("\taddl %s, %s\n", get_register(RangetoRegister_map[live_range[B]]), X);
								}
								if (opCode == LLVMSub){
									printf("\tsubl %s, %s\n", get_register(RangetoRegister_map[live_range[B]]), X);
								}
								if (opCode == LLVMICmp){
									printf("\tcmpl %s, %s\n", get_register(RangetoRegister_map[live_range[B]]), X);
								}
							}
							// if B does not have a physical register assigned to it:
							else {
								// get offset m of B
								int m = offset_map[B];
								// emit addl m(%ebp), X
								if (opCode == LLVMAdd){
									printf("\taddl %d(%%ebp), %s\n", m, X);
								}
								if (opCode == LLVMSub){
									printf("\tsubl %d(%%ebp), %s\n", m, X);
								}
								if (opCode == LLVMICmp){
									printf("\tcmpl %d(%%ebp), %s\n", m, X);
								}
							}
						}
						// if %a is in memory
						if (a_is_in_memory){
							// get offset n of A
							int k = offset_map[A];
							// emit movl %eax, k(%ebp)
							printf("\tmovl %%eax, %d(%%ebp)\n", k);
						}
                    }
                }
                // clear compute liveness data for basicBlock
                clear_data();
			}
		}
 	}

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
		walkFunctions(m);
		//LLVMDumpModule(m);
	}
	else {
	    printf("m is NULL\n");
	}
	
	return 0;
}
