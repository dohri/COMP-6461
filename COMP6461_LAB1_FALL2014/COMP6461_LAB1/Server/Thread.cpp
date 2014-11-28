

#include <stdio.h>
 
#include "Thread.h"
 

 
/*
 
 * This is the callback needed by the Thread class
 
 */
 
void * Thread::pthread_callback (void * ptrThis) 
 
{
 

 
    if ( ptrThis == NULL )
 
             return NULL ;      
 
    Thread  * ptr_this = (Thread *)(ptrThis) ;
 
    ptr_this->run();
 
    return NULL;
 
} 
 

 
void Thread::start () 
 
{
 
        int result;
 
    if(( result = _beginthread((void (*)(void *))Thread::pthread_callback,STKSIZE,this ))<0)
 
        {
 
                printf("_beginthread error\n");
 
                exit(-1);
 
        }       
 
}
 

 
/*
 

 
void Thread::run ()
 
{
 
        this->
 
}
 

 
bool Thread::mutex()
 
{
 
        
 
}*/
 

 
/*class Thread
 
{
 
        int id;
 
        char filename;
 
        char direction;
 
        bool flag;
 

 
        Thread(int newID)
 
        {
 
                id = newID;
 
                filename = ' ';
 
                direction = 'get';
 
                flag = false;
 
        }
 
        
 
        Thread(int newID, char newFile, char newDir, bool newFlag)
 
        {
 
                id = newID;
 
                filename = newFile;
 
                direction = newDir;
 
                flag = newFlag;
 
        }
 
        
 
        void lock()
 
        {
 
                flag = 1;
 
        }
 

 
        void unlock()
 
        {
 
                flag = 0;
 
        }
 

 
        void wait(int id)
 
        {
 
                sleep(50);
 
                //try again
 
        }
 

 
        void resume()
 
        {
 

 
        }
 
}*/
