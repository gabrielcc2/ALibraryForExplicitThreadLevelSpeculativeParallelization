#include <pthread.h>
#include <iostream>
#include <map>
#include <vector>
#include <string.h>
#include <signal.h>
#include <set>
using namespace std;

//!
//! \class  multibranch_speculator
//!
//! \brief  implements a class that takes n conditional branches and 
//!         executes them speculatively, until one of them is
//!         proven valid in the pre-branch section. In order to do this 
//!         the class relies on write_data and read_data functions, that
//!	    check for data dependence violations; and 3 other functions
//!         (validate_supposition, speculate and get_results), 
//!         that are in charge of control dependence. 
//!         Additional functionality is provided with an append function 
//!         that allows the pre-branch to dynamically add new branches 
//!         to an on-going speculative execution.
//!
//! Limitations: 
//! * If a speculative thread is to be canceled, it cannot use functions
//! that involve system mutexes, such as printf, etc. In this case, it 
//! is possible that the thread can be canceled while holding such a mutex, 
//! and the application con go into deadlock. In order to prevent this the
//! user has to surround this "dangerous" code with: 
//!	"pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);" and
//!     "pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);".
//!
//! * Sequential consistency is mostly guaranteed, save for exception 
//! behaviour.
//!

class multibranch_speculator : protected _conditional_speculator
{

private: 

	//position of the valid thread, -1 if none has been validated.
	int _valid_branch;

	//parallel sections of the class
	_previousSection _pb;
	vector <_speculativeBranch> _spec_threads; //the speculative threads and it's data

	//red-black tree, used as a thread index to relate a pthread_id with 
	//it's model id, i.e. it's position in _spec_threads. -1 is used to
	//indicate a thread in deferred cancel
	map <pthread_t, int> _thread_index; 

	//mutex for a synchronized access to both of the former
	pthread_mutex_t _spec_threads_mutex; 


	//red-black tree that relates the reference of a data element from
	//shared_data data with a vector of type int used to keep track of all the
	//readers of that particular data element.
	map <void*, _single_data_readers_log> _readers_log;

	//global mutex related to the _readers_log as a whole
//	pthread_mutex_t _global_readers_log_mutex;
	
