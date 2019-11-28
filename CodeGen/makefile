LLVMCODE = codegen
TEST = test
RESULT = result

$(LLVMCODE): $(LLVMCODE).c $(LLVMCODE).h
	c++ -I `llvm-config --cflags` -c $(LLVMCODE).c
	c++ `llvm-config --cxxflags --ldflags --libs core` $(LLVMCODE).o -o $@

test: $(TEST).c
	clang -S -emit-llvm $(TEST).c -o $(TEST).ll

asm: $(TEST).c
	gcc -S -m32 -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables $(TEST).c -o $(TEST).s

myasm:$(TEST).c
	clang -S -emit-llvm $(TEST).c -o $(TEST).ll
	./$(LLVMCODE) $(TEST).ll > my$(TEST).s

clean: 
	rm -rf $(TEST).ll
	rm -rf $(LLVMCODE)
	rm -rf *.o
	rm -rf *.s
	rm -rf a.out
