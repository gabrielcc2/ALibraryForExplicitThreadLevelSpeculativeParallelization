#include <iostream>
#include "TLS_Lib.h"
using namespace std;

/*Este programa es un ejemplo arbitrario del uso del especulador de lazos. Se declaran 4 funciones, una para cada iteración
y se las ejecuta especulativamente en paralelo a la sección pre-lazo.
*/

loop_speculator b;


void* funcion_pb (void* arg){
	int x=120/4;
	int a=b.write_data ((void*&)b.get_shared_data(), (void*)x, sizeof(int));
	b.commit();
	return 0;
};

void* funcion_b1 (void* arg){
	int a=20;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)20, sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(19), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(20), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(19), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(11), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)20, sizeof(int));
	b.commit();
	return 0;
};

void* funcion_b2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)21;
	int* aux=(((int*&)b.get_shared_data())+1);
	a=b.write_data ((void*&)aux, (void*) 19, sizeof(int));
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.write_data ((void*&)b.get_shared_data(), (void*)(int)21, sizeof(int));
	tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.commit();
	return 0;
};

void* funcion_b3 (void* arg){
	int a=100;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)22, sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(23), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(22), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(23), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(22), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)23, sizeof(int));
	tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.commit();
	return 0;
};

void* funcion_b4 (void* arg){
	int a=23;
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)23, sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(24), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(23), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(24), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)(25), sizeof(int));
	b.write_data((void*&)b.get_shared_data(), (void*)a, sizeof(int));
	b.commit();
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
	auxiliar.push_back(funcion_b2);
	auxiliar.push_back(funcion_b3);
	auxiliar.push_back(funcion_b4);
	vector <void*> constA;
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	constA.push_back((void*)aux1);
	cout<<"Here we go..."<<endl;
	
	int a= b.speculate((void*&)aux, auxiliar, constA, SCHED_RR); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	int x=120/4; 
	cout<<"efecto de mandar a especular: "<<a<<endl;
	a=b.write_data ((void*&)(void*&)aux, (void*)x, sizeof(int));
	int c= *(int*)b.read_data ((void*&)aux, sizeof(int));
	cout<<"PL lee (deberia ser 30): "<<c<<endl;
	if (c!=30){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<"Error de lectura desde el pre-lazo: "<<c<<endl;
		}
	x=b.append(funcion_b4, (void*)aux1);
	cout<<"append: "<<x<<endl;
	x=b.valid_til(-1);
	cout<<"valid_til: "<<x<<endl;
	cout<<"Voy a invocar resultados..."<<endl;
	b.commit();
	a=b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
	if (a==0){
		cout<<"aux valor despues de ejecución (debería ser: 30,18): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		if ((*(int*)aux)!=30 || (*(((int*)aux)+1))!=18){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<(*(int*)aux)<<" "<<(*(((int*)aux)+1))<<endl;
		}
	}
	else {
		cout<<"Falla en el join..."<<endl;
	}

	a= b.speculate((void*&)aux, funcion_pb, (void*)aux1, auxiliar, constA, SCHED_RR); //Se invoca la especulación
	if (a==0){
		cout<<"aux valor despues de ejecución (debería ser: 23,19): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		if ((*(int*)aux)!=23 || (*(((int*)aux)+1))!=19){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<(*(int*)aux)<<" "<<(*(((int*)aux)+1))<<endl;
		}
		
	}
	else {
		cout<<"Falla en el join..."<<endl;
	}

	a= b.speculate((void*&)aux, auxiliar, constA, SCHED_RR); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	x=120/4; 
	cout<<"efecto de mandar a especular: "<<a<<endl;
	a=b.write_data ((void*&)aux, (void*)x, sizeof(int));
	c= *(int*)b.read_data ((void*)aux, sizeof(int));
	x=b.append(funcion_b4, (void*)aux1);
	cout<<"append: "<<x<<endl;
	x=b.valid_til(1);
	cout<<"valid_til: "<<x<<endl;
	cout<<"PL lee (deberia ser 30): "<<c<<endl;
	if (c!=30){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<"Error de lectura desde el pre-lazo: "<<c<<endl;
	}
	cout<<"Voy a invocar resultados..."<<endl;
	b.commit();
	a=b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
	if (a==0){
		cout<<"aux valor despues de ejecución (debería ser: 21,19): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		if ((*(int*)aux)!=21 || (*(((int*)aux)+1))!=19){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<(*(int*)aux)<<" "<<(*(((int*)aux)+1))<<endl;
		}

	}
	else {
		cout<<"Falla en el join..."<<endl;
	}
	
}