	//!
        //! private method to reset or cancel speculative threads
        //!
        //! TO BE NOTED:
        //! * this method takes all class mutexes. On invocation
	//!   _spec_threads_mutex and _global_readers_log_mutex should 
	//!    be on hold and no other class mutex, save those related 
	//!   to a branch that is not about to be canceled or restarted.
	//! * If given reset_all_logs=false && cancel=true, it can be used
	//!   to cancel all branches save the valid one.
	//! * If given reset_all_logs=false && cancel=false, it can be used
	//!   to reinitialize delinquent branches after a detected RAW violation.
	//! * If given reset_all_logs=true && cancel=true and a vector
	//!   threads_to_delete of size 0, the object is initialized
	//!   only deleting the valid_branch from the previous run, if
	//!   it exists.
	//!   
	void _reset_spec_threads (vector <int> threads_to_delete, bool reset_all_logs, bool cancel){ 

		int i;
		bool valid_arguments=true;

		if (!threads_to_delete.empty()){

			for (i=0; i<threads_to_delete.size(); i++){

				if (threads_to_delete[i]<0 || threads_to_delete[i]>=_spec_threads.size()){
					valid_arguments=false; //one thread has an invalid id.
					i=threads_to_delete.size();
				}
					
			}
		}
		else{ //no threads to delete

			/*This option will be used to initialize the object, it only cancels the valid thread from the previous run
			and resets the object's arrays*/

			valid_arguments=false; 

			if (reset_all_logs && cancel){  //all the readers log will be deleted

				pthread_mutex_lock(&_valid_branch_mutex);
				int valid_branch=_valid_branch;
				pthread_mutex_unlock(&_valid_branch_mutex);
							
				if (valid_branch>=0 && valid_branch<_spec_threads.size())				
					pthread_mutex_lock(&_spec_threads[valid_branch]._mutex);

				pthread_mutex_lock(&_is_running_mutex);

				pthread_mutex_lock(&_valid_branch_mutex);

				if (!_pb._isExternal)
	
					pthread_mutex_lock(&_pb._mutex);

				if (!_spec_threads.empty()){
					if (valid_branch>=0 && valid_branch<_spec_threads.size()){				
						if (pthread_kill(_spec_threads[valid_branch]._thread, 0)==0)
							pthread_cancel(_spec_threads[valid_branch]._thread); 
						_thread_index[_spec_threads[valid_branch]._thread]= -1;
						pthread_mutex_unlock(&_spec_threads[valid_branch]._mutex);
//						pthread_mutex_destroy(&_spec_threads[valid_branch]._mutex);
					}
					_spec_threads.clear(); 
				}				

				if (!_readers_log.empty()){
					_readers_log.clear();
				}

				if (!_thread_index.empty()){ 

				/*in order to clear the _thread_index, it will be necessary to guarantee that all the threads in deferred
				cancel have finished, and thus will not use the object*/

					if (!_pb._isExternal)
			
						pthread_mutex_unlock(&_pb._mutex);
		
					pthread_mutex_unlock(&_valid_branch_mutex);
	
					pthread_mutex_unlock(&_is_running_mutex);

					pthread_mutex_unlock(&_spec_threads_mutex);
					
//					pthread_mutex_unlock(&_global_readers_log_mutex);

					map <pthread_t, int>::iterator it;
					for (it=_thread_index.begin(); it!=_thread_index.end(); it++){
						if (it->second==-1){
							int a;
							//do {
								a= pthread_kill(it->first, 0);
							//} while (a==0);
						}
					}
				
//					pthread_mutex_lock(&_global_readers_log_mutex);

					pthread_mutex_lock(&_spec_threads_mutex);

					_thread_index.clear();
				}
				else {
					if (!_pb._isExternal)
			
						pthread_mutex_unlock(&_pb._mutex);
	
					pthread_mutex_unlock(&_valid_branch_mutex);
	
					pthread_mutex_unlock(&_is_running_mutex);

				}
			
			}
		}

		if (valid_arguments) {

			for (i=threads_to_delete.size()-1; i>=0; i--){
				cout<<"About to lock with i="<<i<<" "<<threads_to_delete[i]<<" Size: "<<threads_to_delete.size()<<endl;

				pthread_mutex_lock(&_spec_threads[threads_to_delete[i]]._mutex); 
				cout<<"Pasa.. "<<i<<" "<<threads_to_delete[i]<<endl;
			}


			if (reset_all_logs && cancel){  //all the readers logs will be deleted, as well as all the threads

				pthread_mutex_lock(&_is_running_mutex);

				pthread_mutex_lock(&_valid_branch_mutex);

				if (!_pb._isExternal)

					pthread_mutex_lock(&_pb._mutex);

				for (i=0; i<threads_to_delete.size(); i++){

					if (pthread_kill(_spec_threads[threads_to_delete[i]]._thread, 0)==0)
	
						pthread_cancel(_spec_threads[threads_to_delete[i]]._thread);

					_thread_index[_spec_threads[threads_to_delete[i]]._thread]= -1;
					
//					pthread_mutex_destroy(&_spec_threads[threads_to_delete[i]]._mutex); 
				}


				if (!_pb._isExternal)
					pthread_mutex_unlock(&_pb._mutex);

				pthread_mutex_unlock(&_valid_branch_mutex);

				pthread_mutex_unlock(&_is_running_mutex);

				for (i=0; i<=threads_to_delete.size()-1; i++){

					pthread_mutex_unlock(&_spec_threads[threads_to_delete[i]]._mutex); 
				}

			}			

			else {

					pthread_mutex_lock(&_is_running_mutex);
	
					pthread_mutex_lock(&_valid_branch_mutex);

					if (!_pb._isExternal)
	
						pthread_mutex_lock(&_pb._mutex);

					for (i=0; i<threads_to_delete.size(); i++){
					
						if (pthread_kill(_spec_threads[threads_to_delete[i]]._thread, 0)==0){
							pthread_cancel(_spec_threads[threads_to_delete[i]]._thread);
							
						}
						_thread_index[_spec_threads[threads_to_delete[i]]._thread]= -1;

					}


				//next it will be checked what data has been read by the threads_to_delete.

				set <void*> data_to_access; 
				map <void*, _data_copy*>::iterator k;

				for (i=threads_to_delete.size()-1; i>=0; i--){

					if (!_spec_threads[threads_to_delete[i]]._read_data.empty()){

						set <void*>::iterator it;

						for (it=_spec_threads[threads_to_delete[i]]._read_data.begin(); it!=_spec_threads[threads_to_delete[i]]._read_data.end(); it++){
							if (data_to_access.find(*it)==data_to_access.end()){

								data_to_access.insert(*it);
							}
						}
						_spec_threads[threads_to_delete[i]]._read_data.clear();						
					}
				
					if (!_spec_threads[threads_to_delete[i]]._copied_data.empty()){
						for (k=_spec_threads[threads_to_delete[i]]._copied_data.begin(); k!=_spec_threads[threads_to_delete[i]]._copied_data.end(); k++){
							free((void*)k->second->_data);
							free ((void*)(k->second));
							(*k).second=NULL;
						}
						_spec_threads[threads_to_delete[i]]._copied_data.clear();
					}					
				}

				if (data_to_access.empty()){

					if (!cancel) { //this means that the threads will be restarted

						pthread_attr_t attr;
						pthread_attr_init (&attr);
						pthread_attr_setschedpolicy(&attr, _sched_option);
						
						for (i=0; i<threads_to_delete.size(); i++){
							int success=0;
							//do{
							success=pthread_create(&_spec_threads[threads_to_delete[i]]._thread, &attr, _spec_threads[threads_to_delete[i]]._instructions, _spec_threads[threads_to_delete[i]]._args);
							//	cout<<"inf2..."<<endl;
							//} while (success!=0);
							_thread_index[_spec_threads[threads_to_delete[i]]._thread]= threads_to_delete[i];
						}
					}

					if (!_pb._isExternal)

						pthread_mutex_unlock(&_pb._mutex);

					pthread_mutex_unlock(&_valid_branch_mutex);

					pthread_mutex_unlock(&_is_running_mutex);
				}
				else{
						
					if (!reset_all_logs){ //Only the logs of the threads

						set <void*>::iterator it;

						for (it=data_to_access.begin(); it!=data_to_access.end(); it++){	

							if (_readers_log.find(*it)!=_readers_log.end()){

								_single_data_readers_log * ptr= &_readers_log.find(*it)->second;

								pthread_mutex_lock (&ptr->_mutex);
								
								if (!ptr->_readers.empty()){

									ptr->_readers.clear();
								}

								pthread_mutex_unlock (&ptr->_mutex);


							}
						}
					}
					else{ //only the threads to delete will be deleted from the common readers logs
				
						set <void*>::iterator it;

						for (it=data_to_access.begin(); it!=data_to_access.end(); it++){

	
							if (_readers_log.find(*it)!=_readers_log.end()){
	
								_single_data_readers_log * ptr= &_readers_log.find(*it)->second;
			
								pthread_mutex_lock (&ptr->_mutex);
															
								if (!ptr->_readers.empty()){

									for (i=0; i<threads_to_delete.size(); i++){

										vector<int> toErase;

										for (int j=0; j<ptr->_readers.size(); j++){
											if (ptr->_readers[j]==threads_to_delete[i]){												toErase.push_back(j);
											}
										}
										if (!toErase.empty()){
											int subs=0;
											for (int k=0; k<toErase.size(); k++){
												ptr->_readers.erase(ptr->_readers.begin()+k-subs);
												subs++;
											}
										}
									}
								}

								pthread_mutex_unlock (&ptr->_mutex);
							}
						}
					}

					if (!cancel) { //this means that the threads will be restarted
						pthread_attr_t attr;
						pthread_attr_init (&attr);
						pthread_attr_setschedpolicy(&attr, _sched_option);
						
						for (i=0; i<threads_to_delete.size(); i++){
							int success=0;
							//do {
							success=pthread_create(&_spec_threads[threads_to_delete[i]]._thread, &attr, _spec_threads[threads_to_delete[i]]._instructions, _spec_threads[threads_to_delete[i]]._args);
							//								cout<<"inf...3"<<endl;
//							} while (success!=0);
							_thread_index[_spec_threads[threads_to_delete[i]]._thread]=threads_to_delete[i];
						}
					}

					if (!_pb._isExternal)
	
						pthread_mutex_unlock(&_pb._mutex);

					pthread_mutex_unlock(&_valid_branch_mutex);

					pthread_mutex_unlock(&_is_running_mutex);

				}						
			}
		}
		if ( valid_arguments && !(reset_all_logs&&cancel)){

			for (i=0; i<threads_to_delete.size(); i++){
				pthread_mutex_unlock(&_spec_threads[threads_to_delete[i]]._mutex);
			}
		}
	};	

public: 


