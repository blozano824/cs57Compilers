/* definitions */
%{
	#include <stdio.h>
	#include <stdlib.h>
	#include <iostream> 
	#include <iterator> 
	#include <map> 
	#include <string>
	#include "y.tab.h"

	using namespace std; 
	extern void yyerror(const char *);
	int tokenSearch(char *);
	char *currText;
%}

/* macros */
white [ \t]+
letter [a-zA-Z]
digit [0-9]
id ({letter}|_)({letter}|{digit}|_)*
integer {digit}+

/* rules */
/* Am I allowed to simply detect that these function declaration exist? */
/* Add this to program scheme */
%%

{white}         			{}
{id} 						{currText = strdup(yytext);return tokenSearch(currText);}
{integer}     				{currText = strdup(yytext);return NUM;}
\(              			{return '(';}
\)              			{return ')';}
\{              			{return '{';}
\}              			{return '}';}
;               			{return ';';}
\+              			{return '+';}
-               			{return '-';}
\*              			{return '*';}
\/              			{return '/';}
=               			{return '=';}
\<              			{return '<';}
>               			{return '>';}
\<=             			{return LTE;}
>=              			{return GTE;}
==              			{return EQL;}
. 							{yyerror(yytext);}

%%

/* functions */
int yywrap()
{
	return 1;
}

int tokenSearch(char *word)
{
	// Initialize empty map
    map<string, int> wordMap;
    
	// Add all <word, token> pairs
	wordMap.insert(pair<string, int>("void", 	VOID));
	wordMap.insert(pair<string, int>("extern", 	EXTERN));
    wordMap.insert(pair<string, int>("else", 	ELSE));
    wordMap.insert(pair<string, int>("if", 		IF));
    wordMap.insert(pair<string, int>("int", 	INT));
    wordMap.insert(pair<string, int>("return", 	RETURN));
    wordMap.insert(pair<string, int>("while", 	WHILE));
    wordMap.insert(pair<string, int>("read", 	READ));
    wordMap.insert(pair<string, int>("print", 	PRINT));
    
    
    // Search for word in map 
    map<string, int>::iterator itr; 
    itr = wordMap.find(word);
    
    // Return token if word found in map
    if (itr == wordMap.end()){
        return IDENTIFIER;
    } 
    // Return identifier token if word not found in map
    else {
        return itr->second;
    }
}