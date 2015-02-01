#include <iostream>
#include "TLS_Lib.h"
#include <pthread.h>
using namespace std;

data_value_speculator b;


void* funcion_b1 (void* arg){
	int a=20;
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	b.write_data((int*&)b.get_shared_data(), (int*)1);
	b.write_data((int*&)b.get_shared_data(), (int*)(2));
	b.write_data((int*&)b.get_shared_data(), (int*)(3));
	b.write_data((int*&)b.get_shared_data(), (int*)(4));
	b.write_data((int*&)b.get_shared_data(), (int*)(5));
	b.write_data((int*&)b.get_shared_data(), (int*)6);
	return 0;
};

void* funcion_b2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)33;
	int* aux=(((int*&)b.get_shared_data())+1);
	a=b.write_data ((int*&)aux, (int*)tmp);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	return 0;
};
void* funcion_b3 (void* arg){
	int a=21;
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	b.write_data((int*&)b.get_shared_data(), (int*)7);
	b.write_data((int*&)b.get_shared_data(), (int*)(8));
	b.write_data((int*&)b.get_shared_data(), (int*)(9));
	b.write_data((int*&)b.get_shared_data(), (int*)(10));
	b.write_data((int*&)b.get_shared_data(), (int*)(11));
	b.write_data((int*&)b.get_shared_data(), (int*)12);

	return 0;
};

void* funcion_b4 (void* arg){
	int a=21;
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	b.write_data((int*&)b.get_shared_data(), (int*)13);
	b.write_data((int*&)b.get_shared_data(), (int*)(14));
	b.write_data((int*&)b.get_shared_data(), (int*)(15));
	b.write_data((int*&)b.get_shared_data(), (int*)(16));
	b.write_data((int*&)b.get_shared_data(), (int*)(17));
	b.write_data((int*&)b.get_shared_data(), (int*)18);
	b.write_data((int*&)b.get_shared_data(), (int*)19);
	b.write_data((int*&)b.get_shared_data(), (int*)20);

	return 0;
};

void* funcion_pb (void* arg){
	int x=120/4; 
	int a=b.write_data ((int*&)b.get_shared_data(), (int*)x);
	int c=(int)b.read_data((int*&)b.get_shared_data());
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	b.append(funcion_b4, (void*)arg);
	cout<<"hizo el append..."<<endl;
	b.validate_supposition (0);
	cout<<"terminó..."<<endl;
	return 0;
};

int main()
{
	int aux1[2];
	aux1[0]=17;
	aux1[1]=18;
	void* aux=&aux1[0];
	cout<<"aux valor antes de ejecucion: "<<*(int*)(aux);
	cout<<" "<<*(((int*)aux)+1)<<endl;
	script_vector auxiliar;
	auxiliar.push_back(funcion_b1);
	auxiliar.push_back(funcion_b3);
	auxiliar.push_back(funcion_b2);
	vector <void*> constA;
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	cout<<"Here we go..."<<endl;
	int a;
	a= b.speculate((void*&)aux, auxiliar, constA); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	int x=120/4; 
	a=b.write_data ((int*&)aux, (int*)x);
	int c=(int)b.read_data((int*&)aux);
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	b.append(funcion_b4, (void*)aux1);
	b.validate_supposition (2);
	cout<<"Valido..."<<endl;
	a=b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
	cout<<"Valor de a "<<a<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;

	cout<<"se reusa el objeto..."<<endl;

	a= b.speculate((void*&)aux, auxiliar, constA); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	x=120/4; 
	a=b.write_data ((int*&)aux, (int*)x);
	c=(int)b.read_data((int*&)aux);
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	b.append(funcion_b4, (void*)aux1);
	b.validate_supposition (2);
	cout<<"Valido..."<<endl;
	a=b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
	cout<<"Valor de a "<<a<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;

	cout<<"se reusa el objeto de nuevo..."<<endl;

	a= b.speculate((void*&)aux, funcion_pb, (void*)aux1, auxiliar, constA); //Se invoca la especulación
	cout<<"Valor de a "<<a<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 6,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 6,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 6,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;

	cout<<"Exito..."<<endl;
}
