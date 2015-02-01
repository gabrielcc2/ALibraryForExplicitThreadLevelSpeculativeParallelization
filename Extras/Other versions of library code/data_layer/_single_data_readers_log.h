#include <vector>
#include <pthread.h>
using namespace std;

// Definition of type _single_data_readers_log, that masks a std::vector of type int

//
// structure: _single_data_readers_log, used to encapsulate a vector of type int
//            containing the positions of all the threads that have read a particular
//            data item, as well as a mutex for synchonized use.
//
struct _single_data_readers_log {

	vector <int> _readers;
	pthread_mutex_t _mutex;

	_single_data_readers_log(){
		pthread_mutex_init (&_mutex, NULL);
	};

};
