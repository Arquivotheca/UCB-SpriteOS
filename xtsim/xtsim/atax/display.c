/*
 *
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xw/Xw.h>
#include <Xw/WorkSpace.h>
#include <Xw/RCManager.h>
#include <Xw/PButton.h>
#include <Xw/SText.h>
#include "atax.h"
#include "schedule.h"

#define BORDER_WIDTH	0
static int	cellWidth, cellHeight;

static Widget	topLevel, topManager, controlPannel, buttonPannel;
static Widget	playField;
static GC	gc;
static Widget	restartButton, quitButton;
static Widget	sceneButton[NUM_SCENE];
static Widget	msgWindow;

static Pixmap	white, lightGrey, grey, darkGrey, black;

static PlayerType  player1 = HUMAN;
static PlayerType  player2 = COMPUTER;

DrawSquare(widget, gc, squareType, row, col)
    Widget	widget;
    GC		gc;
    SquareType	squareType;
    int		row, col;
{
    XGCValues	gcValues;
    int		x = BORDER_WIDTH + row*cellWidth;
    int		y = BORDER_WIDTH + col*cellHeight;

    if (squareType != SELECT_SQUARE) {
	XClearArea(XtDisplay(widget), XtWindow(widget),
		x, y, cellWidth, cellHeight, FALSE);
	XDrawRectangle(XtDisplay(widget), XtWindow(widget), gc,
		x, y, cellWidth, cellHeight);
	XSetFillStyle(XtDisplay(widget), gc, FillTiled);
	XFillRectangle(XtDisplay(widget), XtWindow(widget), gc,
		x+3, y+3, cellWidth-4, cellHeight-4);
	XSetFillStyle(XtDisplay(widget), gc, FillSolid);
    }
    switch (squareType) {
    case CLEAR_SQUARE:
	break;
    case BLOCKED_SQUARE:
	XFillRectangle(XtDisplay(widget), XtWindow(widget), gc,
		x, y, cellWidth, cellHeight);
	break;
    case WHITE_SQUARE:
	XGetGCValues(XtDisplay(widget), gc,
		GCForeground | GCBackground, &gcValues);
	XSetForeground(XtDisplay(widget), gc, gcValues.background);
	XFillArc(XtDisplay(widget), XtWindow(widget), gc,
		x+6, y+6, cellWidth-12, cellHeight-12, 0, 360*64);
	XSetForeground(XtDisplay(widget), gc, gcValues.foreground);
	break;
    case BLACK_SQUARE:
	XFillArc(XtDisplay(widget), XtWindow(widget), gc,
		x+6, y+6, cellWidth-12, cellHeight-12, 0, 360*64);
	break;
    case SELECT_SQUARE:
	XSetFillStyle(XtDisplay(widget), gc, FillTiled);
	XFillArc(XtDisplay(widget), XtWindow(widget), gc,
		x+12, y+12, cellWidth-24, cellHeight-24, 0, 360*64);
	XSetFillStyle(XtDisplay(widget), gc, FillSolid);
	break;
    }
}

DrawArea(widget, gc, board, row, col)
    Widget	widget;
    GC		gc;
    Board	*board;
    int		row, col;
{
    int		rowIndx, colIndx;

    for (rowIndx = MAX(row-1, 0); rowIndx <= MIN(row+1, NUM_ROW-1); rowIndx++) {
        for (colIndx = MAX(col-1,0);colIndx <= MIN(col+1,NUM_COL-1);colIndx++) {
	    if (board->playField[rowIndx][colIndx]==board->playField[row][col]){
		DrawSquare(playField, gc,
			(SquareType) board->playField[rowIndx][colIndx],
			rowIndx, colIndx);
	    }
        }
    }
}

void Redisplay(widget, board)
    Widget	widget;
    Board	*board;
{
    static int	initialized = 0;
    Arg		args[2];
    int		numArg;
    XGCValues	gcValues;
    Dimension	width, height;
    XSegment	gridSegments[2*(NUM_ROW+NUM_COL+2)];
    int		numGridSegment;
    int		row, col;

    if (!initialized) {
	/*
	 * Set up GC.
	 */
	initialized = 1;
	numArg = 0;
	XtSetArg(args[numArg], XtNforeground, &gcValues.foreground); numArg++;
	XtSetArg(args[numArg], XtNbackground, &gcValues.background); numArg++;
	XtGetValues(widget, args, numArg);
	white = XwCreateTile(XtScreen(widget),
		gcValues.foreground, gcValues.background, XwFOREGROUND);
	lightGrey = XwCreateTile(XtScreen(widget),
		gcValues.foreground, gcValues.background, Xw25_FOREGROUND);
	grey = XwCreateTile(XtScreen(widget),
		gcValues.foreground, gcValues.background, Xw50_FOREGROUND);
	darkGrey = XwCreateTile(XtScreen(widget),
		gcValues.foreground, gcValues.background, Xw75_FOREGROUND);
	black = XwCreateTile(XtScreen(widget),
		gcValues.foreground, gcValues.background, XwBACKGROUND);
	gcValues.line_width = 2;
	gcValues.tile = grey;
	gc = XtGetGC(widget, GCForeground | GCBackground | GCLineWidth | 
		GCTile, &gcValues);
    }
    /*
     * Get height and width.
     */
    numArg = 0;
    XtSetArg(args[numArg], XtNwidth, &width); numArg++;
    XtSetArg(args[numArg], XtNheight, &height); numArg++;
    XtGetValues(widget, args, numArg);
    cellWidth = (width-2*BORDER_WIDTH)/NUM_COL;
    cellHeight = (height-2*BORDER_WIDTH)/NUM_ROW;
    /*
     * Draw board.
     */
    for (row = 0; row < NUM_ROW; row++) {
        for (col = 0; col < NUM_COL; col++) {
	    DrawSquare(widget, gc,
		    (SquareType) board->playField[row][col], row, col);
        }
    }

}

