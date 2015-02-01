#include "TLS_Lib.h"
#include <iostream>
using namespace std;

/*Esta es una prueba sencilla de especulación entre dos ramas. 
*/

two_branches_speculator b; //El especulador debe declararse como variable global.

int first_run=0;

/*Igual que cuando se programa con pthreads, deben definirse las funciones de cada una de las secciones paralelas. 
En este caso, las secciones paralelas serán las ramas y la pre-rama. */

void* funcion_pb (void* arg){ //Esta es una función para que ejecute la pre-rama. 
	//Se le puede asignar lo que se quiera, pero es importante tomar en cuenta dos cosas.

	//1) Cuando escribe sobre data compartida debe hacerlo a través de la función especulador.write_data(--).
        //2) Antes de terminar debe validar una de las dos ramas a través de especulador.validate_supposition (bool);

	int x=200/10;
	int a=b.write_data ((void*&)b.get_shared_data(), (void*)x, sizeof (int)); //Escribe 20 en el primer elemento del arreglo.
	/*
	Observa que write_data recibe la referencia a la data sobre la cual va a escribir, y una copia en forma de void* del valor
        que va a escribir. Por último recibe el tamaño de la data. 

        Quizás la interfaz de esta funcionalidad no sea la más amigable al usuario, pero basta para informar al modelo de ejecución
        que se va a escribir sobre cierta data compartida.
	*/

	b.validate_supposition (true);
	return 0;
};

void* funcion_b1 (void* arg){
	int a=20;
	int p= b.write_data((void*&)b.get_shared_data(), (void*)a, sizeof (int));
	return 0;
};

void* funcion_b2 (void* arg){
	int a=19;
	int c=33;
	void* tmp=(void*)c;
	void*& aux=(void*&)b.get_shared_data();
	a=b.write_data ((void*&)b.get_shared_data(), (void*)c, sizeof (int));
	a=b.write_data ((void*&)aux, (void*) c, sizeof (int));
	int tmp2=*(int*)b.read_data(b.get_shared_data(), sizeof(int));
	return 0;
};

void* funcion_c1 (void* arg){
	int* aux=(((int*)b.get_shared_data())+1);
	if (first_run!=0){
		b.write_data((void*&)aux,(void*)20, sizeof (int)); //Escribe 20 en el segundo elemento del arreglo.	
	}
	else {
		int a=20;
		b.write_data((void*&)b.get_shared_data(), (void*)a, sizeof (int));
		b.write_data((void*&)b.get_shared_data(), (void*)(a+1), sizeof (int)); //Escribe 21 en el primer elemento del arreglo.
		a=0;
		a=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int)); //Lee el 21 que escribió.
		a++;
		b.write_data((void*&)aux, (void*)a, sizeof (int)); //Escribe 22 en el segundo elemento del arreglo.
	}

	return 0;
};

void* funcion_c2 (void* arg){
	int a=19;
	void* tmp=(void*)(int)32;
	int* aux=(((int*)b.get_shared_data())+1);
	a=b.write_data ((void*&)aux, (void*)tmp, sizeof (int));
	int tmp2=*(int*)b.read_data((void*)b.get_shared_data(), sizeof(int));
	return 0;
};
int main()
{
	int aux1[2];
	aux1[0]=17;
	aux1[1]=18;
	void* aux=&aux1[0];
	cout<<"aux values before execution: "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl; //17 18

	int a;
	/*La invocación a especular recibe los argumentos de las ramas, seguidos de la función de cada rama.
	Por último se indica con un unsigned int, el tipo de planificación que se solicita para la ejecución
        de las hebras, de acuerdo al formato de pthreads.*/
	a= b.speculate((void*&)aux, funcion_b1,  (void*)aux1, funcion_b2, (void*)aux1, SCHED_FIFO);  

	/*Una vez iniciadas las dos ramas especulativas, el código que sigue es la pre-rama, mientras no se invoque
        especulador.get_results()...*/

	int x=160/4; 

	a=b.write_data ((void*&)aux, (void*)1, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)2, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)3, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)4, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)5, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)6, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)7, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)8, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)9, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)10, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)11, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)19, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)30, sizeof (int));
	int c=*(int*)b.read_data((void*)aux, sizeof(int));
	cout<<"PB-1 reads from shared data (should read 30): "<<c<<endl;

	b.validate_supposition (false); //b2=true. Lo que indica que el resultado final debe manifestar todos los cambios que pb 
        //y luego b2 realicen, pero nignuno de b1.

	a=b.write_data ((void*&)aux, (void*)30, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)12, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)12, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)21, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)14, sizeof (int));
	cout<<"PB-2 wrote aux1[0]= "<<aux1[0]<<endl; //Deberia ser 14
	a=b.write_data ((void*&)aux, (void*)17, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)15, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)13, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)16, sizeof (int));
	a=b.write_data ((void*&)aux, (void*)18, sizeof (int));	

	a=b.get_results(); //This ends the Pre-Branch
	cout<<"aux values after execution (should be: 33,18 ): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;



	//Ahora haremos una segunda ejecución, para probar que el especulador se resetea adecuadamente.

	a= b.speculate((void*&)aux, funcion_c1, (void*)aux1, funcion_c2, (void*)aux1, SCHED_RR); 
	
	/*Un-managed Pre-Branch...
	...
	*/


	b.validate_supposition (true); //c1=true.

	cout<<"Llega hasta aca..."<<endl;
	a=b.get_results(); //This ends the Pre-Branch
	cout<<"Pasa..."<<endl;
	cout<<"aux values after execution (should be: 21,22 ): "<<aux1[0]<<" "<<aux1[1]<<endl;

	first_run=-1;
	//Ahora especularemos pasandole la pre-rama como función. Esto se ejecuta como un llamado bloqueante.
	a= b.speculate ((void*&)aux, funcion_pb, (void*)aux1, funcion_c1, (void*)aux1, funcion_c2, (void*)aux1, SCHED_FIFO);
	cout<<"aux values after execution (should be: 20,20): "<<*(int*)(aux)<<" "<<*(((int*)aux)+1)<<endl;


	return 0;
}
