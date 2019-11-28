#include<stdio.h>

extern int func(int);

void print(int a){
	printf("%d\n", a);
}

int read(){
	int i;
	scanf("%d", &i);
	return i; 
}

int main(){
	int i = func(0);
	printf("%d\n", i);
	return 0;
}
