#include <pthread.h>
using namespace std;
//!
//! \class  ht_speculator
//!
//! \brief  implements a class that allows the user to use the helper-thread model 
//!         to do run-ahead execution and pre-fetch data that it's program will use.
//!         in order to do this, the class provides one only function: speculate, which 
//!         allows the user to run his pre-fetch and run-ahead functions, setting an
//!         affinity with the cpu used by the calling thread.
//!
//!         TO BE NOTED: In the current version this function only masks a call to pthread_create.
//!         Future versions might explore the possibilities of setting the speculative thread to
//!         the exact same CPU used by the calling thread, or signal changes in the calling thread
//!         affinity so those could be made on the speculative thread affinity.
//!
//! Limitations: 
//!
//! * If the affinity of the calling thread changes after the call, this won't be reflected
//! on the speculative threads.
//!
//! * Sequential consistency is mostly guaranteed, save for exception 
//! behaviour.
//!
class ht_speculator {
public: 

	//!
        //! speculate: a function that takes the pre-fetch or run-ahead function of the user, 
        //! as well as it's arguments, and starts it's speculative execution as a detached thread, 
	//! while setting it's affinity with the CPU used by the calling thread. its sched_policy
	//! is the third parameter.
	//!
	void speculate (void* (f)(void*), void* consts, int sched_policy){
		
		pthread_t  thread_to_run;	
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		pthread_attr_setschedpolicy(&attr, sched_policy);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		
		pthread_create (&thread_to_run, &attr, f, consts);//inherits the affinity mask of the calling thread.
		
	};

	
};
