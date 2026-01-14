#include "linked_list.h"
#include "semaphore.h"
// Creates and returns a new list
list_t* list_create()
{
  //create space for our list using malloc.
  list_t* ll = (list_t *)malloc(sizeof(list_t));
  
  //initialize attributes
  if(ll)
    {
      ll->head = NULL;
      ll->count = 0;
    }
    return ll;
}

// Destroys a list
void list_destroy(list_t* list)
{
  list_node_t* current = list->head;
  
  while(current)
    {
      list_node_t* next = current->next;
      free(current);
      current = next;
    }

  free(list);
}

// Returns beginning of the list
list_node_t* list_begin(list_t* list)
{
  return list->head;
}

// Returns next element in the list
list_node_t* list_next(list_node_t* node)
{
  if (node)
    {
      return node->next;
    }

  return NULL;
}

// Returns data in the given list node
void* list_data(list_node_t* node)
{
  if (node)
    {
      return node->data;
    }
  
  return NULL;
}

// Returns the number of elements in the list
size_t list_count(list_t* list)
{
  return list->count;
}

// Adjusted
// Finds the first node in the list with the given data
list_node_t* list_find(list_t* list, void* data) {
    if (!list) {
        return NULL;
    }

    list_node_t* current = list->head;
    while (current) {
        if (current->data == data) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Adjusted
// Inserts a new node at the beginning of the list with the given data
void list_insert(list_t* list, void* data) {
    if (!list) {
        return;
    }

    list_node_t* new_node = malloc(sizeof(list_node_t));
    if (!new_node) {
        return; // Allocation failed
    }

    new_node->data = data;
    new_node->next = list->head;
    new_node->prev = NULL;

    if (list->head) {
        list->head->prev = new_node;
    }

    list->head = new_node;
    list->count++;
}

// Adjusted
// Removes a node from the list and frees the node resources
void list_remove(list_t* list, list_node_t* node) {
    if (!list || !node) {
        return;
    }

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    free(node);
    list->count--;
}

// Executes a function for each element in the list
void list_foreach(list_t* list)
{
  for(list_node_t* curr = list->head; curr != NULL; curr = curr->next)
    {
      sem_post(curr->data);
    }
}
