
/* $Header$
 * $Log: interface.h,v $
 * Revision 1.4  2000/08/12 06:14:17  revar
 * Changed {ontime} and {idle} to refer to the least idle of a users connections.
 * Changed maximum MUF stacksize to 1024 elements.
 * Optimized almost all MUF connection primitives to be O(1) instead of O(n),
 *   by using lookup tables instead of searching a linked list.
 *
 * Revision 1.3  2000/07/18 18:18:19  winged
 * Various fixes to support warning-free compiling with -Wall -Wstrict-prototypes -Wno-format -- added single-inclusion capability to all headers.
 *
 * Revision 1.2  2000/03/29 12:21:01  revar
 * Reformatted all code into consistent format.
 * 	Tabs are 4 spaces.
 * 	Indents are one tab.
 * 	Braces are generally K&R style.
 * Added ARRAY_DIFF, ARRAY_INTERSECT and ARRAY_UNION to man.txt.
 * Rewrote restart script as a bourne shell script.
 *
 * Revision 1.1.1.1  1999/12/12 07:28:12  revar
 * Initial Sourceforge checkin, fb6.00a29
 *
 * Revision 1.1.1.1  1999/12/12 07:28:12  foxen
 * Initial FB6 CVS checkin.
 *
 * Revision 1.1  1996/06/17 17:29:45  foxen
 * Initial revision
 *
 * Revision 5.12  1994/01/06  03:18:09  foxen
 * Version 5.12
 *
 * Revision 5.1  1993/12/17  00:35:54  foxen
 * initial revision.
 *
 * Revision 1.2  90/07/19  23:14:38  casie
 * Removed log comments from top.
 * 
 * 
 */

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "copyright.h"

#include "db.h"
#include "mcp.h"

/* these symbols must be defined by the interface */
extern int notify(dbref player, const char *msg);
extern int notify_nolisten(dbref player, const char *msg, int ispriv);
extern void wall_and_flush(const char *msg);
extern void flush_user_output(dbref player);
extern void wall_wizards(const char *msg);
extern int shutdown_flag;		/* if non-zero, interface should shut down */
extern int restart_flag;		/* if non-zero, should restart after shut down */
extern void emergency_shutdown(void);
extern int boot_off(dbref player);
extern void boot_player_off(dbref player);
extern int online(dbref player);
extern int* get_player_descrs(dbref player, int*count);
extern int least_idle_player_descr(dbref who);
extern int pcount(void);
extern int pidle(int c);
extern int pdbref(int c);
extern int pontime(int c);
extern char *phost(int c);
extern char *puser(int c);
extern int pfirstdescr(dbref who);
extern void pboot(int c);
extern void pnotify(int c, char *outstr);
extern int dbref_first_descr(dbref c);
extern int pdescr(int c);
extern int pdescrcon(int c);
extern McpFrame *descr_mcpframe(int c);
extern int pnextdescr(int c);
extern int pdescrflush(int c);
extern dbref partial_pmatch(const char *name);

/* the following symbols are provided by game.c */

extern void process_command(int descr, dbref player, char *command);

extern dbref create_player(const char *name, const char *password);
extern dbref connect_player(const char *name, const char *password);
extern void do_look_around(int descr, dbref player);

extern int init_game(const char *infile, const char *outfile);
extern void dump_database(void);
extern void panic(const char *);

#endif /* _INTERFACE_H */