	//!
        //! default constructor
        //!
	multibranch_speculator(){
		_is_running=false;
//		pthread_mutex_init (&_global_readers_log_mutex, NULL); 
		pthread_mutex_init (&_spec_threads_mutex, NULL); 
		_valid_branch=-1;//no branch has been validated
	};


	//!
        //! function providing access to the shared_data as a whole
        //! 
	void*& get_shared_data (){ 

		pthread_mutex_lock(&_is_running_mutex);

		bool manages_pre_branch=!_pb._isExternal;
		if (!_is_running){

			pthread_mutex_unlock(&_is_running_mutex);

			return (void*&)_null_data; //the object is inactive

		}

		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_spec_threads_mutex);     //LAS OTRAS HEBRAS SE TRANCAN AQUI!!!

		if (_thread_index.empty() ||_thread_index.find(pthread_self())!=_thread_index.end()){
	
			pthread_mutex_unlock(&_spec_threads_mutex);
	
			return _shared_data; //valid branch or branch in deferred cancelation
		}
		pthread_mutex_unlock(&_spec_threads_mutex);

		if (manages_pre_branch){

			pthread_mutex_lock(&_pb._mutex);

			if (pthread_self()==_pb._thread){

				pthread_mutex_unlock(&_pb._mutex);
				return _shared_data;
			}
			
			else {
				pthread_mutex_unlock(&_pb._mutex);

				return (void*&)_null_data; //invalid caller
			}
		}


