#include <pthread.h>
#include "_speculator.h"
using namespace std;

//! \class  conditional_speculator
//! \brief  implements an abstract class that is the basis for instantiable child
//!         classes such as two_branches_speculator and multibranch_speculator. 
//!         It simply encapsulates the speculator state (active or inactive) and the
//!         reference of the shared data, additionally a mutex to a data item carrying
//!         the id of the valid_branch. Operations over those elements have to be
//!         implemented in the instantiable classes.
//!
//! Limitations: 
//!

class _conditional_speculator: protected _speculator {
	protected:
		
		//mutex related to _valid_branch
		pthread_mutex_t _valid_branch_mutex; 
		
		_conditional_speculator (){ //necesarry to initialize the mutex.
			pthread_mutex_init (&_valid_branch_mutex, NULL); 
		}
};	
