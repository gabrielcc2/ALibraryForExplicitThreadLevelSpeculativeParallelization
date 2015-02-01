#include <iostream>
#include "TLS_Lib.h"
using namespace std;

critical_section_speculator b;

int aux1[2]= {17, 18};
void* aux=&aux1[0];
pthread_t t0, t1, t2, t3, t4;

void* funcion_pb (void* arg){
	int x=120/4;
	int a=b.write_data ((void*&)b.get_shared_data(), (void*)1, sizeof(int));
	b.commit();
	return (void*)(int)0;
};

void* funcion_b1 (void* arg){
	int a=20;
	int tmp2=(int)b.read_data((void*&)b.get_shared_data(), sizeof(int));
/*	b.write_data((int*&)b.get_shared_data(), (int*)0);
	b.write_data((int*&)b.get_shared_data(), (int*)(1));
	b.write_data((int*&)b.get_shared_data(), (int*)(2));
	b.write_data((int*&)b.get_shared_data(), (int*)(3));
	b.write_data((int*&)b.get_shared_data(), (int*)(4));
	b.write_data((int*&)b.get_shared_data(), (int*)5);*/
        int* aux=(((int*&)b.get_shared_data())+1);
	a=b.write_data ((void*&)aux, (void*) 2, sizeof(int));
	b.commit();
	return (void*)(int)1;
};

void* funcion_b2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)20;
//	int* aux=(((int*&)b.get_shared_data())+1);
//	a=b.write_data ((int*&)aux, (int*) tmp);
//	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
	b.write_data ((void*&)b.get_shared_data(), (void*)(int)2, sizeof(int));
//	(int)b.read_data((int*&)b.get_shared_data());
	b.commit();
	return (void*)(int)2;
};
void* funcion_b3 (void* arg){
	int a=100;
	int* aux=(((int*&)b.get_shared_data())+1);
	b.write_data((void*&)aux, (void*)3, sizeof(int));
/*	b.write_data((int*&)b.get_shared_data(), (int*)(8));
	b.write_data((int*&)b.get_shared_data(), (int*)(9));
	b.write_data((int*&)b.get_shared_data(), (int*)(10));
	b.write_data((int*&)b.get_shared_data(), (int*)(11));
	b.write_data((int*&)b.get_shared_data(), (int*)12);*/
//	(int)b.read_data((int*&)b.get_shared_data());
	b.commit();
	return (void*)(int)3;
};

void* funcion_b4 (void* arg){
	int a=20;
//	int tmp2=(int)b.read_data((int*&)b.get_shared_data());
/*	b.write_data((int*&)b.get_shared_data(), (int*)13);
	b.write_data((int*&)b.get_shared_data(), (int*)(14));
	b.write_data((int*&)b.get_shared_data(), (int*)(15));
	b.write_data((int*&)b.get_shared_data(), (int*)(16));
	b.write_data((int*&)b.get_shared_data(), (int*)(17));*/
	b.write_data((void*&)b.get_shared_data(), (void*)3, sizeof(int));
	b.commit();
	return (void*)(int)4;
};
void* funcion_bloqueo (void* arg){
	void* p;
	if (pthread_self()==t0){
		cout<<"entra pb..."<<endl;
		p=b.speculate((void*&)aux, funcion_pb, arg, SCHED_RR); //PB, AUX=1
	}
	else if (pthread_self()==t1){
		cout<<"entra b1..."<<endl;
		p=b.speculate((void*&)aux, funcion_b1, arg, SCHED_RR); //B1, AUX2=2
	}
	else if (pthread_self()==t2){
		cout<<"entra b2..."<<endl;
		p=b.speculate((void*&)aux, funcion_b2, arg, SCHED_RR); //B2, AUX1=2
	}	
	else if (pthread_self()==t3){
		cout<<"entra b3..."<<endl;
		p=b.speculate((void*&)aux, funcion_b3, arg, SCHED_RR); //B3, AUX2=3
	}
	else if (pthread_self()==t4){
		cout<<"entra b4..."<<endl; //B4, AUX1=3
		p=b.speculate((void*&)aux, funcion_b4, arg, SCHED_RR);
	}
	return p;
}

int main()
{

	cout<<"aux valor antes de ejecucion: "<<*(int*)(aux);
	cout<<" "<<*(((int*)aux)+1)<<endl;

	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_create (&t0, &attr, funcion_bloqueo, aux);
	pthread_create (&t1, &attr, funcion_bloqueo, aux);
	pthread_create (&t2, &attr, funcion_bloqueo, aux);
	pthread_create (&t3, &attr, funcion_bloqueo, aux);
	pthread_create (&t4, &attr, funcion_bloqueo, aux);

	void* a, *c, *d, *e, *f;

	pthread_join(t0, &a);
	pthread_join(t1, &c);
	pthread_join(t2, &d);
	pthread_join(t3, &e);
	pthread_join(t4, &f);

	cout<<(int)a<<" "<<(int)c<<" "<<(int)d<<" "<<(int)e<<" "<<(int)f<<" "<<endl;
	
	cout<<"aux valor despues de ejecución (deben ser valores de fin de hebra): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;
		
}