		return _shared_data; //unmanaged pre-branch or branch in deferred cancelation
	};


	//!
        //! function allowing to validate one of the threads of the model
        //! according to it's position
	//!
        //! TO BE NOTED:
        //! * has to be called from the pre-branch section
        //! 
	int validate_supposition (int thread_to_validate){ 
		pthread_mutex_lock(&_is_running_mutex);

		if (!_is_running){
			pthread_mutex_unlock(&_is_running_mutex);
			return -1; //the object is inactive
		}

		bool manages_pre_branch=!_pb._isExternal;

		pthread_mutex_unlock(&_is_running_mutex);

		bool is_pre_branch=false;

		pthread_mutex_lock(&_valid_branch_mutex);
		if (_valid_branch!=-1){
			pthread_mutex_unlock(&_valid_branch_mutex);
			return -1;
		}
		pthread_mutex_unlock(&_valid_branch_mutex);


		if (manages_pre_branch){

			pthread_mutex_lock(&_pb._mutex);

			if (pthread_self()!=_pb._thread){
				pthread_mutex_unlock(&_pb._mutex);
				return -1; //invalid caller
			}
			pthread_mutex_unlock(&_pb._mutex);
			is_pre_branch=true;
		}


		pthread_mutex_lock(&_spec_threads_mutex);

		if (!is_pre_branch){

			if (thread_to_validate<0){
					pthread_mutex_unlock(&_spec_threads_mutex);
					return -1; //invalid argument
			}
			if (_thread_index.find(pthread_self())!=_thread_index.end()){
					pthread_mutex_unlock(&_spec_threads_mutex);
					return -1;//invalid caller
			}
		}


		pthread_mutex_lock(&_valid_branch_mutex);
		_valid_branch=thread_to_validate;
		pthread_mutex_unlock(&_valid_branch_mutex);

		if (thread_to_validate<_spec_threads.size()){
			vector <int> threads_to_delete;
//			for (int i=0; i<_spec_threads.size(); i++){
			for (int i=_spec_threads.size()-1; i>=0; i--){
				if (i!=thread_to_validate)
					threads_to_delete.push_back(i);
			}

		//	cout<<"llega hasta aca..."<<thread_to_validate<<endl;

			
			if (!threads_to_delete.empty()){
				//the threads that were invalidated are deleted
				_reset_spec_threads(threads_to_delete, false, true); 
			}
		}

	//	cout<<"sale sin problemas..."<<endl;
		pthread_mutex_unlock(&_spec_threads_mutex);
		return 0;

	};

	//!
        //! function to be called from the speculative branches
        //! and pre-branch, in order to read the shared_data
	//! while keeping the expected sequential data consistency
        //! 
	void* read_data(void* data_to_be_read, unsigned int size){ 


		pthread_mutex_lock(&_is_running_mutex);
		if (!_is_running){
			pthread_mutex_unlock(&_is_running_mutex);
			return data_to_be_read; //the object is inactive, it's calling data is returned instead 
					             //of NULL, to prevent segmentation faults.
		}
		bool manages_pre_branch=!_pb._isExternal;
		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_valid_branch_mutex);
		int valid_branch=_valid_branch;
		pthread_mutex_unlock(&_valid_branch_mutex);


		void* retval;
		bool thrd_id_equals_pb=false;


		if (manages_pre_branch){

			pthread_mutex_lock(&_pb._mutex);

			thrd_id_equals_pb=(pthread_self()==_pb._thread);

			pthread_mutex_unlock(&_pb._mutex);

		}

		if (thrd_id_equals_pb){
			return data_to_be_read;//the pre-branch does a standard read
		}

		pthread_mutex_lock(&_spec_threads_mutex);

		pthread_t thrd_id=pthread_self();

		//the branches need to make a copy or read an already existing copy, and log the reading.

		int current_thread=-1;

		if (_thread_index.empty()){
				pthread_mutex_unlock(&_spec_threads_mutex);
				return data_to_be_read;

		}
		if (_thread_index.find(thrd_id)!=_thread_index.end()){
			current_thread=_thread_index.find(thrd_id)->second;
		}
		if (current_thread>=0 && (valid_branch==-1||(valid_branch==current_thread))){

			_speculativeBranch* thrd_ptr=&_spec_threads[current_thread];

			pthread_mutex_lock(&thrd_ptr->_mutex);

			map <void*, _data_copy*>::iterator it;

			it=thrd_ptr->_copied_data.find((void*)data_to_be_read);

			if (it!=thrd_ptr->_copied_data.end()){
					retval=(void*)malloc (size);
					memcpy (retval, it->second->_data, size);
					pthread_mutex_unlock(&thrd_ptr->_mutex);
					pthread_mutex_unlock(&_spec_threads_mutex);
					return retval;//if a copy is found, the copy is read
			}
			
			if (thrd_ptr->_read_data.find((void*)data_to_be_read)!=thrd_ptr->_read_data.end()){
					pthread_mutex_unlock(&thrd_ptr->_mutex);
	
					pthread_mutex_unlock(&_spec_threads_mutex);

					return data_to_be_read;//if the data has already been read, then the reading neet not be logged.
			}

			thrd_ptr->_read_data.insert((void*)data_to_be_read);//the read is logged in the thread

//			pthread_mutex_lock (&_global_readers_log_mutex);
		
			if (_readers_log.find((void*)data_to_be_read)!=_readers_log.end()){

					_single_data_readers_log * ptr=	&_readers_log.find((void*)data_to_be_read)->second;

					pthread_mutex_lock(&ptr->_mutex);

//					pthread_mutex_unlock(&_global_readers_log_mutex);

					ptr->_readers.push_back(current_thread);//the read is marked in the data

					pthread_mutex_unlock(&ptr->_mutex);

					pthread_mutex_unlock(&thrd_ptr->_mutex);

					pthread_mutex_unlock(&_spec_threads_mutex);

					return data_to_be_read;
			}
			else{// the data log has to be created
				pthread_mutex_t temp;
				pthread_mutex_init (&temp, NULL); 
				_single_data_readers_log temp_log;
				temp_log._readers.push_back(current_thread);		
				_readers_log.insert(pair<void*, _single_data_readers_log>((void*)data_to_be_read, temp_log));

//				pthread_mutex_unlock(&_global_readers_log_mutex);

				pthread_mutex_unlock(&thrd_ptr->_mutex);
	
				pthread_mutex_unlock(&_spec_threads_mutex);		

				return data_to_be_read;
			}
		}
		if (!manages_pre_branch){
			pthread_mutex_unlock(&_spec_threads_mutex);
			return data_to_be_read;//un-managed pre-branch
		}

		pthread_mutex_unlock(&_spec_threads_mutex);
		return data_to_be_read; //invalid caller or branch in deferred cancelation. 
	   				     //it's calling data is returned instead of NULL, to prevent segmentation faults.
	};


	//!
        //! function to be called from the speculative branches
        //! and pre-branch, in order to write the shared_data
	//! while keeping the expected sequential data consistency
        //! 
	int write_data(void*& data_to_be_written_upon, void* data_to_write, unsigned int size){
		
		pthread_mutex_lock(&_is_running_mutex);
		if (!_is_running){
			pthread_mutex_unlock(&_is_running_mutex);
			return -1; //the object is inactive. 
		}
		bool manages_pre_branch=!_pb._isExternal;
		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_valid_branch_mutex);
		int valid_branch=_valid_branch;
		pthread_mutex_unlock(&_valid_branch_mutex);
		
		bool thrd_id_equals_pb=false;
	
		if (manages_pre_branch){
			pthread_mutex_lock(&_pb._mutex);
			thrd_id_equals_pb=(pthread_self()==_pb._thread);
			pthread_mutex_unlock(&_pb._mutex);
		}


		pthread_mutex_lock(&_spec_threads_mutex);

		pthread_t thrd_id=pthread_self();

		bool is_pre_branch=false||thrd_id_equals_pb;
				
		int current_thread=-2;
		
		if (_thread_index.empty()){
			pthread_mutex_unlock(&_spec_threads_mutex);
			return -1;
		}

		if (_thread_index.find(thrd_id)!=_thread_index.end()){
			current_thread=_thread_index.find(thrd_id)->second;
		}

		if (current_thread==-2 && !manages_pre_branch){
			is_pre_branch=true;
		}
	
		if (is_pre_branch){

//			pthread_mutex_lock (&_global_readers_log_mutex);

			if (_readers_log.find((void*)data_to_be_written_upon)==_readers_log.end()){ 

				pthread_mutex_unlock(&_spec_threads_mutex);
				
				memcpy ((void*)data_to_be_written_upon, (void*)&data_to_write, size);

//				pthread_mutex_unlock(&_global_readers_log_mutex);

				return 0;//if no log exists for the data, then it is simply written.
			}

			//Otherwise the data is written, then data dependency checks are preformed, in search of a true
			//dependence violation (RAW).

			memcpy ((void*)data_to_be_written_upon, (void*)&data_to_write, size);

			_single_data_readers_log *ptr=&_readers_log.find((void*)data_to_be_written_upon)->second;
			
			pthread_mutex_lock(&ptr->_mutex);

//			pthread_mutex_unlock (&_global_readers_log_mutex);			

			vector <int> delinquent_readers= ptr->_readers;

			pthread_mutex_unlock(&ptr->_mutex);

			if (!delinquent_readers.empty()){//a true data dependency violation has ocurred (RAW).
			
				_reset_spec_threads(delinquent_readers, false, false);
			}
			pthread_mutex_unlock(&_spec_threads_mutex);

			return 0;
		}
		if (current_thread>=0 && (_valid_branch==-1||(_valid_branch==current_thread))){

			_speculativeBranch* thrd_ptr=&_spec_threads[current_thread];

			pthread_mutex_lock(&thrd_ptr->_mutex);
		//	pthread_mutex_unlock(&_spec_threads_mutex);

			//if the thread has a copy, it is written
			if (!thrd_ptr->_copied_data.empty()){
				if(thrd_ptr->_copied_data.find((void*)data_to_be_written_upon)!=thrd_ptr->_copied_data.end()){
					thrd_ptr->_copied_data[(void*)data_to_be_written_upon]->_size=size;
					memcpy (thrd_ptr->_copied_data[(void*)data_to_be_written_upon]->_data, (void*)&data_to_write, size);
					pthread_mutex_unlock(&thrd_ptr->_mutex);
					pthread_mutex_unlock(&_spec_threads_mutex);
					return 0;
				}
			}
			//if there is no per-thread copy, it is made and written.
			_data_copy * copy;
			copy= (struct _data_copy*) malloc (sizeof(struct _data_copy));
			copy->_size=size;
			copy->_data= (void*) malloc (size);
			memcpy (copy->_data, (void*)&data_to_write, size);
			thrd_ptr->_copied_data.insert(pair<void*, _data_copy*>((void*)data_to_be_written_upon, copy)); 
			pthread_mutex_unlock(&thrd_ptr->_mutex);
			pthread_mutex_unlock(&_spec_threads_mutex);
			return 0;
		}
		pthread_mutex_unlock(&_spec_threads_mutex);
		return -1;//invalid caller or branch in deferred cancelation.
	};

	//!
        //! function that permits the pre_branch to dynamically append
        //! a new speculative thread at the end of the array in an on-going
	//! speculative execution.
        //! 
	int append(void* (f)(void*), void* consts){

		pthread_mutex_lock(&_is_running_mutex);
		if (!_is_running){ 
			pthread_mutex_unlock(&_is_running_mutex);
			return -1; //the object is not running.
		}
		bool manages_pre_branch=!_pb._isExternal;
		pthread_mutex_unlock(&_is_running_mutex);

		bool is_pre_branch=false;
		if (manages_pre_branch){
			pthread_mutex_lock(&_pb._mutex);
			if (pthread_self()==_pb._thread){
				is_pre_branch=true;
			}
			pthread_mutex_unlock(&_pb._mutex);
		}

		int current_thread=-2;

		int valid_code;

		pthread_mutex_lock(&_valid_branch_mutex);
		valid_code=_valid_branch;
		pthread_mutex_unlock(&_valid_branch_mutex);

		pthread_mutex_lock(&_spec_threads_mutex);
		if (valid_code!=-1 && valid_code<_spec_threads.size()){
			pthread_mutex_unlock(&_spec_threads_mutex);
			return -1;
		}
		pthread_t thrd_id=pthread_self();
		if (_thread_index.find(thrd_id)!=_thread_index.end()){
			current_thread=_thread_index.find(thrd_id)->second;
		}
			
		if (current_thread==-2 && !manages_pre_branch){
			is_pre_branch=true;
		}
		if (is_pre_branch){
			pthread_attr_t attr;
			pthread_attr_init (&attr);
			pthread_attr_setschedpolicy(&attr, _sched_option);
			int success=0;
			_spec_threads.resize(_spec_threads.size()+1);
			pthread_mutex_lock(&_spec_threads[_spec_threads.size()-1]._mutex);
			pthread_mutex_lock(&_is_running_mutex);
			pthread_mutex_lock(&_valid_branch_mutex);
			if (manages_pre_branch)
				pthread_mutex_lock(&_pb._mutex);
		//	do {
			success=pthread_create(&_spec_threads[_spec_threads.size()-1]._thread, &attr, f, consts);
//								cout<<"inf4..."<<endl;
		//	} while (success!=0);
			_thread_index[_spec_threads[_spec_threads.size()-1]._thread]=_spec_threads.size()-1;
			if (manages_pre_branch)
				pthread_mutex_unlock(&_pb._mutex);
			pthread_mutex_unlock(&_valid_branch_mutex);
			pthread_mutex_unlock(&_is_running_mutex);
			pthread_mutex_unlock(&_spec_threads[_spec_threads.size()-1]._mutex);
			pthread_mutex_unlock(&_spec_threads_mutex);

			return 0;

		}
		pthread_mutex_unlock(&_spec_threads_mutex);

		return -1;//invalid caller.
	};	


	//!
        //! speculate: a complete function that takes the instructions and arguments
        //! of the branches and pre-branch; and starts their speculatively parallel 
	//! execution, while maintaining the sequential consistency. This function 
        //! returns in the &shared_data, the results of the computation.
	//!
	//! TO BE NOTED:
	//!    * In this version of the function, the object is in control of the pre-
	//!    branch. This means that on invocation, the caller blocks until the pre-
	//!    branch and validated thread complete their execution.
	//!   
	int speculate (void*& shared_data, void* (fpb)(void*), void* const_args_pb, script_vector thread_instructions, vector <void*> const_args_spec_threads, int sched_policy){

		pthread_mutex_lock(&_is_running_mutex);
		if (_is_running){ 
			pthread_mutex_unlock(&_is_running_mutex);
			return -1;//the object is already busy running, hence no other speculation can start.
		}
		if (thread_instructions.size()!=const_args_spec_threads.size()){ 
			pthread_mutex_unlock(&_is_running_mutex);
			return -1;//invalid arguments
		}
		_is_running=true; 
		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_spec_threads_mutex);
