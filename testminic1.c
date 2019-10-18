extern void print(int);
extern int read();


int func1(int a){
	int loc1; 
	int loc2;
	int i;

	loc1 = a + 10;
	loc2 = a * 10;
	
	if (loc1 > loc2) {
		print(loc1);
	}
	
	if (loc1 > loc2) {
		print(loc1);
	}
	else 
		print(loc2);
	
	
	i = 0;

	while (i < loc1){
		i = i + 1;		
		loc2 = loc2 + i;
	}

	return(loc1 + loc2); 
}