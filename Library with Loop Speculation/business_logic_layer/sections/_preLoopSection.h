#include <pthread.h>
using namespace std;
//! \structure  _preLoopSection
//! \brief  structure used to encapsulate the pre-loop information
//!
//! Limitations: 
//!

struct _preLoopSection: _previousSection {	
		bool _commit;

		_preLoopSection(){
			_commit=false;
		}
};	