//		pthread_mutex_lock(&_global_readers_log_mutex);
		
		vector <int> threads_to_cancel;//a selective reset is performed, canceling only 
		_reset_spec_threads(threads_to_cancel, true, true);//the valid thread from the previous run, if any.

		pthread_mutex_lock(&_is_running_mutex);
		_pb._isExternal=false; 
		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_valid_branch_mutex);
		_valid_branch=-1;
		pthread_mutex_unlock(&_valid_branch_mutex);
	
		_shared_data=shared_data;
	
		pthread_attr_t attr;
		pthread_attr_init (&attr);
		

		if (sched_policy== SCHED_FIFO||sched_policy==SCHED_RR||sched_policy== SCHED_OTHER){
			_sched_option=sched_policy;
		}
		else {
			_sched_option=SCHED_RR;
		}
		pthread_attr_setschedpolicy(&attr, _sched_option);


		int success=0;	

		pthread_mutex_lock(&_pb._mutex);
		success=pthread_create (&_pb._thread, &attr, fpb, const_args_pb);
		pthread_mutex_unlock(&_pb._mutex);

		if (success!=0){


//				pthread_mutex_unlock(&_global_readers_log_mutex);
				pthread_mutex_unlock(&_spec_threads_mutex);

				pthread_mutex_lock(&_is_running_mutex);
				_is_running=false;
				pthread_mutex_unlock(&_is_running_mutex);
		
				return -1;//the pre-branch could not be created.
		}

		_spec_threads.resize(thread_instructions.size());

		for (int i=thread_instructions.size()-1; i>=0; i--){
			pthread_mutex_lock(&_spec_threads[i]._mutex);
		}
		for (unsigned int i=0; i<thread_instructions.size(); i++){
			_spec_threads[i]._instructions=thread_instructions[i];
			_spec_threads[i]._args=const_args_spec_threads[i];
			success=pthread_create (&_spec_threads[i]._thread, &attr, 
					        _spec_threads[i]._instructions, 
						_spec_threads[i]._args);
			_thread_index[_spec_threads[i]._thread]= i;
			pthread_mutex_unlock(&_spec_threads[i]._mutex);
			if (success!=0){
				for (int j=i; j<thread_instructions.size(); j++){
					pthread_mutex_unlock(&_spec_threads[j]._mutex);
				}
				vector <int> threads_to_delete;
				for (int j=0; j>i; j++){
					threads_to_delete.push_back(j);
				}
				_reset_spec_threads(threads_to_delete, true, true);
//				pthread_mutex_unlock(&_global_readers_log_mutex);
				pthread_mutex_unlock(&_spec_threads_mutex);

				pthread_mutex_lock(&_is_running_mutex);
				_is_running=false;
				pthread_mutex_unlock(&_is_running_mutex);
	
				return -1;//one thread could not be created.
			}
		}
		
