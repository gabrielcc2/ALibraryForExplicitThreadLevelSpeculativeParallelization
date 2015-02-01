#include <pthread.h>
#include <map>
#include <vector>
#include <string.h>
#include <signal.h>
#include <set>

using namespace std;

// Definition of type script_function, that masks a void* (*)(void*
typedef void* (*script_function)(void*); 
// Definition of type script_vector, a std::vector of type script_function
typedef std::vector<script_function> script_vector;


//
// structure: _parallel_loop_thread, used to encapsulate a pthread, 
//           which will run a thread for parallel loop execution in batches, 
//           keeping as well some related data. 
//
struct _parallel_loop_thread{ 
	pthread_mutex_t _thread_mutex;
	pthread_t _thread;
	
	bool _commit;//helps to determine if the loop iteration 
	//that the _loop_spec_thread represents has commited or not.

	_parallel_loop_thread(){
		pthread_mutex_init (&_thread_mutex, NULL);
		_commit=false;
	};
	
};

//!
//! \class  parallel_loop_in_batches
//!
//! \brief  implements a class that takes a window of n ordered interations from a loop and 
//!         executes them in parallel without consistency checking. To ensure that all the
//!         iterations are completed, the class relies on a commit function.
//!         
//!         Additional functionality is provided with an append function.
//!
//!         This class is the same as loop_speculator, but without the dependency tracking.
//!
class parallel_loop_in_batches {
private: 

     	//bool values to set if the object is active, or has control
	//of the pre-loop section 
	bool _is_active, _has_pre_loop; 

	//mutex for synchronized use of the previous group of variables
	pthread_mutex_t _is_active_mutex; 

	//thread to manage the pre-loop section, boolean value to check if
	//it has ended it's execution & related mutex
	pthread_t _pl; 
	bool _pl_commit; 
	pthread_mutex_t _pl_mutex; 

	//the parallel loop threads and it's data
	vector <_parallel_loop_thread> _loop_threads;

	//red-black tree, used as a thread index to relate a pthread_id with 
	//it's model id, i.e. it's position in _loop_threads
	map <pthread_t, int> _thread_index; 

