name=minic
$(name): $(name).l $(name).y
	yacc -d $(name).y
	lex $(name).l
	g++ -o $(@) lex.yy.c y.tab.c ast.c

test1:
	./$(name) < testminic1.c

test2:
	./$(name) < testminic2.c

clean:
	rm lex.yy.c y.tab.c y.tab.h $(name)
