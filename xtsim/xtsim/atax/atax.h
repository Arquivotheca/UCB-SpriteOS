/*
 *
 */

#define MIN(a,b)	( (a) < (b) ? (a) : (b) )
#define MAX(a,b)	( (a) > (b) ? (a) : (b) )
#define ABS(a)		( (a) < 0 ? -(a) : (a) )

#define NUM_ROW		7
#define NUM_COL		7
#define NUM_SQUARE	(NUM_ROW * NUM_COL)
#define NUM_SCENE	6

#define CLEAR_SQUARE	0
#define BLOCKED_SQUARE	1
#define WHITE_SQUARE	2
#define BLACK_SQUARE	3
#define NUM_SQUARE_TYPE	4
#define SELECT_SQUARE	5

typedef int SquareType;

typedef struct {
    int			config;
    int			numSquare[NUM_SQUARE_TYPE];
    unsigned char	playField[NUM_ROW][NUM_COL];
} Board;

typedef enum {HUMAN, COMPUTER} PlayerType;
