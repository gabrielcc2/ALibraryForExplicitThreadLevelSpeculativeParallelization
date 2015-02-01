#include <iostream>
#include "TLS_Lib.h"
using namespace std;

loop_speculator b;

void* funcion_pb (void* arg){
	int x=120/4;
	int a=b.write_data ((int*&)b.get_shared_data(), (int*)x);
	cout<<"Valido..."<<endl;
	b.commit();
	return 0;
};

void* funcion_b1 (void* arg){
	//cout<<"Inicio 0"<<endl;
	int a=20;
//	sleep(2);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B0 lee (deberia ser 30): "<<tmp2<<endl;
	b.write_data((int*&)b.get_shared_data(), (int*)20);
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(2));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)5);
	//cout<<"Fin 0"<<endl;
	b.commit();
	return 0;
};

void* funcion_b2 (void* arg){
	//cout<<"Inicio 1"<<endl;
	int a=19;
	void* tmp=(void*)(int)20;
	int* aux=(((int*&)b.get_shared_data())+1);
	a=b.write_data ((int*&)aux, (int*) tmp);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B1 lee (deberia ser 20): "<<tmp2<<endl;
	b.write_data ((int*&)b.get_shared_data(), (int*)(int)6);
	tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B1 lee (deberia ser 20): "<<tmp2<<endl;
	//cout<<"Fin 1"<<endl;
	b.commit();
	return 0;
};
void* funcion_b3 (void* arg){
	//cout<<"Inicio 2"<<endl;
	int a=100;
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B2 lee (deberia ser 22): "<<tmp2<<endl;
	b.write_data((int*&)b.get_shared_data(), (int*)7);
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)12);
	tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B2 lee (deberia ser 20): "<<tmp2<<endl;
	//cout<<"Fin 2"<<endl;
	b.commit();
	return 0;
};

void* funcion_b4 (void* arg){
	//cout<<"Inicio 0"<<endl;
	int a=20;
//	sleep(2);
	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	//cout<<"B0 lee (deberia ser 30): "<<tmp2<<endl;
	b.write_data((int*&)b.get_shared_data(), (int*)13);
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(20));
	b.write_data((int*&)b.get_shared_data(), (int*)(17));
	b.write_data((int*&)b.get_shared_data(), (int*)a);
	//cout<<"Fin 0"<<endl;
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
	
	int a= b.speculate((void*&)aux, auxiliar, constA); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	int x=120/4; 
	cout<<"efecto de mandar a especular: "<<a<<endl;
//	sleep(4);
	a=b.write_data ((int*&)aux, (int*)x);
	int c= (int)b.read_data ((int*&)aux);
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
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

	a= b.speculate((void*&)aux, funcion_pb, (void*)aux1, auxiliar, constA); //Se invoca la especulación
	if (a==0){
		cout<<"aux valor despues de ejecución (debería ser: 20,20): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		if ((*(int*)aux)!=20 || (*(((int*)aux)+1))!=20){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<(*(int*)aux)<<" "<<(*(((int*)aux)+1))<<endl;
		}
		
	}
	else {
		cout<<"Falla en el join..."<<endl;
	}

	a= b.speculate((void*&)aux, auxiliar, constA); //Se invoca la especulación
	/*Lo siguiente corresponderá al código de la sección pre-rama*/
	x=120/4; 
	cout<<"efecto de mandar a especular: "<<a<<endl;
//	sleep(4);
	a=b.write_data ((int*&)aux, (int*)x);
	c= (int)b.read_data ((int*&)aux);
	x=b.append(funcion_b4, (void*)aux1);
	cout<<"append: "<<x<<endl;
	x=b.valid_til(1);
	cout<<"valid_til: "<<x<<endl;
	cout<<"PB lee (deberia ser 30): "<<c<<endl;
	cout<<"Voy a invocar resultados..."<<endl;
	b.commit();
	a=b.get_results(); //Con la invocación de esta función se finaliza la pre-rama.
	if (a==0){
		cout<<"aux valor despues de ejecución (debería ser: 6,20): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		if ((*(int*)aux)!=6 || (*(((int*)aux)+1))!=20){
		cout<<"*************************************************************************************************************************"<<endl;
		cout<<(*(int*)aux)<<" "<<(*(((int*)aux)+1))<<endl;
		}

	}
	else {
		cout<<"Falla en el join..."<<endl;
	}
	
}