//		pthread_mutex_unlock(&_global_readers_log_mutex);
		pthread_mutex_unlock(&_spec_threads_mutex);

		int valid_branch;
		int joins_but_no_validation=0;

//		do{

			success=pthread_join (_pb._thread, NULL);
		
			if (success!=0){
				pthread_mutex_lock(&_valid_branch_mutex);
				if (_valid_branch==-1){
					success=0;
					joins_but_no_validation++;
				}
				else{
					valid_branch=_valid_branch;
				}
				if (joins_but_no_validation>10){ 

				/*just in case the validated supposition is not logged in a timely fasion, it is tried
				10 times to see if it get's validated, if not then the object asumes there was no validated 
				branch. 
				Although slightly un-elegant, the validity of this solution has not been disproved by empirical 
				tests.*/

					success=-1;  
					valid_branch=-1; 
				}
				pthread_mutex_unlock(&_valid_branch_mutex); 
			}
			

//								cout<<"inf5..."<<endl;
//		} while (success==0);


		pthread_mutex_lock(&_spec_threads_mutex);

		if (valid_branch>=_spec_threads.size())
			valid_branch=-1;
	
		if (valid_branch==-1){
//			cout<<"No validation..."<<endl;
			vector <int> threads_to_delete;
			for (int i=0; i<_spec_threads.size(); i++){
				threads_to_delete.push_back(i);
			}
			_reset_spec_threads(threads_to_delete, true, true);
			pthread_mutex_unlock(&_spec_threads_mutex);

			pthread_mutex_lock(&_is_running_mutex);
			_is_running=false;
			pthread_mutex_unlock(&_is_running_mutex);

			return -1;//no speculative thread was validated, thus all are canceled save the pre-branch.
		}

		pthread_mutex_unlock(&_spec_threads_mutex);

		//do{
			success=pthread_join (_spec_threads[valid_branch]._thread, NULL);
