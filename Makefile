LLVMCODE = optimizer
TEST = test

$(LLVMCODE): $(LLVMCODE).c
	c++ `llvm-config --cflags` -c $(LLVMCODE).c
	c++ `llvm-config --cxxflags --ldflags --libs core` $(LLVMCODE).o -o $@

test: $(TEST).c
	clang -S -emit-llvm $(TEST).c -o $(TEST).ll

testoptimizer: 
	./$(LLVMCODE) $(TEST).ll 2> optimized.ll

clean: 
	rm -rf $(TEST).ll
	rm -rf optimized.ll
	rm -rf $(LLVMCODE)
	rm -rf *.o
