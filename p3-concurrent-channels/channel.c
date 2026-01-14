#include "channel.h"

// Creates a new channel with the provided size and returns it to the caller
// A 0 size indicates an unbuffered channel, whereas a positive size indicates a buffered channel

// size refers to # of messages, not number of bytes (not sure what that means or what to actually do about it)
chan_t* channel_create(size_t size)
{
  /* IMPLEMENT THIS */
  
  // to proceed, we malloc space and initialize channel_t data

  // ignore spurious requests
  if (size < 0)
    {
      return NULL; 
    }

  // Allocate memory for the channel (using malloc)
  chan_t* ptr = (chan_t*)malloc(sizeof(chan_t));
  if (ptr == NULL)
    {
      // Handle memory allocation failure
      return NULL;
    }

  // Initialize buffer for buffered channels
  if (size > 0)
    {
      // (lecture 24) use buffer_create with passed size  
      ptr->buffer = buffer_create(size);
      // if buffer fails 
      if (ptr->buffer == NULL)
	{
	  // Handle buffer creation failure
	  free(ptr);
	  return NULL;
	}
    }
  
  // else (== 0) indicates unbuffered channel 
  else
    {
      // handle unbuffered channel creation
      // free ptr and return NULL until implementing bonus (?)
      free(ptr);
      return NULL;
      // ptr->buffer = NULL;
    }
   
  // init semaphore for mutex (will always be 0 or 1)
  sem_init(&ptr->mutex,0,1);
  
  // init semaphore for available spaces (= size since size represents # of elements in the buffer)
  sem_init(&ptr->available, 0, (unsigned int)size);

  // init semaphore for slots used (initially 0 - no spots are used)
  sem_init(&ptr->used, 0, 0); 
  
  //initilize locks.
  pthread_mutex_init(&ptr->recv_lock, NULL);
  pthread_mutex_init(&ptr->send_lock, NULL);
  
  //initialize linked lists.
  ptr->recv_list = list_create();
  ptr->send_list = list_create();

  //initially not closed.
  ptr->closed = false;
  
  return ptr;
}

// Writes data to the given channel
// This can be both a blocking call i.e., the function only returns on a successful completion of send (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is full (blocking = false)
// In case of the blocking call when the channel is full, the function waits til the channel has space to write the new data
// Returns SUCCESS for successfully writing data to the channel,
// WOULDBLOCK if the channel is full and the data was not added to the buffer (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_send(chan_t* channel, void* data, bool blocking)
{
  // from lec 24 - channel is the channel to perform operation, data is a single message to send (no size, includes NULL)
  // channel has finite buffer to add/remove data
  // purpose of send is to add one message to the buffer, using buffer_add
  // concerns: many threads call send, so we need mutex/semaphores. if the buffer is full, we need to wait, which we also use semaphores for
  
  // spurious request
  if (channel == NULL)
    {
      // return other_error (IDK)
      return OTHER_ERROR;
    }

  // non-blocking check
  if (!blocking)
    {

      // trywait for available, if fails just return WOULDBLOCK
      if (sem_trywait(&channel->available) == -1)
	{
	  // wait on lock before checking close
	  sem_wait(&channel->mutex);
	  // CLOSED_ERROR supersedes regular protocol
	  if (channel->closed)
	    {
	      // unlock mutex
	      sem_post(&channel->mutex);
	      return CLOSED_ERROR;
	    }

	  // unlock mutex if not needed
	  sem_post(&channel->mutex);
	  return WOULDBLOCK;
	}
    }

  // blocking
  else
    {
       // we wait on our semaphore for available space
      sem_wait(&channel->available);
    }

  // lock mutex for close check/use in critical section
  sem_wait(&channel->mutex);

  //  if the channel is closed before critical section
  if (channel->closed)
    {
      // unlock mutex
      sem_post(&channel->mutex);
      // post for available (only senders - poke other waiting threads)
      sem_post(&channel->available);
      return CLOSED_ERROR;
    }
  
  
  // bool added attempts to add into buffer
  bool added = buffer_add(data, channel->buffer);
  
  // if not added there was some error
  if (!added)
    {
      // release sem
      sem_post(&channel->mutex);
      return OTHER_ERROR;
    }

  // release sem
  sem_post(&channel->mutex);
  // post for used slots (since data is sent)
  sem_post(&channel->used);
  
  //post a message if send_list is not empty.
  pthread_mutex_lock(&channel->send_lock);
  if(list_count(channel->send_list) > 0)
    {
      list_foreach(channel->send_list);
    }
  pthread_mutex_unlock(&channel->send_lock);
  // o/w success
  return SUCCESS;
}

