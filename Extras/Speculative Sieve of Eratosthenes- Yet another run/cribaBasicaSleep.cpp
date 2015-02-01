//Criba basica sin los sleep

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
using namespace std;

int num_primes=100;

int main()
{

	bool vector_primos[num_primes];
	
	cout<<"Inicio del cálculo no especulativo."<<endl;

	for (int i=0; i<num_primes; i++){
		vector_primos[i]=true; //Inicialización.
	}
	for (int i=2; i<num_primes; i++){ //Determinación de números no primos.
		for (int j=2; j<ceil((float)num_primes/i); j++){
			vector_primos[j*i]=false; 
		}
	}

	cout<<"Fin del cálculo no especulativo."<<endl;

	cout<<"Primos calculados no especulativamente: "<<endl;	

	for (int i=2; i<num_primes; i++){
		if (vector_primos[i]){
				cout<<" "<<i;
		}
		sleep(1);	
	}
	cout<<endl;	
}

