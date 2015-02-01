#include <pthread.h>
#include <set>
using namespace std;
//! \structure  _restartableThread
//! \brief  structure used to encapsulate a thread with information for it's reseting and
//! for keeping track of read_data, necessary for RAW dependency checking.
//! Limitations: 
//!

struct _restartableThread {	
		//mutex related to thread
		pthread_mutex_t _mutex; 

		pthread_t _thread;

		void*(*_instructions)(void*);
		void* _args;

		//set for tracking the shared_data items that the thread has read. 
		set <void*> _read_data;	

		_restartableThread(){
			_instructions=NULL;
			_args=NULL;
			pthread_mutex_init (&_mutex, NULL); 
		}
};	

