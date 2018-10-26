#include <iostream>
#include <list>
#include <stdio.h>
#include <stdlib.h>

#include "yque.h"

using namespace std;

typedef std::list<void *> List;

yque yque_create()
{
	List *l = new List();
	return l;
}

void yque_destroy(yque que)
{
	List *l = static_cast<List *>(que); 
	delete l;
}

void yque_destroy_custom(yque que, user_free free)
{
	List *l = static_cast<List *>(que);
	for (List::iterator it = l->begin(); it != l->end(); ++it)
	{
		free(*it);
	}
	delete l;
}

void yque_push_front(yque que, void *data)
{
	List *l = static_cast<List *>(que);
	l->push_front(data);
}

void yque_push_back(yque que, void *data)
{
	List *l = static_cast<List *>(que);
	l->push_front(data);
}

void *yque_pop_front(yque que)
{
	List *l = static_cast<List *>(que);
	void *front = l->front();
	l->pop_front();
	return front;
}

void *yque_pop_back(yque que)
{
	List *l = static_cast<List *>(que);
	void *back = l->back();
	l->pop_back();
	return back;
}

int yque_size(yque que)
{
	List *l = static_cast<List *>(que);
	return l->size();
}

int yque_empty(yque que)
{
	List *l = static_cast<List *>(que);
	if(l->empty())
		return 1;
	return 0;
}

void *yque_front(yque que)
{
	List *l = static_cast<List *>(que);
	return l->front();
}

void *yque_back(yque que)
{
	List *l = static_cast<List *>(que);
	return l->back();
}

yque_iter yque_iter_new(yque que)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = new List::iterator(l->begin());
	return it;
}

yque_iter yque_iter_next(yque_iter iter)
{
	List::iterator *it = static_cast<List::iterator *>(iter);
	++ (*it);
	return it;
}

yque_iter yque_iter_prev(yque_iter iter)
{
	List::iterator *it = static_cast<List::iterator *>(iter);
	-- (*it);
	return it;
}

yque_iter yque_iter_begin(yque que, yque_iter iter)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = static_cast<List::iterator *>(iter);
	*it = l->begin();
	return it;
}

int yque_iter_done(yque que, yque_iter iter)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = static_cast<List::iterator *>(iter);
	if (*it ==  l->end())
		return 1;
	return 0;
}

void *yque_iter_data(yque_iter iter)
{
	List::iterator *it = static_cast<List::iterator *>(iter);
	return **it;
}

void yque_erase(yque que, yque_iter iter)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = static_cast<List::iterator *>(iter);
	*it = l->erase(*it);
}

void yque_erase_custom(yque que, yque_iter iter, user_free free)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = static_cast<List::iterator *>(iter);
	free(**it);
	*it = l->erase(*it);
}

void yque_insert(yque que, yque_iter iter, void *data)
{
	List *l = static_cast<List *>(que);
	List::iterator *it = static_cast<List::iterator *>(iter);
	l->insert(*it, data);
}

void yque_iter_delete(yque_iter iter)
{
	List::iterator *it = static_cast<List::iterator *>(iter);
	delete it;
}


// int main(void) {
// 	void *q = yque_create();
// 	printf("q=%p\n", q);
// 	for(int i=0; i < 10; i++)
// 	{
// 		int *x = NULL;
// 		x = (int *) malloc(sizeof(int));
// 		*x = i;
// 		cout << "add " << *x << endl;
// 		yque_push_front(q, (void *) x);
// 	}

// 	yque_iter iter = yque_iter_new(q);
// 	for(; !yque_iter_done(q, iter); yque_iter_next(iter))
// 	{
// 		cout << *((int *)(yque_iter_data(iter))) << "ok" << endl;
// 	}
	
// 	yque_iter_begin(q, iter);
// 	for(; !yque_iter_done(q, iter); yque_iter_next(iter))
// 	{
// 		int *x = (int *)(yque_iter_data(iter));
// 		if((*x)%2 == 0)
// 			yque_erase_custom(q, iter, free);
// 		else if(*x == 5)
// 		{
// 			int *x = (int *) malloc(sizeof(int));
// 			*x = 11;
// 			yque_insert(q, iter, x);
// 		}
// 	}
// 	yque_iter_begin(q, iter);
// 	for(; !yque_iter_done(q, iter); yque_iter_next(iter))
// 	{
// 		cout << *((int *)(yque_iter_data(iter))) << "ok" << endl;
// 	}
// 	yque_iter_delete(iter);
// 	yque_destroy_custom(q, free);
// 	return 0;
// }