	//mutex for a synchronized access to both of the former
	pthread_mutex_t _loop_threads_mutex;

	
        //! private method that allows to cancel threads whose
        //! positions are passed as an argument. It is used if there is a 
	//! error on starting the execution, and also for resetting the object.
        //!
        //! TO BE NOTED:
        //! * this method takes all class mutexes. On invocation
	//!   _loop_threads_mutex should be on hold and no other class mutex, 
	//!   save those related to a thread that is not about to be canceled or restarted.
	//! 
	//! * if called with an empty vector, it will reset all logs and
	//! wait for the threads in deferred cancel to finish.
	//! 
	//! 
	void _cancel_loop_threads_and_reset_logs (vector <int> threads_to_delete){

		int i;
		bool valid_arguments=true;

		if (!threads_to_delete.empty()){

			for (i=0; i<threads_to_delete.size(); i++){

				if (threads_to_delete[i]<0 || threads_to_delete[i]> static_cast<int>(_loop_threads.size())){
					valid_arguments=false; //one thread has an invalid id.
					i=threads_to_delete.size();
				}
					
			}
		}
		else{
			valid_arguments=false; //no threads to delete

			/*This option will be used to initialize the object, reseting it's inner arrays*/

			pthread_mutex_lock(&_is_active_mutex);

			if (_has_pre_loop)
	
				pthread_mutex_lock(&_pl_mutex);

			if (!_loop_threads.empty()){
				_loop_threads.clear(); 
			}				


			if (!_thread_index.empty()){ 

				/*in order to clear the _thread_index, it will be necessary to guarantee that all the threads in deferred
				cancel have finished, and thus will not use the object*/

				if (_has_pre_loop)
					pthread_mutex_unlock(&_pl_mutex);
		
				pthread_mutex_unlock(&_is_active_mutex);

				pthread_mutex_unlock(&_loop_threads_mutex);
					
				map <pthread_t, int>::iterator it;
				for (it=_thread_index.begin(); it!=_thread_index.end(); it++){
					if (it->second==-1){
						int a;
						do {
							a= pthread_kill(it->first, 0);
						} while (a==0);
					}
				}
				
				pthread_mutex_lock(&_loop_threads_mutex);

				_thread_index.clear();

			}
			else {
				if (_has_pre_loop)
			
					pthread_mutex_unlock(&_pl_mutex);
		
				pthread_mutex_unlock(&_is_active_mutex);

			}			
			
		}


		if (valid_arguments){
	
			for (unsigned int i=threads_to_delete.size()-1; i>=0; i--){
				pthread_mutex_lock(&_loop_threads[threads_to_delete[i]]._thread_mutex);
			}

			pthread_t thrd_id=pthread_self();

			pthread_mutex_lock(&_is_active_mutex);

			if (_has_pre_loop)
	
				pthread_mutex_lock(&_pl_mutex);		

			for (unsigned int i=0; i<threads_to_delete.size(); i++){
				if (pthread_equal(_loop_threads[threads_to_delete[i]]._thread, thrd_id)==0){
					if (pthread_kill(_loop_threads[threads_to_delete[i]]._thread, 0)==0){
						pthread_cancel(_loop_threads[threads_to_delete[i]]._thread);
					}
					_thread_index[_loop_threads[threads_to_delete[i]]._thread]= -1;
					pthread_mutex_destroy(&_loop_threads[threads_to_delete[i]]._thread_mutex); 
				}
				
			}
	
			if (_has_pre_loop)
				pthread_mutex_unlock(&_pl_mutex);

			pthread_mutex_unlock(&_is_active_mutex);

		}
	};
	
public: 

	//!
        //! default constructor
        //!
	parallel_loop_in_batches(){

		_is_active=false;
		_pl_commit=false;
		_has_pre_loop=false;
		pthread_mutex_init (&_is_active_mutex, NULL); 
		pthread_mutex_init (&_pl_mutex, NULL); 
		pthread_mutex_init (&_loop_threads_mutex, NULL); 	
	};


	//!
        //! required function that signals that a given thread has ended
        //! it's execution, it is assumed to be called from a valid 
	//! thread
	//!
	void commit (){ 
		pthread_t thread_id=pthread_self();
		if (_thread_index.find(thread_id)!=_thread_index.end()){
			int pos=_thread_index.find(thread_id)->second;
			if (pos>=0){ //valid thread
				_loop_threads[pos]._commit=true;
			}
		}
		else {	//unmanaged pre-loop				
			_pl_commit=true;					
		}
	};

	
	//!
        //! function that permits the pre_loop section to dynamically append
        //! a new thread at the end of the array in an on-going
	//! execution.
        //! no checks are made, in order to keep performance.
        //! 
	int append(void* (f)(void*), void* consts){
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_RR);
//                pthread_attr_setstacksize(&attr, 16384);
		int success;
	
		int i=_loop_threads.size();
			
