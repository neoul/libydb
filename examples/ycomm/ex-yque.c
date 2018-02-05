#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yque.h"

int main(void) {
	yque que = yque_create();
	printf("que=%p\n", que);
	for(int i=0; i < 10; i++)
	{
		int *x = NULL;
		x = (int *) malloc(sizeof(int));
		*x = i;
		printf("yque_push_front %d\n", *x);
		yque_push_front(que, (void *) x);
	}

	yque_iter iter = yque_iter_new(que);
	for(; !yque_iter_done(que, iter); yque_iter_next(iter))
	{
        int *x = (int *)(yque_iter_data(iter));
        printf("yque_iteration %d\n", *x);
	}
	
	yque_iter_begin(que, iter);
	for(; !yque_iter_done(que, iter); yque_iter_next(iter))
	{
		int *x = (int *)(yque_iter_data(iter));
		if((*x)%2 == 0)
        {
            printf("yque_erase_custom %d\n", *x);
            yque_erase_custom(que, iter, free);
        }   
		else if(*x == 5)
		{
			int *x = (int *) malloc(sizeof(int));
			*x = 11;
            printf("yque_insert %d\n", *x);
			yque_insert(que, iter, x);
		}
	}
	yque_iter_begin(que, iter);
	for(; !yque_iter_done(que, iter); yque_iter_next(iter))
	{
        int *x = (int *)(yque_iter_data(iter));
        printf("yque_iteration %d\n", *x);
	}
	yque_iter_delete(iter);
	yque_destroy_custom(que, free);
	return 0;
}