PrintBoard()
{
}

int
BlackUtilProc(board)
    Board	*board;
{
    return board->numSquare[BLACK_SQUARE] - board->numSquare[WHITE_SQUARE];
}

#define BLINK_INTERVAL 200

ComputerMove(argBoard, argSquareType)
    Board       *argBoard;
    SquareType  argSquareType;
{
    Board       *board = argBoard;
    SquareType  squareType = argSquareType;
    Board       newBoard;
    int         fromRow, fromCol, toRow, toCol;
    int         score;

    score=FindMove(board, squareType, &fromRow, &fromCol, &toRow, &toCol,
	    BlackUtilProc, 0);
    if (score > -NUM_SQUARE-1) {
	/*
	 * Blink source square.
	 */
	DrawSquare(playField, gc,
		(SquareType) CLEAR_SQUARE,
		fromRow, fromCol);
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) board->playField[fromRow][fromCol],
		fromRow, fromCol);
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) CLEAR_SQUARE,
		fromRow, fromCol);
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) board->playField[fromRow][fromCol],
		fromRow, fromCol);
	XSync(XtDisplay(topLevel), 0);
	/*
	 * Move piece.
	 */
        DoMove(board, squareType, fromRow, fromCol, toRow, toCol);
	DrawSquare(playField, gc,
		(SquareType) board->playField[fromRow][fromCol],
		fromRow, fromCol);
	/*
	 * Blink destination square.
	 */
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) board->playField[toRow][toCol],
		toRow, toCol);
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) CLEAR_SQUARE,
		toRow, toCol);
	XSync(XtDisplay(topLevel), 0);
	Delay(BLINK_INTERVAL);
	DrawSquare(playField, gc,
		(SquareType) board->playField[toRow][toCol],
		toRow, toCol);
	DrawArea(playField, gc, board, toRow, toCol);
	XSync(XtDisplay(topLevel), 0);
    }
}

typedef enum { USER_BUSY, USER_WAIT, USER_SEL } UserState;

UserMove(widgetArg, boardArg, eventArg)
    Widget		widgetArg;
    Board		*boardArg;
    XButtonEvent	*eventArg;
{
    Widget		widget = widgetArg;
    Board		*board = boardArg;
    XButtonEvent	*event = eventArg;
    static int		fromRow, fromCol, toRow, toCol;
    static UserState	userState = USER_WAIT;
    static SquareType	squareType = WHITE_SQUARE;
    static char		msgBuf[80];

    if (!IsMove(board, WHITE_SQUARE)) {
	return;
    }
    switch (userState) {
    case USER_BUSY:
	break;
    case USER_WAIT:
	fromRow = (event->x-BORDER_WIDTH)/cellWidth;
	fromCol = (event->y-BORDER_WIDTH)/cellHeight;
	if (board->playField[fromRow][fromCol] != squareType) {
	    PrintMsg("Not your piece.");
	    break;
	}
	userState = USER_SEL;
	DrawSquare(widget, gc, SELECT_SQUARE, fromRow, fromCol);
	break;
    case USER_SEL:
	toRow = (event->x-BORDER_WIDTH)/cellWidth;
	toCol = (event->y-BORDER_WIDTH)/cellHeight;
	if (board->playField[toRow][toCol] == squareType) {
	    DrawSquare(widget, gc, squareType, fromRow, fromCol);
	    fromRow = toRow;
	    fromCol = toCol;
	    DrawSquare(widget, gc, SELECT_SQUARE, fromRow, fromCol);
	    break;
	}
	if (fromRow < 0 || fromRow >= NUM_ROW ||
		toRow < 0 || toRow >= NUM_ROW ||
		fromCol < 0 || fromCol >= NUM_COL ||
		toCol < 0 || toCol >= NUM_COL) {
		PrintMsg("Off board.");
	    break;
	}
	if (board->playField[toRow][toCol] != CLEAR_SQUARE) {
	    PrintMsg("Not a clear space.");
	    break;
	}
	if (ABS(fromRow-toRow) > 2 || ABS(fromCol-toCol) > 2) {
	    PrintMsg("Too far.");
	    break;
	}
	userState = USER_BUSY;
	if (Spawn("ComputerMove")) {
	    break;
	}
	/*
	 * Child process.
	 */
	DoMove(board, squareType, fromRow, fromCol, toRow, toCol);
	DrawSquare(playField, gc,
		(SquareType) board->playField[fromRow][fromCol],
		fromRow, fromCol);
	DrawArea(playField, gc, board, toRow, toCol);
	if (IsMove(board, BLACK_SQUARE)) {
	    sprintf(msgBuf, "Thinking.\nScore: %d to %d.",
		board->numSquare[WHITE_SQUARE], board->numSquare[BLACK_SQUARE]);
	    PrintMsg(msgBuf);
	    XSync(XtDisplay(topLevel), 0);
	    ComputerMove(board, BLACK_SQUARE);
	}
	while (!IsMove(board, WHITE_SQUARE) && IsMove(board, BLACK_SQUARE)) {
	    ComputerMove(board, BLACK_SQUARE);
	}
	sprintf(msgBuf, "Your turn.\nScore: %d to %d.",
	    board->numSquare[WHITE_SQUARE], board->numSquare[BLACK_SQUARE]);
	PrintMsg(msgBuf);
	userState = USER_WAIT;
	Terminate();
	break;
    }
}

