/*
 * Implements a priority queue based on arrays.
 */

#include "pqarr.h"

#define INIT_MAX_QUEUE_ELEM	1000
#define PARENT(i)		((i) >> 1)
#define LEFT_CHILD(i)		((i) << 1)
#define RIGHT_CHILD(i)		(((i) << 1) + 1)
#define LESSTHAN_CHILD(pq, i)		\
    (LEFT_CHILD(i) >= pq->n ? 0 :	\
    (RIGHT_CHILD(i) >= pq->n ? LEFT_CHILD(i) :	\
    ((pq)->lessThanProc((pq)->elem[LEFT_CHILD(i)],(pq)->elem[RIGHT_CHILD(i)]) ?\
	LEFT_CHILD(i) : RIGHT_CHILD(i))))

void
PQArr_Print(pq, printProc)
    PQArr	*pq;
    void	(*printProc)();
{
    int		i;

    printf("MAXN:%d N:%d ELEM:", pq->maxN, pq->n);
    for (i = 1; i < pq->n; i++) {
	printProc(pq->elem[i]);
	if (i % 25 == 0) {
	    printf("\n");
	}
    }
    printf("\n");
}

PQArr *
PQArr_Create(lessThanProc)
    int		(*lessThanProc)();
{
    PQArr	*pq;

    pq = (PQArr *) malloc(sizeof(PQArr));
    pq->elem = (int **) malloc(INIT_MAX_QUEUE_ELEM * sizeof(int*));
    pq->maxN = INIT_MAX_QUEUE_ELEM;
    pq->n = 1;
    pq->lessThanProc = lessThanProc;
    return pq;
}

void
PQArr_Delete(pq)
    PQArr	*pq;
{
    free(pq->elem);
    free(pq);
}

void
PQArr_Enqueue(pq, elemPtr)
    PQArr	*pq;
    int		*elemPtr;
{
    int		curElem;

    if (pq->n >= pq->maxN) {
	pq->maxN *= 4;
	pq->elem = (int **) realloc(pq->elem, pq->maxN * sizeof(int *));
    }
    for (curElem = pq->n; PARENT(curElem) != 0 &&
	    pq->lessThanProc(elemPtr, pq->elem[PARENT(curElem)]);
	    curElem = PARENT(curElem)) {
	pq->elem[curElem] = pq->elem[PARENT(curElem)];
    }
    pq->elem[curElem] = elemPtr;
    pq->n++;
}

int *
PQArr_Dequeue(pq)
    PQArr	*pq;
{
    int		curElem;
    int		nextElem;
    int		*retElemPtr;
    int		*lastElemPtr;

    if (pq->n == 1) {
	return NULL;
    }
    retElemPtr = pq->elem[1];
    pq->n--;
    lastElemPtr = pq->elem[pq->n];

    curElem = 1;
    nextElem = LESSTHAN_CHILD(pq, curElem);
    while (nextElem != 0 && pq->lessThanProc(pq->elem[nextElem], lastElemPtr)) {
	pq->elem[curElem] = pq->elem[nextElem];
	curElem = nextElem;
	nextElem = LESSTHAN_CHILD(pq, curElem);
    }
    pq->elem[curElem] = lastElemPtr;
    return retElemPtr;
}
