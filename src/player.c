/* $Header$ */

/*
 * $Log: player.c,v $
 * Revision 1.1  1999/12/16 03:23:29  revar
 * Initial revision
 *
 * Revision 1.1.1.1  1999/12/12 07:27:43  foxen
 * Initial FB6 CVS checkin.
 *
 * Revision 1.1  1996/06/12 02:46:29  foxen
 * Initial revision
 *
 * Revision 5.14  1994/03/14  12:20:58  foxen
 * Fb5.20 release checkpoint.
 *
 * Revision 5.13  1994/01/18  20:52:20  foxen
 * Version 5.15 release.
 *
 * Revision 5.12  1994/01/06  03:12:12  foxen
 * version 5.12
 *
 * Revision 5.1  1993/12/17  00:07:33  foxen
 * initial revision.
 *
 * Revision 1.6  90/09/28  12:24:42  rearl
 * Added check for NULL in add_hash().
 *
 * Revision 1.5  90/09/18  08:01:03  rearl
 * Hash functions introduced for player lookup.
 *
 * Revision 1.4  90/09/16  04:42:39  rearl
 * Preparation code added for disk-based MUCK.
 *
 * Revision 1.3  90/08/27  03:32:10  rearl
 * Minor initialization stuff for newly created players.
 *
 * Revision 1.2  90/08/11  04:07:12  rearl
 * *** empty log message ***
 *
 * Revision 1.1  90/07/19  23:03:57  casie
 * Initial revision
 *
 *
 */

#include "copyright.h"
#include "config.h"

#include "db.h"
#include "params.h"
#include "tune.h"
#include "interface.h"
#include "externs.h"

static hash_tab player_list[PLAYER_HASH_SIZE];

dbref 
lookup_player(const char *name)
{
    hash_data *hd;

    if ((hd = find_hash(name, player_list, PLAYER_HASH_SIZE)) == NULL) {
	return NOTHING;
    } else {
	return (hd->dbval);
    }
}

dbref 
connect_player(const char *name, const char *password)
{
    dbref   player;

    if (*name == '#' && number(name+1) && atoi(name+1)) {
	player = (dbref) atoi(name + 1);
	if ((player < 0) || (player >= db_top) || (Typeof(player) != TYPE_PLAYER))
	    player = NOTHING;
    } else {
	player = lookup_player(name);
    }
    if (player == NOTHING)
	return NOTHING;
    if (PLAYER_PASSWORD(player)
	    && *PLAYER_PASSWORD(player)
	    && strcmp(PLAYER_PASSWORD(player), password))
	return NOTHING;

    return player;
}

dbref 
create_player(const char *name, const char *password)
{
    dbref   player;

    if (!ok_player_name(name) || !ok_password(password))
	return NOTHING;

    /* else he doesn't already exist, create him */
    player = new_object();

    /* initialize everything */
    NAME(player) = alloc_string(name);
    DBFETCH(player)->location = tp_player_start;	/* home */
    FLAGS(player) = TYPE_PLAYER | PCREATE_FLAGS;
    OWNER(player) = player;
    ALLOC_PLAYER_SP(player);
    PLAYER_SET_HOME(player, tp_player_start);
    DBFETCH(player)->exits = NOTHING;

    PLAYER_SET_PENNIES(player, tp_start_pennies);
    PLAYER_SET_PASSWORD(player, alloc_string(password));
    PLAYER_SET_CURR_PROG(player, NOTHING);
    PLAYER_SET_INSERT_MODE(player, 0);

    /* link him to tp_player_start */
    PUSH(player, DBFETCH(tp_player_start)->contents);
    add_player(player);
    DBDIRTY(player);
    DBDIRTY(tp_player_start);

    return player;
}

void 
do_password(dbref player, const char *old, const char *newobj)
{
    if (!PLAYER_PASSWORD(player)|| strcmp(old, PLAYER_PASSWORD(player))) {
	notify(player, "Sorry");
    } else if (!ok_password(newobj)) {
	notify(player, "Bad new password.");
    } else {
	free((void *) PLAYER_PASSWORD(player));
	PLAYER_SET_PASSWORD(player, alloc_string(newobj));
	DBDIRTY(player);
	notify(player, "Password changed.");
    }
}

void 
clear_players(void)
{
    kill_hash(player_list, PLAYER_HASH_SIZE, 0);
    return;
}

void 
add_player(dbref who)
{
    hash_data hd;

    hd.dbval = who;
    if (add_hash(NAME(who), hd, player_list, PLAYER_HASH_SIZE) == NULL) {
	panic("Out of memory");
    } else {
	return;
    }
}


void 
delete_player(dbref who)
{
    int result;
    char buf[BUFFER_LEN];
    char namebuf[BUFFER_LEN];
    int i, j;
    dbref found, ren;


    result = free_hash(NAME(who), player_list, PLAYER_HASH_SIZE);

    if (result) {
        wall_wizards("## WARNING: Playername hashtable is inconsistent.  Rebuilding it.  Don't panic.");
        clear_players();
        for (i = 0; i < db_top; i++) {
            if (Typeof(i) == TYPE_PLAYER) {
		found = lookup_player(NAME(i));
		if (found != NOTHING) {
		    ren = (i == who)? found : i;
		    j = 0;
		    do {
		        sprintf(namebuf, "%s%d", NAME(ren), ++j);
		    } while (lookup_player(namebuf) != NOTHING);

		    sprintf(buf,
		            "## Renaming %s(#%d) to %s to prevent name collision.",
			    NAME(ren), ren, namebuf);
		    wall_wizards(buf);

		    log_status("SANITY NAME CHANGE: %s(#%d) to %s\n",
			       NAME(ren), ren, namebuf);

		    if (ren == found) {
			free_hash(NAME(ren), player_list, PLAYER_HASH_SIZE);
		    }
		    if (NAME(ren)) {
			free((void*)NAME(ren));
		    }
		    ts_modifyobject(ren);
		    NAME(ren) = alloc_string(namebuf);
		    add_player(ren);
		} else {
		    add_player(i);
		}
            }
        }
	result = free_hash(NAME(who), player_list, PLAYER_HASH_SIZE);
	if (result) {
	    wall_wizards("## WARNING: Playername hashtable still inconsistent.  Now you can panic.");
	}
    }

    return;
}