static Board	board;

InitBoardCallbackProc(widget, config)
    Widget	widget;
    int		config;
{
    InitBoard(&board, config);
    Redisplay(playField, &board);
    PrintMsg(" ");
}

RestartBoardCallbackProc()
{
    InitBoard(&board, board.config);
    Redisplay(playField, &board);
    PrintMsg(" ");
}

PrintMsg(msg)
    char *	msg;
{
    Arg		args[1];

    XtSetArg(args[0], XtNstring, msg);
    XtSetValues(msgWindow, args, 1);
}

main(argc, argv)
    int		argc;
    char	*argv[];
{
    Arg		args[10];
    int		numArg;
    int		i;
    char	charBuf[80];
    extern	exit();

    InitBoard(&board, 1);
    InitSim();

    /*
     * TopLevel consists of a playField and a controlPannel.
     */
    topLevel = XtInitialize(argv[0], "Atax", NULL, 0, &argc, argv);
    numArg = 0;
    XtSetArg(args[numArg], XtNcolumns, 2); numArg++;
    XtSetArg(args[numArg], XtNforceSize, TRUE); numArg++;
    topManager = XtCreateManagedWidget("topManager", XwrowColWidgetClass,
	    topLevel, args, numArg);
    /*
     * PlayField.
     */
    numArg = 0;
    XtSetArg(args[numArg], XtNwidth, 64*NUM_ROW+2*BORDER_WIDTH); numArg++;
    XtSetArg(args[numArg], XtNheight, 64*NUM_COL+2*BORDER_WIDTH); numArg++;
    playField = XtCreateManagedWidget("playField", XwworkSpaceWidgetClass,
	    topManager, args, numArg);
    XtAddCallback(playField, XtNexpose, Redisplay, &board);
    XtAddEventHandler(playField, ButtonPressMask, FALSE, UserMove, &board);
    /*
     * ControlPannel.
     */
    numArg = 0;
    XtSetArg(args[numArg], XtNcolumns, 1); numArg++;
    XtSetArg(args[numArg], XtNforceSize, TRUE); numArg++;
    controlPannel = XtCreateManagedWidget("controlPannel", XwrowColWidgetClass,
	    topManager, args, numArg);
    /*
     * ControlPannel consists of various buttons and a mesage window.
     */
    numArg = 0;
    XtSetArg(args[numArg], XtNcolumns, 2); numArg++;
    XtSetArg(args[numArg], XtNforceSize, TRUE); numArg++;
    buttonPannel = XtCreateManagedWidget("buttonPannel", XwrowColWidgetClass,
	    controlPannel, args, numArg);
    
    restartButton = XtCreateManagedWidget("restart", XwpushButtonWidgetClass,
	    buttonPannel, args, numArg);
    XtAddCallback(restartButton, XtNselect, RestartBoardCallbackProc, NULL);

    quitButton = XtCreateManagedWidget("quit", XwpushButtonWidgetClass,
	    buttonPannel, args, numArg);
    XtAddCallback(quitButton, XtNselect, exit, NULL);
    for (i = 0; i < NUM_SCENE; i++) {
	sprintf(charBuf, "scene%d", i);
	sceneButton[i] = XtCreateManagedWidget(charBuf, XwpushButtonWidgetClass,
		buttonPannel, NULL, 0);
	XtAddCallback(sceneButton[i], XtNselect, InitBoardCallbackProc, i);
    }
    numArg = 0;
    XtSetArg(args[numArg], XtNstring, "Msg:"); numArg++;
    msgWindow = XtCreateManagedWidget("msgWindow", XwstaticTextWidgetClass,
	    controlPannel, args, numArg);

    XtRealizeWidget(topLevel);
    Terminate();
}