		_loop_threads.resize(i+1);
		_loop_threads[i]._commit=false;
		success=pthread_create(&_loop_threads[i]._thread, &attr, f, consts);
		if (success!=0){
			_thread_index.insert(pair<pthread_t, int>(_loop_threads[i]._thread, -1));
			return -1;
		}
		_thread_index.insert(pair<pthread_t, int>(_loop_threads[i]._thread, i));
		return 0;
	};	



	//!
        //! run: a complete function that takes orderly the instructions and arguments
        //! of n loop iterations from a window; and starts their parallel execution, while 
	//! maintaining the sequential consistency with an un-managed pre-loop section.
	//! This function will return in the shared data, the results of the computation, when
	//! get_results is called.
	//!
	//! TO BE NOTED:
	//!    * In this version of the function, the object is not in control of the pre-
	//!    loop section. This means that on invocation, the caller only blocks for the creation
	//!    of the threads, and can resume it's execution as a possible pre-loop, until
	//!    calling get_results(), when the caller blocks until all valid loop iterations/threads return.
	//!
	int run (script_vector thread_instructions, vector <void*> const_args){

		_is_active=true; 
	
		pthread_mutex_lock(&_loop_threads_mutex);
	
		vector <int> threads_to_cancel;//a selective reset is performed, waiting for the threads in
		_cancel_loop_threads_and_reset_logs(threads_to_cancel);//deferred cancel to finish, and reseting the
		//arrays that the execution uses.

		_has_pre_loop=false; 

		pthread_mutex_lock(&_pl_mutex);
		_pl_commit=false;
		pthread_mutex_unlock(&_pl_mutex);
	
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_RR);
//                pthread_attr_setstacksize(&attr, 16384);

		//the threads are created
		int success;
		_loop_threads.resize(thread_instructions.size());		
		
		for (unsigned int i=0; i<thread_instructions.size(); i++){
			pthread_mutex_lock(&_loop_threads[i]._thread_mutex);
		}
		for (unsigned int i=0; i<thread_instructions.size(); i++){

			success=pthread_create (&_loop_threads[i]._thread, &attr, thread_instructions[i], const_args[i]);
			
			if (success==0){
				_thread_index.insert(pair<pthread_t, int>(_loop_threads[i]._thread, i));
			}

			pthread_mutex_unlock(&_loop_threads[i]._thread_mutex);

			if (success!=0){
				for (unsigned int j=i+1; j<thread_instructions.size(); j++){
					pthread_mutex_unlock(&_loop_threads[j]._thread_mutex);
				}
				for (unsigned int j=0; j<i; j++){
					threads_to_cancel.push_back(j);
				}
				_cancel_loop_threads_and_reset_logs(threads_to_cancel);
					
				pthread_mutex_unlock(&_loop_threads_mutex);

				pthread_mutex_lock(&_is_active_mutex);
				_is_active=false;
				pthread_mutex_unlock(&_is_active_mutex);
				return -1;// a thread could not be created.
			}
		}
		pthread_mutex_unlock(&_loop_threads_mutex);
		return 0;
	};

	//!
        //! function to get the results of the execution once the un-managed pre-
        //! loop section has ended it's execution. 
        //! 
	//! TO BE NOTED: If some join is not possible, no check is made.
        //! 
	int get_results(){
		bool commit_made=false;
		unsigned int joined_but_didnt_commit=0;
		int success;
		do{
			success=-1;
			if (success!=0){
				pthread_mutex_lock(&_pl_mutex);
				if (_pl_commit){
					commit_made=true;
				}
				else{
					if (joined_but_didnt_commit<10){ 
						success=0;
						joined_but_didnt_commit++;
					}
				}
				pthread_mutex_unlock(&_pl_mutex); 
			}
			
		} while (success==0);

		int i=0; 

		while (_loop_threads.size()>i){

			pthread_t thread_to_join=_loop_threads[i]._thread;

			commit_made=false;
			joined_but_didnt_commit=0;
	
			do{
			
				success=pthread_join (thread_to_join, NULL);


			} while (success==0);	
			i++;
		}

		pthread_mutex_lock(&_is_active_mutex);
		_is_active=false;
		pthread_mutex_unlock(&_is_active_mutex);
		return 0;
	};

 	int cancel_iteration (int a){
		if (_loop_threads[a]._commit)
			return 0;
                _loop_threads[a]._commit=true;
//		if (pthread_kill(_loop_threads[a]._thread, 0)==0){
			pthread_cancel(_loop_threads[a]._thread);
			_thread_index[_loop_threads[a]._thread]= -1;
//		}
		return 0;
	}
	
};
