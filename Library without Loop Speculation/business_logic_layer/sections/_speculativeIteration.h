#include <map>
using namespace std;

//! \structure  _speculativeIteration
//! \brief  struct used to encapsulate the data for a speculativeIteration, mostly if it has commited or not.
//!
//! Limitations: 
//!

struct _speculativeIteration: _restartableThread {	

	bool _commit;
	//set for tracking the shared_data items that the thread has written. 
	set <void*> _written_data;

	_speculativeIteration(){
		_commit=false;
	}

};	

