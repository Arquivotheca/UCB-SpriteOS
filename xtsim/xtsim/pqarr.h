/*
 * Implements a priority queue based on arrays.
 */

#include <stddef.h>

typedef struct {
    int		**elem;
    int		maxN;
    int		n;
    int		(*lessThanProc)();
} PQArr;

#define PQArr_First(pq)		(pq->n == 1 ? NULL : pq->elem[1])

extern void PQArr_Print();
extern PQArr *PQArr_Create();
extern void PQArr_Delete();
extern void PQArr_Enqueue();
extern int *PQArr_Dequeue();
