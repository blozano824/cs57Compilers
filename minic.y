/* definitions */
%{
	#include <stdio.h>
	#include <stdlib.h>
	#include <iostream> 
	#include <iterator> 
	#include <map> 
	#include <string>
	#include <algorithm>
	#include <set>
	#include "ast.h"
	
	#define YYSTYPE ast_Node*
	
	int yylex(void);
	astNode* root;
	extern char *currText;
	int findUndeclared(vector<string>, vector<string>);
	int findDuplicates(vector<string>);
	void traverseNode(astNode*);
	void traverseStmt(astStmt*);
	vector<string> declarations;
	vector<string> variables;
	
	using namespace std;
%}

%token VOID INT IDENTIFIER NUM LTE GTE EQL
%token ELSE IF RETURN WHILE
%token EXTERN READ PRINT

%nonassoc IF_ONLY
%nonassoc ELSE
%nonassoc RETURN
%right PRINT
%right '='
%left '<' LTE '>' GTE EQL
%left '+' '-'
%left '*' '/'
%left UMINUS

%start program

%%

/* rules */

program :
	print_declaration read_declaration func_declaration {
		root = createProg($1, $2, $3);
	}
|	read_declaration print_declaration func_declaration {
		root = createProg($1, $2, $3);
	}
;

print_declaration :
	EXTERN VOID PRINT '(' type_declaration ')' ';' {
		astNode* n = createExtern("print");
		$$ = n;
	}
;

read_declaration :
	EXTERN INT READ '(' ')' ';' {
		astNode* n = createExtern("read");
		$$ = n;
	}
;

func_declaration :
	type_declaration identifier '(' arg ')' block_statement {
		astNode* arg = $4;
  	    astNode* block = $6;
		astNode* n = createFunc(currText, arg, block);
		$$ = n;
	} 
|	type_declaration identifier '()' block_statement {
		astNode* block = $4;
		astNode* n = createFunc(currText, NULL, block);
		$$ = n;
	} 
;

arg : 
	type_declaration identifier {
		astNode* n = createVar(currText);
		$$ = n;
	}
;

block_statement :
  	'{' local_declarations statement_list '}' {
  	    vector<astNode*>* block_list = new vector<astNode*>;
  	    astNode* local_declarations = $2;
  	    astNode* statement_list = $3;
  	    if (local_declarations != NULL) {
  	    	vector<astNode*> slist = *(local_declarations->stmt.block.stmt_list);
			vector<astNode*>::iterator it = slist.begin();
			while (it != slist.end()){
				block_list->push_back(*it);
				it++;
			}
        }
        if (statement_list != NULL) {
        	vector<astNode*> slist = *(statement_list->stmt.block.stmt_list);
			vector<astNode*>::iterator it = slist.begin();
			while (it != slist.end()){
				block_list->push_back(*it);
				it++;
			}
        }
		astNode* n = createBlock(block_list);
		$$ = n;
  	}
;

local_declarations :
	local_declarations var_declaration {
		astNode* local_declarations = $1;
		astNode* var_declaration = $2;
		if (local_declarations != NULL) {
    		vector<astNode*>* decl_list = local_declarations->stmt.block.stmt_list;
        	decl_list->push_back(var_declaration);
      		$$ = local_declarations;
    	} 
    	else {
    		vector<astNode*>* decl_list = new vector<astNode*>;
    		decl_list->push_back(var_declaration);
    		astNode* n = createBlock(decl_list);
    		$$ = n;
      	}
  	}
| 	/* nothing */ {
		$$ = NULL;
  	}
;

var_declaration :
	type_declaration identifier ';'{
		astNode* n = createDecl(currText);
		$$ = n;
	}
;

type_declaration : 
	INT {
	}
;

statement_list : 
	statement_list statement {
		astNode* statement_list = $1;
		astNode* statement = $2;
		if (statement_list != NULL) {
    		vector<astNode*>* state_list = statement_list->stmt.block.stmt_list;
        	state_list->push_back(statement);
      		$$ = statement_list;
    	} 
    	else {
    		vector<astNode*>* state_list = new vector<astNode*>;
    		state_list->push_back(statement);
    		astNode* n = createBlock(state_list);
    		$$ = n;
      	}
  	}
|  	/* nothing */ {
		$$ = NULL;
	}
;

statement : 
	expression_statement { 
		$$ = $1; 
	}
|  	block_statement { 
		$$ = $1; 
	}
|  	if_statement { 
		$$ = $1; 
	}
|  	while_statement { 
		$$ = $1; 
	}
|  	return_statement { 
		$$ = $1; 
	}
|  	read_statement { 
		$$ = $1; 
	}
|  	print_statement { 
		$$ = $1; 
	}
;

expression_statement : 
	expression ';' { 
		$$ = $1;
	}
|	';' { 
		$$ = NULL; 
	}
;

expression : 
  	var '=' expression {
  		astNode* var = $1;
  		astNode* expression = $3;
  		astNode* n = createAsgn(var, expression);
  		$$ = n;
  	}
| 	r_value {
		$$ = $1;
  	}
;

r_value : 
  	expression '+' expression {
  		astNode* n = createBExpr($1, $3, add);
		$$ = n;
  	}  
| 	expression '-' expression {
		astNode* n = createBExpr($1, $3, sub);
		$$ = n;
  	}
| 	expression '*' expression {
		astNode* n = createBExpr($1, $3, mul);
		$$ = n;
  	}
| 	expression '/' expression {
		astNode* n = createBExpr($1, $3, divide);
		$$ = n;
  	}
| 	expression '<' expression {
		astNode* n = createRExpr($1, $3, lt);
		$$ = n;
  	}
