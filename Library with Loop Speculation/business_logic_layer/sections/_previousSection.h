#include <pthread.h>
using namespace std;
//! \structure  _previousSection
//! \brief  structure used to encapsulate a thread with information for it's reseting.
//!         
//! Limitations: 
//!

struct _previousSection {	
		pthread_t _thread;
		pthread_mutex_t _mutex; 

		bool _isExternal; //this value will be used to signal if the previous section
                                  //is been run in a pthread created within the speculator (_isExternal=false)
                                  //or if it runs in thread that started earlier than the speculation (_isExternal=true).
                                  //This difference is properly signaled by the user in her call to speculate. If a function
                                  //is passed (in this call) for the previous section, then the previous section is to be run 
                                  //in a pthread created in the speculation (which means the forementioned call is a blocking call).
                                  //If no function is passed in the call, it is a non-blocking call, and the calling thread will
                                  //go on to execute the previous section.

		_previousSection(){
			_isExternal=true;
			pthread_mutex_init (&_mutex, NULL); 
		}
};	

