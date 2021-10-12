/*
 *
 */

#include <assert.h>
#include "atax.h"
#include "field77_0.h"
#include "field77_1.h"
#include "field77_2.h"
#include "field77_3.h"
#include "field77_4.h"
#include "field77_5.h"

char *playField77[NUM_SCENE] = {
    field77_0_bits,
    field77_1_bits,
    field77_2_bits,
    field77_3_bits,
    field77_4_bits,
    field77_5_bits,
};

InitBoard(board, config)
    Board	*board;
    int		config;
{
    int		i;
    int		row, col;

    board->config = config;
    for (row = 0; row < NUM_ROW; row++) {
	for (col = 0; col < NUM_COL; col++) {
	    if (playField77[config][row] & (1 << (NUM_COL - 1 - col))) {
		board->playField[row][col] = BLOCKED_SQUARE;
	    } else {
		board->playField[row][col] = CLEAR_SQUARE;
	    }
	}
    }
    board->playField[0][0] = WHITE_SQUARE;
    board->playField[NUM_ROW-1][NUM_COL-1] = BLACK_SQUARE;

    for (i = 0; i < NUM_SQUARE_TYPE; i++) {
	board->numSquare[i] = 0;
    }
    for (row = 0; row < NUM_ROW; row++) {
	for (col = 0; col < NUM_COL; col++) {
	    switch (board->playField[row][col]) {
	    case CLEAR_SQUARE:	
		board->numSquare[CLEAR_SQUARE]++;
		break;
	    case BLOCKED_SQUARE:	
		board->numSquare[BLOCKED_SQUARE]++;
		break;
	    case WHITE_SQUARE:	
		board->numSquare[WHITE_SQUARE]++;
		break;
	    case BLACK_SQUARE:	
		board->numSquare[BLACK_SQUARE]++;
		break;
	    }
	}
    }
}

PlaceBlob(board, squareType, row, col)
    Board	*board;
    int		row, col;
    SquareType	squareType;
{
    int		rowIndx, colIndx;
    SquareType	opponentSquareType =
	    (squareType == WHITE_SQUARE ? BLACK_SQUARE : WHITE_SQUARE);

    board->numSquare[CLEAR_SQUARE]--;
    board->numSquare[squareType]++;
    board->playField[row][col] = squareType;
    for (rowIndx = MAX(row-1, 0); rowIndx <= MIN(row+1, NUM_ROW-1); rowIndx++) {
	for (colIndx = MAX(col-1,0);colIndx <= MIN(col+1,NUM_COL-1);colIndx++) {
	    if (board->playField[rowIndx][colIndx] == opponentSquareType) {
		board->numSquare[opponentSquareType]--;
		board->numSquare[squareType]++;
		board->playField[rowIndx][colIndx] = squareType;
	    }
	}
    }
}

RemoveBlob(board, row, col)
    Board	*board;
    int		row, col;
{
    board->numSquare[CLEAR_SQUARE]++;
    board->numSquare[board->playField[row][col]]--;
    board->playField[row][col] = CLEAR_SQUARE;
}

DoMove(board, squareType, fromRow, fromCol, toRow, toCol)
    Board	*board;
    SquareType	squareType;
    int		fromRow, fromCol, toRow, toCol;
{
    if (ABS(fromRow-toRow)==2 || ABS(fromCol-toCol)==2){
	RemoveBlob(board, fromRow, fromCol);
    }
    PlaceBlob(board, squareType, toRow, toCol);
}

/*
 * MIN-MAX search.
 */
