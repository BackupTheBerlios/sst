#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <curses.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#include "sst.h"
#include "sstlinux.h"

static int linecount;	/* for paging */
static int curses = TRUE;

WINDOW *curwnd;

static void outro(void)
/* wrap up, either normally or due to signal */
{
    if (curses) {
	clear();
	curs_set(1);
	(void)refresh();
	(void)resetterm();
	//(void)echo();
	(void)endwin();
	putchar('\n');
    }
}

void iostart(int usecurses) 
{
    if ((curses = usecurses)) {
	if (atexit(outro)){
	    fprintf(stderr,"Unable to register outro(), exiting...\n");
	    exit(1);
	}
	(void)initscr();
#ifdef KEY_MIN
	keypad(stdscr, TRUE);
#endif /* KEY_MIN */
	(void)saveterm();
	(void)nonl();
	(void)cbreak();
	//(void)noecho();
	FULLSCREEN_WINDOW = stdscr;
	SRSCAN_WINDOW     = newwin(12, 25, 0,       0);
	REPORT_WINDOW     = newwin(10, 0,  1,       25);
	LRSCAN_WINDOW     = newwin(10, 0,  0,       64); 
	LOWER_WINDOW      = newwin(0,  0,  12,      0);
	BOTTOM_WINDOW     = newwin(1,  0,  LINES-1, 0); 
	scrollok(LOWER_WINDOW, TRUE);
    }
}


void waitfor(void)
/* wait for user action -- OK to do nothing if on a TTY */
{
    if (curses)
	getch();
}

void pause_game(int i) 
{
    char *prompt;
    char buf[BUFSIZ];
    if (i==1) {
	if (skill > 2)
	    prompt = "[ANOUNCEMENT ARRIVING...]";
	else
	    prompt = "[IMPORTANT ANNOUNCEMENT ARRIVING -- PRESS ENTER TO CONTINUE]";
    }
    else {
	if (skill > 2)
	    prompt = "[CONTINUE?]";
	else
	    prompt = "[PRESS ENTER TO CONTINUE]";

    }
    if (curses) {
	drawmaps(0);
	setwnd(BOTTOM_WINDOW);
	wclear(BOTTOM_WINDOW);
	waddstr(BOTTOM_WINDOW, prompt);
	wgetnstr(BOTTOM_WINDOW, buf, sizeof(buf));
	wclear(BOTTOM_WINDOW);
	wrefresh(BOTTOM_WINDOW);
	setwnd(LOWER_WINDOW);
    } else {
	putchar('\n');
	proutn(prompt);
	fgets(buf, sizeof(buf), stdin);
	if (i != 0) {
	    int j;
	    for (j = 0; j < 24; j++)
		putchar('\n');
	}
	linecount = 0;
    }
}


void skip(int i) 
{
    while (i-- > 0) {
	if (curses) {
	    proutn("\n\r");
	} else {
	    linecount++;
	    if (linecount >= 24)
		pause_game(0);
	    else
		putchar('\n');
	}
    }
}

static void vproutn(char *fmt, va_list ap) 
{
    if (curses) {
	vwprintw(curwnd, fmt, ap);
	wrefresh(curwnd);
    }
    else
	vprintf(fmt, ap);
}

void proutn(char *fmt, ...) 
{
    va_list ap;
    va_start(ap, fmt);
    vproutn(fmt, ap);
    va_end(ap);
}

void prout(char *fmt, ...) 
{
    va_list ap;
    va_start(ap, fmt);
    vproutn(fmt, ap);
    va_end(ap);
    skip(1);
}

