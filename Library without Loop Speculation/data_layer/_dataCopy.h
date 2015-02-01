#include <stdlib.h>
using namespace std;
//
// structure: _data_copy, used to encapsulate a copied data and it's size.
//
struct _data_copy {
	unsigned int _size;
	void * _data;	

	_data_copy(){
		_size=0;
		_data=NULL;
	}

};