| 	expression LTE expression {
		astNode* n = createRExpr($1, $3, le);
		$$ = n;
  	}
| 	expression '>' expression {
		astNode* n = createRExpr($1, $3, gt);
		$$ = n;
  	}
| 	expression GTE expression {
		astNode* n = createRExpr($1, $3, ge);
		$$ = n;
  	}
| 	expression EQL expression  {
		astNode* n = createRExpr($1, $3, eq);
		$$ = n;
  	}
| 	'-' expression %prec UMINUS {
		astNode* n = createUExpr($2, uminus);
		$$ = n;
	}
| 	var {
		$$ = $1;
  	}
| 	'(' expression ')' {
		$$ = $2;
  	}
|	'(' ')' {
		$$ = NULL;
	}
| 	NUM {
		astNode* n = createCnst(atoi(currText));
		$$ = n;
  	}
;

if_statement : 
	IF '(' expression ')' statement %prec IF_ONLY {
		astNode* n = createIf($3, $5, NULL);
		$$ = n;
  	}
|	IF '(' expression ')' statement ELSE statement {
		astNode* n = createIf($3, $5, $7);
		$$ = n;
  	}
;

while_statement : 
  	WHILE '(' expression ')' statement {
  		astNode* n = createWhile($3, $5);
  		$$ = n;
  	}
;

return_statement : 
	RETURN expression ';' {
		astNode* n = createRet($2);
		$$ = n;
  	}
;

print_statement : 
	PRINT expression ';' {
		astNode* n = createCall("print", $2);
		$$ = n;
  	} 

read_statement : 
	READ expression ';' {
		astNode* n = createCall("read", NULL);
		$$ = n;
  	} 

var : 
	identifier {
		astNode* n = createVar(currText);
		$$ = n;
	}
;

identifier :
	IDENTIFIER {
  	}
;

%%
/* function definitions */
int main(int argc, char *argv[]){
	yyparse();
	printf("\n");
 	printNode(root);
	traverseNode(root);
	if (findDuplicates(declarations)){
		printf("Duplicate declarations found!\n");
	}
	if (findUndeclared(declarations, variables)){
		printf("Undeclared variables found!\n");
	}
}

int findUndeclared(vector<string> declarations, vector<string> variables)
{
	set<string> variables_set;
	set<string> declarations_set;
	set<string> undeclared;
    copy(
    	variables.begin(), 
    	variables.end(), 
    	inserter(
			variables_set, 
    	 	variables_set.end()
    	)
    );
    copy(
    	declarations.begin(), 
    	declarations.end(), 
    	inserter(declarations_set, declarations_set.end())
    );
    set_difference(
    	variables_set.begin(), 
    	variables_set.end(), 
    	declarations_set.begin(), 
    	declarations_set.end(),
    	inserter(undeclared, undeclared.end())
    );
    return !undeclared.empty();
}

int findDuplicates(vector<string> words) 
{ 
    vector<string> duplicate; 
  
    sort(words.begin(), words.end()); 
  
    for (int i = 1; i<words.size(); i++) 
        if (words[i-1] == words[i]) 
            duplicate.push_back(words[i]); 
  
    int i = 0; 
    for (i = 0; i<duplicate.size(); i++) 
        return 1;
  
    return 0;
}

void traverseNode(astNode *node){	
	switch(node->type){
		case ast_prog:{
						traverseNode(node->prog.func);
						break;
					  }
		case ast_func:{
						if (node->func.param != NULL)
							declarations.push_back(node->func.param->var.name);
							
						traverseNode(node->func.body);
						break;
					  }
		case ast_stmt:{
						astStmt stmt= node->stmt;
 						traverseStmt(&stmt);
						break;
					  }
		case ast_extern:{
						break;
					  }
		case ast_var: {	
						variables.push_back(node->var.name);
						break;
					  }
		case ast_cnst: {
						break;
					  }
		case ast_rexpr: {
						traverseNode(node->rexpr.lhs);
						traverseNode(node->rexpr.rhs);
						break;
					  }
		case ast_bexpr: {
						traverseNode(node->bexpr.lhs);
						traverseNode(node->bexpr.rhs);
						break;
					  }
		case ast_uexpr: {
						traverseNode(node->uexpr.expr);
						break;
					  }
		default: {
					fprintf(stderr,"Incorrect node type\n");
				 }
	}
}

void traverseStmt(astStmt *stmt){
	switch(stmt->type){
		case ast_call: { 
							if (stmt->call.param != NULL)
								traverseNode(stmt->call.param);
							break;
						}
		case ast_ret: {
							traverseNode(stmt->ret.expr);
							break;
						}
		case ast_block: {
							vector<astNode*> slist = *(stmt->block.stmt_list);
							vector<astNode*>::iterator it = slist.begin();
							while (it != slist.end()){
								traverseNode(*it);
								it++;
							}
							break;
						}
		case ast_while: {
							traverseNode(stmt->whilen.cond);
							traverseNode(stmt->whilen.body);
							break;
						}
		case ast_if: {
							traverseNode(stmt->ifn.cond);
							traverseNode(stmt->ifn.if_body);
							if (stmt->ifn.else_body != NULL)
							{
								traverseNode(stmt->ifn.else_body);
							}
							break;
						}
		case ast_asgn:	{
							traverseNode(stmt->asgn.lhs);
							traverseNode(stmt->asgn.rhs);
							break;
						}
		case ast_decl:	{
							declarations.push_back(stmt->decl.name);
							break;
						}
		default: {
					fprintf(stderr,"Incorrect node type\n");
				 }
	}
}


void yyerror(const char *s){
	fprintf(stderr,"error: %s\n", s);
}