FindMove(board, squareType, bestFromRow, bestFromCol, bestToRow, bestToCol,
	utilProc, depth)
    Board	*board;
    SquareType	squareType;
    int		*bestFromRow, *bestFromCol, *bestToRow, *bestToCol;
    int		(*utilProc)();
    int		depth;
{
    static int	numIter;
    int		fromRow, fromCol, toRow, toCol;
    int		tmp;
    int		bestScore;
    int		score;
    Board	tmpBoard;
    SquareType	opponentSquareType;

    if (depth == 0) {
	numIter = 0;
    }
    numIter++;
    opponentSquareType = (squareType==WHITE_SQUARE ?BLACK_SQUARE :WHITE_SQUARE);
    if (depth >= 2) {
	return utilProc(board);
    }
    bestScore = (depth % 2 == 0 ? -NUM_SQUARE-1 : NUM_SQUARE+1);
    for (fromRow = 0; fromRow < NUM_ROW; fromRow++) {
	for (fromCol = 0; fromCol < NUM_COL; fromCol++) {
	    if (board->playField[fromRow][fromCol] != squareType) {
		continue;
	    }
	    for (toRow = MAX(fromRow-2, 0); toRow <= MIN(fromRow+2, NUM_ROW-1);
		    toRow++) {
		for (toCol = MAX(fromCol-2,0);toCol <= MIN(fromCol+2,NUM_COL-1);
			toCol++) {
		    if (board->playField[toRow][toCol] != CLEAR_SQUARE) {
			continue;
		    }
		    bcopy(board, &tmpBoard, sizeof(Board));
		    DoMove(&tmpBoard,squareType,fromRow, fromCol, toRow, toCol);
		    score = FindMove(&tmpBoard, opponentSquareType,
			    &tmp, &tmp, &tmp, &tmp, utilProc, depth+1);
		    if (depth % 2 == 0 & score > bestScore ||
		        depth % 2 == 1 & score < bestScore ||
			    score == bestScore && ABS(fromRow-toRow) <= 1 &&
			    ABS(fromCol-toCol) <= 1) {
			bestScore = score;
			*bestFromRow = fromRow;
			*bestFromCol = fromCol;
			*bestToRow = toRow;
			*bestToCol = toCol;
		    }
		}
	    }
	}
    }
    if (ABS(bestScore) == NUM_SQUARE+1) {
	return utilProc(board);
    }
    return bestScore;
}

IsMove(board, squareType)
    Board	*board;
    SquareType	squareType;
{
    int		fromRow, fromCol, toRow, toCol;
    for (fromRow = 0; fromRow < NUM_ROW; fromRow++) {
	for (fromCol = 0; fromCol < NUM_COL; fromCol++) {
	    if (board->playField[fromRow][fromCol] != squareType) {
		continue;
	    }
	    for (toRow = MAX(fromRow-2, 0); toRow <= MIN(fromRow+2, NUM_ROW-1);
		    toRow++) {
		for (toCol = MAX(fromCol-2,0);toCol <= MIN(fromCol+2,NUM_COL-1);
			toCol++) {
		    if (board->playField[toRow][toCol] != CLEAR_SQUARE) {
			continue;
		    }
		    return 1;
		}
	    }
	}
    }
    return 0;
}

/*
main(argc, argv)
*/
foo(argc, argv)
    int		argc;
    char	*argv[];
{
    Board	board;
    PlayerType	player1 = HUMAN;
    PlayerType	player2 = COMPUTER;

    InitBoard(&board, 1);
    PrintBoard(&board);
    while (board.numSquare[CLEAR_SQUARE] > 0) {
	if (IsMove(&board, WHITE_SQUARE)) {
	    if (player1 == HUMAN) {
		UserMove(&board, WHITE_SQUARE);
	    } else {
		ComputerMove(&board, WHITE_SQUARE);
	    }
	    PrintBoard(&board);
	}
	if (IsMove(&board, BLACK_SQUARE)) {
	    if (player2 == HUMAN) {
		UserMove(&board, BLACK_SQUARE);
	    } else {
		ComputerMove(&board, BLACK_SQUARE);
	    }
	    PrintBoard(&board);
	}
    }
    printf("White=%d Black=%d\n",
	    board.numSquare[WHITE_SQUARE], board.numSquare[BLACK_SQUARE]);
}