//								cout<<"inf6..."<<endl;
//		}while (success==0);

		pthread_mutex_lock(&_spec_threads_mutex);
		pthread_mutex_lock(&_spec_threads[valid_branch]._mutex);

		if (!_spec_threads[valid_branch]._copied_data.empty()){ 
			//the changes that the valid thread made are communicated unto the &_shared_data
			map <void*, _data_copy*>::iterator i; 
			for (i=_spec_threads[valid_branch]._copied_data.begin(); i!=_spec_threads[valid_branch]._copied_data.end(); i++){
				memcpy((void*&)const_cast<void*&>(i->first), (void*)(i->second->_data), i->second->_size);
				free ((void*)(i->second->_data));
				free ((void*)(i->second));
				(*i).second=NULL;
			}								
			_spec_threads[valid_branch]._copied_data.clear();									
		}
		pthread_mutex_unlock(&_spec_threads[valid_branch]._mutex);

		pthread_mutex_unlock(&_spec_threads_mutex);

		pthread_mutex_lock(&_is_running_mutex);
		_is_running=false;
		pthread_mutex_unlock(&_is_running_mutex);

		return 0;

	};



	//!
        //! speculate: a function that takes the instructions and arguments
        //! of the branches , and starts their speculatively parallel execution, 
        //! while maintaining the sequential consistency with an unmanaged pre-branch. 
	//!
	//! TO BE NOTED:
	//!    * In this version of the function, the object is not in control of the pre-
	//!    branch. This means that on invocation, the caller only blocks for the creation
	//!    of the branches, and can resume it's execution as a possible pre-branch, until
	//!    calling get_results(), when the caller blocks until the valid branch returns.
	//!
	int speculate (void*& shared_data, script_vector thread_instructions, vector <void*> const_args_spec_threads, int sched_policy){
		pthread_mutex_lock(&_is_running_mutex);
		if (_is_running){ 
			pthread_mutex_unlock(&_is_running_mutex);
			return -1;//the object is already running, hence no other speculation can be started.
		}
		if (thread_instructions.size()!=const_args_spec_threads.size()){ 
			pthread_mutex_unlock(&_is_running_mutex);
			return -1;//invalid arguments
		}
		_is_running=true; 
		pthread_mutex_unlock(&_is_running_mutex);

		pthread_mutex_lock(&_spec_threads_mutex);
//		pthread_mutex_lock(&_global_readers_log_mutex);

		vector <int> threads_to_cancel;//a selective reset is performed, canceling only 
		_reset_spec_threads(threads_to_cancel, true, true);//the valid thread from the previous run, if any.

		pthread_mutex_lock(&_is_running_mutex);
		_pb._isExternal=true; 
		pthread_mutex_unlock(&_is_running_mutex);


		pthread_mutex_lock(&_valid_branch_mutex);
		_valid_branch=-1;
		pthread_mutex_unlock(&_valid_branch_mutex);

		_shared_data=shared_data;
	
		pthread_attr_t attr;
		pthread_attr_init (&attr);

		if (sched_policy== SCHED_FIFO||sched_policy==SCHED_RR||sched_policy== SCHED_OTHER){
			_sched_option=sched_policy;
		}
		else {
			_sched_option=SCHED_RR;
		}
		pthread_attr_setschedpolicy(&attr, _sched_option);


		int success=0;	
		_spec_threads.resize(thread_instructions.size());
		for (int i=thread_instructions.size()-1; i>=0; i--){
			pthread_mutex_lock(&_spec_threads[i]._mutex);
		}
		for (int i=0; i<thread_instructions.size(); i++){
			_spec_threads[i]._instructions=thread_instructions[i];
			_spec_threads[i]._args=const_args_spec_threads[i];
			success=pthread_create (&_spec_threads[i]._thread, &attr, 
					        _spec_threads[i]._instructions, 
						_spec_threads[i]._args);
			_thread_index[_spec_threads[i]._thread]= i;
			pthread_mutex_unlock(&_spec_threads[i]._mutex);
			if (success!=0){
				for (int j=i; j<thread_instructions.size(); j++){
					pthread_mutex_unlock(&_spec_threads[j]._mutex);
				}
				vector <int> threads_to_delete;
				for (int j=0; j>i; j++){
					threads_to_delete.push_back(j);
				}
				_reset_spec_threads(threads_to_delete, true, true);
//				pthread_mutex_unlock(&_global_readers_log_mutex);
				pthread_mutex_unlock(&_spec_threads_mutex);

				pthread_mutex_lock(&_is_running_mutex);
				_is_running=false;
				pthread_mutex_unlock(&_is_running_mutex);
				return -1;//a thread could not be created.
			}
			
		}
//		pthread_mutex_unlock(&_global_readers_log_mutex);
		pthread_mutex_unlock(&_spec_threads_mutex);
		return 0;
	};

	//!
        //! function to get the results of the speculation once the un-managed pre-
        //! branch has ended it's execution. 
        //! 
	//! TO BE NOTED:
	//!    * If the pre-branch does not validate the supposition, then none of
	//!    the results of the branches will be accepted.
	//!
	int get_results(){
		pthread_mutex_lock(&_is_running_mutex);
		if (!_is_running || !_pb._isExternal){
			pthread_mutex_unlock(&_is_running_mutex);
			return -1;//the object is running or manages it's pre-branch
		}
		pthread_mutex_unlock(&_is_running_mutex);


		pthread_mutex_lock(&_valid_branch_mutex);
		int valid_branch=_valid_branch;
		pthread_mutex_unlock(&_valid_branch_mutex);

		pthread_mutex_lock(&_spec_threads_mutex);
		if (valid_branch>=_spec_threads.size()){
			valid_branch=-1;
		}
		pthread_mutex_unlock(&_spec_threads_mutex);

		if (valid_branch==-1){
//			cout<<"No validation..."<<endl;
			pthread_mutex_lock(&_spec_threads_mutex);
			vector <int> threads_to_delete;
			for (int i=0; i<_spec_threads.size(); i++){
				threads_to_delete.push_back(i);
			}
			_reset_spec_threads(threads_to_delete, true, true);
			pthread_mutex_unlock(&_spec_threads_mutex);

			pthread_mutex_lock(&_is_running_mutex);
			_is_running=false;
			pthread_mutex_unlock(&_is_running_mutex);
			return -1; //no speculative thread was validated.
		}
		int a=0;

		//do{
			a=pthread_join (_spec_threads[valid_branch]._thread, NULL);
//			cout<<"inf7..."<<endl;

//		} while (a==0);

		pthread_mutex_lock(&_spec_threads_mutex);
//		cout<<valid_branch<<endl;
//		pthread_mutex_lock(&_spec_threads[valid_branch]._mutex); //LAZO INFINITO CON ESTO!
		if (!_spec_threads[valid_branch]._copied_data.empty()){ 
			//the changes that the valid thread made are communicated unto the &_shared_data
			map <void*, _data_copy*>::iterator i; 
			
			for (i=_spec_threads[valid_branch]._copied_data.begin(); i!=_spec_threads[valid_branch]._copied_data.end(); i++){
				memcpy((void*&)const_cast<void*&>(i->first), (void*)(i->second->_data), i->second->_size);
				free ((void*)(i->second->_data));
				free ((void*)(i->second));
				(*i).second=NULL;
			}								
			_spec_threads[valid_branch]._copied_data.clear();									
		}

//		pthread_mutex_unlock(&_spec_threads[valid_branch]._mutex);

		pthread_mutex_unlock(&_spec_threads_mutex);

		pthread_mutex_lock(&_is_running_mutex);
		_is_running=false;
		pthread_mutex_unlock(&_is_running_mutex);

		return 0;
	};

};
