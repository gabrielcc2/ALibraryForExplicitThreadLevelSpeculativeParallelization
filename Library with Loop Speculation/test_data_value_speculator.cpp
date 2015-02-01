#include <iostream>
#include "TLS_Lib.h"
#include <pthread.h>
using namespace std;

multibranch_speculator b;


void* funcion_b1 (void* arg){
	int a=20;
	int c=6;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	void* tmp= (void*)c;
	a=b.write_data((void*&)b.get_shared_data(), (void*)c, sizeof(int));
	return 0;
};

void* funcion_b2 (void* arg){
	int a=19;
	int valor=33;
	int* tmp=&valor;
	int* aux=((int*)b.get_shared_data()+1);
	a=b.write_data ((void*&)aux, (void*)valor, sizeof(int));
	return 0;
};
void* funcion_b3 (void* arg){
	int a=21;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(),sizeof (int));
	b.write_data((void*&)b.get_shared_data(), (void*)7, sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(8),  sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(9),  sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(10),  sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(11),  sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)12,  sizeof(int));

	return 0;
};

void* funcion_b4 (void* arg){
	int a=21;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(),  sizeof(int));
	int* aux=((int*)b.get_shared_data()+1);
	b.write_data ((void*&)aux, (void*)33, sizeof(int));
	return 0;
};

void* funcion_pb (void* arg){
	int x=120/4; 
	int a=b.write_data ((void*&)b.get_shared_data(), (void*)x, sizeof(int));
	int c=*(int*)b.read_data((void*)b.get_shared_data(),  sizeof(int));
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	b.append(funcion_b4, (void*)arg);
	b.append(funcion_b1, (void*)arg);
	b.append(funcion_b2, (void*)arg);
	cout<<"PB hizo varios append..."<<endl;
	b.validate_supposition (2);
	cout<<"terminó..."<<endl;
	return 0;
};

int main()
{
	int aux1[2];
	aux1[0]=17;
	aux1[1]=18;
	void* aux=&aux1[0];
	cout<<"aux valor antes de ejecucion: "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;

	script_vector auxiliar; //Tipo definido en TLS_Lib.h. Enmascara a un vector stl que contiene funciones: vector<void*(*)(void*)>.

	auxiliar.push_back(funcion_b1);
	auxiliar.push_back(funcion_b2);
	auxiliar.push_back(funcion_b3);


	vector <void*> constA;
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);

	cout<<"Inicio de especulación..."<<endl;

	int a;
	a= b.speculate((void*&)aux, auxiliar, constA, SCHED_FIFO); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	int x=120/4; 
	a=b.write_data ((void*&)aux, (void*)x,  sizeof(int));
	int c=*(int*)b.read_data((void*)aux,  sizeof(int));
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	b.append(funcion_b4, (void*)aux1);
	cout<<"Pasa el append..."<<a<<endl;
	b.validate_supposition (3);
	b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
        cout<<"Fin de especulación..."<<endl;
	cout<<"aux valor despues de ejecución (debería ser: 30,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;


	cout<<"se reusa el objeto..."<<endl;
	cout<<"Inicio de especulación..."<<endl;

	a= b.speculate((void*&)aux, funcion_pb, (void*)aux1, auxiliar, constA, SCHED_FIFO); //Se invoca la especulación
        cout<<"Fin de especulación..."<<endl;

	cout<<"aux valor despues de ejecución (debería ser: 12,33): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;

}