void prouts(char *fmt, ...) 
/* print slowly! */
{
    clock_t endTime;
    char *s, buf[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    skip(1);
    for (s = buf; *s; s++) {
	endTime = clock() + CLOCKS_PER_SEC*0.05;
	while (clock() < endTime) continue;
	if (curses) {
	    waddch(curwnd, *s);
	    wrefresh(curwnd);
	}
	else {
	    putchar(*s);
	    fflush(stdout);
	}
    }
}

void cgetline(char *line, int max)
{
    if (curses) {
	wgetnstr(curwnd, line, max);
	strcat(line, "\n");
	wrefresh(curwnd);
    } else {
	fgets(line, max, stdin);
    }
    line[strlen(line)-1] = '\0';
}

void setwnd(WINDOW *wnd)
/* change windows -- OK for this to be a no-op in tty mode */
{
    if (curses) {
     curwnd=wnd;
     curs_set(wnd == FULLSCREEN_WINDOW || wnd == LOWER_WINDOW);
    }
}

void clreol (void)
/* clear to end of line -- can be a no-op in tty mode */
{
   if (curses) {
       wclrtoeol(curwnd);
       wrefresh(curwnd);
   }
}

void clrscr (void)
/* clear screen -- can be a no-op in tty mode */
{
   if (curses) {
       wclear(curwnd);
       wmove(curwnd,0,0);
       wrefresh(curwnd);
   }
}

void textcolor (int color)
{
    if (curses) {
	wattroff(curwnd, A_REVERSE);
	// FIXME
   }
}

void highvideo (void)
{
    if (curses) {
	attron(A_REVERSE);
    }
}
 
void commandhook(char *cmd, int before) {
}

/*
 * Things past this point have policy implications.
 */

void drawmaps(short l)
/* hook to be called after moving to redraw maps */
{
    if (curses) {
	if (l == 1) 
	    sensor();
	if (l != 2) {
	    setwnd(SRSCAN_WINDOW);
	    wmove(curwnd, 0, 0);
	    enqueue("no");
	    srscan(SCAN_FULL);
	    setwnd(REPORT_WINDOW);
	    wclear(REPORT_WINDOW);
	    wmove(REPORT_WINDOW, 0, 0);
	    srscan(SCAN_NO_LEFTSIDE);
	    setwnd(LRSCAN_WINDOW);
	    wclear(LRSCAN_WINDOW);
	    wmove(LRSCAN_WINDOW, 0, 0);
	    enqueue("l");
	    lrscan();
	}
    }
}

void boom(int ii, int jj)
/* enemy fall down, go boom */ 
{
    if (curses) {
	setwnd(SRSCAN_WINDOW);
	drawmaps(2);
	wmove(SRSCAN_WINDOW, ii*2+3, jj+2);
	wattron(SRSCAN_WINDOW, A_REVERSE);
	waddch(SRSCAN_WINDOW, game.quad[ii][jj]);
	wrefresh(SRSCAN_WINDOW);
	sound(500);
	delay(1000);
	nosound();
	wmove(SRSCAN_WINDOW, ii*2+3, jj+2);
	wattroff(SRSCAN_WINDOW, A_REVERSE);
	waddch(SRSCAN_WINDOW, game.quad[ii][jj]);
	wrefresh(SRSCAN_WINDOW);
	setwnd(LOWER_WINDOW);
	delay(500);
    }
} 

void warble(void)
/* sound and visual effects for teleportation */
{
    if (curses) {
	drawmaps(1);
	setwnd(LOWER_WINDOW);
	sound(50);
	delay(1000);
	nosound();
    } else
	prouts(" . . . . . ");
}

void tracktorpedo(int x, int y, int ix, int iy, int wait, int l, int i, int n, int iquad)
/* torpedo-track animation */
{
    if (!curses) {
	if (l == 1) {
	    if (n != 1) {
		skip(1);
		proutn("Track for torpedo number %d-  ", i);
	    }
	    else {
		skip(1);
		proutn("Torpedo track- ");
	    }
	} else if (l==4 || l==9) 
	    skip(1);
	proutn("%d - %d   ", (int)x, (int)y);
    } else {
	if (game.damage[DSRSENS]==0 || condit==IHDOCKED) {
	    drawmaps(2);
	    delay((wait!=1)*400);
	    if ((game.quad[ix][iy]==IHDOT)||(game.quad[ix][iy]==IHBLANK)){
		game.quad[ix][iy]='+';
		drawmaps(2);
		game.quad[ix][iy]=iquad;
		sound(l*10);
		delay(100);
		nosound();
	    }
	    else {
		game.quad[ix][iy] |= DAMAGED;
		drawmaps(2);
		game.quad[ix][iy]=iquad;
		sound(500);
		delay(1000);
		nosound();
		wattroff(curwnd, A_REVERSE);
	    }
	} else {
	    proutn("%d - %d   ", (int)x, (int)y);
	}
    }
}

void makechart(void) 
{
    if (curses) {
	setwnd(LOWER_WINDOW);
	wclear(LOWER_WINDOW);
	chart(0);
    }
}

void setpassword(void) 
{
    if (!curses) {
	while (TRUE) {
	    scan();
	    strcpy(game.passwd, citem);
	    chew();
	    if (*game.passwd != 0) break;
	    proutn("Please type in a secret password-");
	}
    } else {
	int i;
        for(i=0;i<3;i++) game.passwd[i]=(char)(97+(int)(Rand()*25));
        game.passwd[3]=0;
    }
}