// Reads data from the given channel and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This can be both a blocking call i.e., the function only returns on a successful completion of receive (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is empty (blocking = false)
// In case of the blocking call when the channel is empty, the function waits till the channel has some data to read
// Returns SUCCESS for successful retrieval of data,
// WOULDBLOCK if the channel is empty and nothing was stored in data (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_receive(chan_t* channel, void** data, bool blocking)
{  
  // spurious request
  if (channel == NULL)
    {
      // return other_error 
      return OTHER_ERROR;
    }

  // non-blocking check
  if (!blocking)
    {
      // trywait for receive
      if (sem_trywait(&channel->used) == -1)
	{
	  // lock mutex before close check
	  sem_wait(&channel->mutex);

	  // CLOSED_ERROR supersedes regular protocol
	  if (channel->closed)
	    {
	      // unlock mutex
	      sem_post(&channel->mutex);
	      // post for used (poke other waiting threads)
	      sem_post(&channel->used);
	      return CLOSED_ERROR;
	    }
	  // unlock mutex
	  sem_post(&channel->mutex);
	  return WOULDBLOCK;
	}
    }

  // blocking
  else
    {
      // wait on semaphore for slots used
      sem_wait(&channel->used);
    }

  // lock mutex for close check/use in critical section
  sem_wait(&channel->mutex);
  
  // if channel is closed
  if (channel->closed)
    {
      // unlock mutex
      sem_post(&channel->mutex);
      // post for used (poke other waiting recv threads)
      sem_post(&channel->used);
      return CLOSED_ERROR;
    }


  // void remData attempts to remove data from buffer
  void *remData = buffer_remove(channel->buffer);

  // if not removed there was some error
  if (remData == BUFFER_EMPTY)
    {
      // release sem
      sem_post(&channel->mutex);
      return OTHER_ERROR;
    }
  
  // then put remData into data pointer
  *data = remData;
  // release mutex sem
  sem_post(&channel->mutex);
  // post for available (since data is received)
  sem_post(&channel->available); 
  
  //if recieve list has size greater than 0, post.
  pthread_mutex_lock(&channel->recv_lock);
  if(list_count(channel->recv_list) > 0)
    {
      list_foreach(channel->recv_list);
    }
  pthread_mutex_unlock(&channel->recv_lock);
  // success  
  return SUCCESS;
}

// Closes the channel and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the channel is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the channel is already closed, and
// OTHER_ERROR in any other error case
enum chan_status channel_close(chan_t* channel)
{
  // spruious request
  if (channel == NULL)
    {
      return OTHER_ERROR;
    }

  // lock mutex for close check
  sem_wait(&channel->mutex);
    // check if channel is closed (error handling)
  if (channel->closed)
    {
      sem_post(&channel->mutex);
      // post and return error if so
      return CLOSED_ERROR;
    }
  
  // last thread will always poke the next thread -> no need for inital check 
  
  // close channel
  channel->closed = true;
  
  // chain reaction/domino effect: do sem_post for receiver and one for sender. that will poke one thread for recv/send. these threads
  // will poke another thread. that will wake up one blocked recv and one blocked send, they will realize it is closed. then these
  // threads poke another thread (doing one sem_post for rec or send end). that will wake up another blocked thread and so on.
  
  // wake up potentially waiting senders and receivers 
  sem_post(&channel->available);
  sem_post(&channel->used);

  // relock mutex
  sem_post(&channel->mutex);
  
  //post signal to select.
  pthread_mutex_lock(&channel->send_lock);
  if(list_count(channel->send_list) > 0)
    {
      list_foreach(channel->send_list);
    }
  pthread_mutex_unlock(&channel->send_lock);
  
  pthread_mutex_lock(&channel->recv_lock);
  if(list_count(channel->recv_list) > 0)
    {
      list_foreach(channel->recv_list);
    }
  pthread_mutex_unlock(&channel->recv_lock);
  //return SUCCESS
  return SUCCESS;
}

// Frees all the memory allocated to the channel
// The caller is responsible for calling channel_close and waiting for all threads to finish their tasks before calling channel_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if channel_destroy is called on an open channel, and
// OTHER_ERROR in any other error case
enum chan_status channel_destroy(chan_t* channel)
{

