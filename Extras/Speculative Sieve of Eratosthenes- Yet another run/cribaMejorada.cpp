//Criba mejorada con los 100

#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include "TLS_Lib.h"
using namespace std;

/*Este programa compara el rendimiento de la especulación de lazos vs. la iteración no especulativa. 
Para ello se implementa la Criba de Eratostenes, evaluando si 100 números son primos.

La versión especulativa consiste divide el problema en dos secciones, una primera especulación utilizada
para inicializar el vector, y una segunda que calcula los números primos. Para la extracción del paralelismo
se utilizó solo el lazo externo del método, asignándose cada iteración de este a una hebra distinta. Por este
motivo este programa ejemplo carece de escalabilidad: el sistema no podrá proveer un mayor número de hebras, de hecho
100 son bastantes hebras que pedir a un sistema común (por la forma del algoritmo, en realidad se le pedirá poco menos de la
mitad).

En una primera corrida el rendimiento del programa tradicional es similar al especulativo, esto es de
esperar puesto que el programa tradicional consiste exclusivamente de operaciones de lecturas y escrituras sobre la
data compartida. La versión especulativa realiza estas operaciones en un modo casi tan secuencial como el del programa
base, y además le añade el sobrecosto de la gestión del modelo, por todo esto las ventajas del paralelismo no son tan evidentes,
aunque por lo general el rendimiento de la especulación supera al tradicional. 

Una evidente mejora del rendimiento se observaría si el lazo externo incluyese una carga adicional que no involucrase operaciones 
estrictas sobre la data compartida. A fin de observar un caso semejante se realizó una segunda corrida del método anterior, 
introduciendo una operación sleep(1) al final de cada iteración del lazo exterior. En el caso especulativo esta carga se solapa 
entre las varias hebras, por lo cual se observará un rendimiento notablemente superior al caso secuencial. 

 */

loop_speculator b;
int num_primes=30;

void* initialize (void* arg){
	int base=(((int)arg)-1)*(num_primes/10);
	int tope=((int)arg)*(num_primes/10);

	bool aux2=true;
	for (int i=base; i<tope; i++){
		bool* aux=((bool*&)b.get_shared_data()+i);
		while (b.write_data((bool*&)aux, (bool*)aux2)!=0){
		}
	}
	b.commit();
};

void* get_primes (void* arg){
	bool aux2=false;
	for (int j=2; j<ceil((float)num_primes/(int)arg); j++){
		bool* aux=((bool*&)b.get_shared_data()+(j*(int)arg));
		b.write_data((bool*&)aux, (bool*)aux2);
	}
	b.commit();
}



int main()
{

	bool vector_primos[num_primes];
	bool* argumento=&vector_primos[0];

	script_vector auxiliar;
	auxiliar.push_back(initialize); //Función a pasar para primera iteración
	vector <void*> constA;
	constA.push_back((void*)1); //Argumento de primera iteración.

	cout<<"Inicio del cálculo especulativo."<<endl;

	//En primer lugar se inicializará el vector: 

	b.speculate((void*&)argumento, auxiliar, constA); //Se manda a especular con solo la primera iteración.
	//Lo siguiente corresponderá al código pre-especulativo.

	for (int i=2; i<(num_primes/10)+1; i++){
		b.append(initialize, (void*)i); //Se añaden las demás iteraciones y argumentos. 

	//Cada iteración inicializará 10 posiciones del vector.
	
	}
	b.commit(); //Se marca que la sección pre-especulativa ha terminado.
	b.get_results(); //Se piden los resultados de la especulación.

	//La segunda fase especulativa consiste en determinar los números que no son primos.
	//Lo siguiente corresponderá al código pre-especulativo.

	auxiliar.clear();
	auxiliar.push_back(get_primes); //Función a pasar para la primera iteración
	constA.clear();
	constA.push_back((void*)2); //Argumento de primera iteración.
	b.speculate((void*&)argumento, auxiliar, constA); //Se manda a especular con solo la primera iteración.
	for (int i=3; i<num_primes; i++){
		b.append(get_primes, (void*)i); //Se añaden las demás iteraciones y argumentos.
	}
	b.commit();//Se marca que la sección pre-especulativa ha terminado.
	b.get_results();  //Se piden los resultados de le especulación.

	cout<<"Fin del cálculo especulativo."<<endl;

	cout<<"Primos calculados especulativamente: "<<endl;

	for (int i=2; i<num_primes; i++){
		if (vector_primos[i]){
				cout<<" "<<i;
		}
	}
	cout<<endl;	
}
