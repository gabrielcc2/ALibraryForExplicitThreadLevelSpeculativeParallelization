#include "TLS_Lib.h"
#include <iostream>
using namespace std;

branch_speculator b;

void* funcion_pb (void* arg){
	int x=120/4;
	int a=b.write_data ((int*&)b.get_shared_data(), (int*)x);
	cout<<"PB reads from shared data (should read 30): "<<(int)b.read_data((int*&)b.get_shared_data())<<endl;
	b.validate_supposition (true);
	return 0;
};

void* funcion_b1 (void* arg){
	int a=20;
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	return 0;
};

void* funcion_b2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)33;
	int* aux=(((int*)b.get_shared_data())+1);
	a=b.write_data ((int*&)aux, (int*)tmp);
	a=b.write_data ((int*&)aux, (int*)tmp);
	a=b.write_data ((int*&)aux, (int*)tmp);
	a=b.write_data ((int*&)aux, (int*)tmp);
	a=b.write_data ((int*&)aux, (int*)tmp);
	a=b.write_data ((int*&)aux, (int*)tmp);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	return 0;
};

void* funcion_c1 (void* arg){
	int a=20;
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	b.write_data((int*&)b.get_shared_data(), (int*)(a+1));
	a=(int)b.read_data((int*&)b.get_shared_data());
	a=20;
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	return 0;
};

void* funcion_c2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)32;
	int* aux=(((int*)b.get_shared_data())+1);
	a=b.write_data ((int*&)aux, (int*)tmp);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	return 0;
};
int main()
{
	int aux1[2];
	aux1[0]=17;
	aux1[1]=18;
	void* aux=&aux1[0];
	cout<<"aux values before execution: "<<*(int*)(aux);
	cout<<" "<<*(((int*)aux)+1)<<endl;
	int a;
	a= b.speculate((void*&)aux, funcion_b1,  (void*)aux1, funcion_b2, (void*)aux1); 
	/*Un-managed Pre-Branch...*/
	int x=120/4; 
	a=b.write_data ((int*&)aux, (int*)1);
	a=b.write_data ((int*&)aux, (int*)2);
	a=b.write_data ((int*&)aux, (int*)3);
	a=b.write_data ((int*&)aux, (int*)4);
	a=b.write_data ((int*&)aux, (int*)5);
	a=b.write_data ((int*&)aux, (int*)6);
	a=b.write_data ((int*&)aux, (int*)7);
	a=b.write_data ((int*&)aux, (int*)8);
	a=b.write_data ((int*&)aux, (int*)9);
	a=b.write_data ((int*&)aux, (int*)10);
	a=b.write_data ((int*&)aux, (int*)11);
	a=b.write_data ((int*&)aux, (int*)19);
	a=b.write_data ((int*&)aux, (int*)30);
	int c=(int)b.read_data((int*&)aux);
	cout<<"PB reads from shared data (should read 30): "<<c<<endl;
	b.validate_supposition (false);
	a=b.write_data ((int*&)aux, (int*)30);
	a=b.write_data ((int*&)aux, (int*)12);
	a=b.write_data ((int*&)aux, (int*)12);
	a=b.write_data ((int*&)aux, (int*)21);
	a=b.write_data ((int*&)aux, (int*)14);
	a=b.write_data ((int*&)aux, (int*)17);
	a=b.write_data ((int*&)aux, (int*)15);
	a=b.write_data ((int*&)aux, (int*)13);
	a=b.write_data ((int*&)aux, (int*)16);
	a=b.write_data ((int*&)aux, (int*)18);	
	a=b.write_data ((int*&)aux, (int*)30);

	a=b.get_results(); //This ends the Pre-Branch
	cout<<"aux values after execution (should be: 30,33 ): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;



	a= b.speculate((void*&)aux, funcion_c1, (void*)aux1, funcion_c2, (void*)aux1); 
	/*Un-managed Pre-Branch...*/
	b.validate_supposition (true);
	a=b.get_results(); //This ends the Pre-Branch

	cout<<"aux values after execution (should be: 20,33 ): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;


	a= b.speculate ((void*&)aux, funcion_pb, (void*)aux1, funcion_c1, (void*)aux1, funcion_c2, (void*)aux1);
	cout<<"Exito: "<<a<<endl;
	cout<<"aux values after execution (should be: 20,33 ): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"Exito: "<<a<<endl;


	return 0;
}