  // spurious request
  if (channel == NULL)
    {
      return OTHER_ERROR;
    }

  // if the channel is still open
  if (!channel->closed)
    {
      return DESTROY_ERROR;
    }

  // using sem destory on sems
  sem_destroy(&channel->mutex);
  sem_destroy(&channel->available);
  sem_destroy(&channel->used);

  //destroy send and receive lists in the channel.
  list_destroy(channel->recv_list);
  list_destroy(channel->send_list);

  //destroy recv and send locks.
  pthread_mutex_destroy(&channel->recv_lock);
  pthread_mutex_destroy(&channel->send_lock);
  
  // free the buffer with buffer_free
  buffer_free(channel->buffer);

  // free channel
  free(channel);

  return SUCCESS;
}

// Takes an array of channels, channel_list, of type select_t and the array length, channel_count, as inputs
// This API iterates over the provided list and finds the set of possible channels which can be used to invoke the required operation (send or receive) specified in select_t
// If multiple options are available, it selects the first option and performs its corresponding action
// If no channel is available, the call is blocked and waits till it finds a channel which supports its required operation
// Once an operation has been successfully performed, select should set selected_index to the index of the channel that performed the operation and then return SUCCESS
// In the event that a channel is closed or encounters any error, the error should be propagated and returned through select
// Additionally, selected_index is set to the index of the channel that generated the error
enum chan_status channel_select(size_t channel_count, select_t* channel_list, size_t* selected_index)
{
  //status var.
  enum chan_status status = OTHER_ERROR;
  //Create a local semaphore pointer and initialize it to zero.
  sem_t local;
  sem_init(&local, 0, 0); 
  
  // Iterate though the array and add all the semaphores to all the channels
  size_t i;
  for(i = 0; i < channel_count; i++)
    {
      select_t* curr = &channel_list[i];

      // if the current channel is sending, we add local to recv list
      if(curr->is_send)
	{
	  // critical section
	  pthread_mutex_lock(&curr->channel->recv_lock);
	  if(list_find(curr->channel->recv_list, &local)==NULL)
	    {
	      list_insert(curr->channel->recv_list, &local);
	    }
	  pthread_mutex_unlock(&curr->channel->recv_lock);
	}
      // if the current channel is receiving, we add local to send list
      else
	{
	  // critical section
	  pthread_mutex_lock(&curr->channel->send_lock);
	  if(list_find(curr->channel->send_list, &local)==NULL)
	    {
	      list_insert(curr->channel->send_list, &local);
	    }
	  pthread_mutex_unlock(&curr->channel->send_lock);
	}   
    }

  // loop until any channel performs the operation
  while(1)
    {
      i = 0;
      // perform send/recv until it does not return WOULDBLOCK
      while(true)
	{
	  select_t *curr = &channel_list[i];
	  if(curr->is_send)
	    {
	      status = channel_send(curr->channel, curr->data, false);
	    }
	  else
	    {
	      status = channel_receive(curr->channel, &curr->data, false);
	    }

	  // if you iterate thru list and don't find anything channel has to wait
	  if(status != WOULDBLOCK || i == (channel_count-1))
	    {
	      break;
	    }
	  i++;
	}

      // selected_index = current index
      *selected_index = i;
      //if status is not wouldblock, have performed the operation remove all nodes.
      if(status != WOULDBLOCK)
	{
	  for(i = 0; i < channel_count; i++)
	    {
	      select_t* curr = &channel_list[i];
	  
	      //if the current channel is sending we add local to recv list.
	      if(curr->is_send)
		{
		  pthread_mutex_lock(&curr->channel->recv_lock);
		  list_node_t *rnode = list_find(curr->channel->recv_list, &local);
		  if(rnode)
		    {
		      list_remove(curr->channel->recv_list, rnode);
		    }
		  pthread_mutex_unlock(&curr->channel->recv_lock);
		}
	      //if the current channel is receiving we add local to send list.
	      else
		{
		  pthread_mutex_lock(&curr->channel->send_lock);
		  list_node_t *snode = list_find(curr->channel->send_list, &local);
		  if(snode)
		    {
		      list_remove(curr->channel->send_list, snode);
		    }
		  pthread_mutex_unlock(&curr->channel->send_lock);
		}   
	    }
      
	  // Destroy the semaphore and return the value.
	  sem_destroy(&local);
	  return status;
	}

      //if the status is blocked we wait.
      sem_wait(&local);
    }
}
