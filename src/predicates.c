/* $Header$ */

#include "copyright.h"
#include "config.h"

/* Predicates for testing various conditions */

#include <ctype.h>

#include "db.h"
#include "props.h"
#include "interface.h"
#include "params.h"
#include "tune.h"
#include "externs.h"

int
OkObj(dbref obj)
{
	if (obj < 0 || obj >= db_top) {
		return 0;
	}
	if (Typeof(obj) == TYPE_GARBAGE) {
		return 0;
	}
	return 1;
}

int
can_link_to(dbref who, object_flag_type what_type, dbref where)
{
	if (where == HOME)
		return 1;
	if (where < 0 || where >= db_top)
		return 0;
	switch (what_type) {
	case TYPE_EXIT:
		return (controls(who, where) || (FLAGS(where) & LINK_OK));
		/* NOTREACHED */
		break;
	case TYPE_PLAYER:
		return (Typeof(where) == TYPE_ROOM && (controls(who, where)
											   || Linkable(where)));
		/* NOTREACHED */
		break;
	case TYPE_ROOM:
		return ((Typeof(where) == TYPE_ROOM || Typeof(where) == TYPE_THING)
				&& (controls(who, where) || Linkable(where)));
		/* NOTREACHED */
		break;
	case TYPE_THING:
		return ((Typeof(where) == TYPE_ROOM || Typeof(where) == TYPE_PLAYER ||
				 Typeof(where) == TYPE_THING) && (controls(who, where) || Linkable(where)));
		/* NOTREACHED */
		break;
	case NOTYPE:
		return (controls(who, where) || (FLAGS(where) & LINK_OK) ||
				(Typeof(where) != TYPE_THING && (FLAGS(where) & ABODE)));
		/* NOTREACHED */
		break;
	}
	return 0;
}

int
can_link(dbref who, dbref what)
{
	return (controls(who, what) || ((Typeof(what) == TYPE_EXIT)
									&& DBFETCH(what)->sp.exit.ndest == 0));
}

/*
 * Revision 1.2 -- SECURE_TELEPORT
 * you can only jump with an action from rooms that you own
 * or that are jump_ok, and you cannot jump to players that are !jump_ok.
 */

int
could_doit(int descr, dbref player, dbref thing)
{
	dbref source, dest, owner;

	if (Typeof(thing) == TYPE_EXIT) {
		if (DBFETCH(thing)->sp.exit.ndest == 0) {
			return 0;
		}

		owner = OWNER(thing);
		source = DBFETCH(player)->location;
		dest = *(DBFETCH(thing)->sp.exit.dest);

		if (Typeof(dest) == TYPE_PLAYER) {
			dbref destplayer = dest;

			dest = DBFETCH(dest)->location;
			if (!(FLAGS(destplayer) & JUMP_OK) || (FLAGS(dest) & BUILDER)) {
				return 0;
			}
		}

		/* for actions */
		if ((DBFETCH(thing)->location != NOTHING) &&
			(Typeof(DBFETCH(thing)->location) != TYPE_ROOM)) {

			if ((Typeof(dest) == TYPE_ROOM || Typeof(dest) == TYPE_PLAYER) &&
				(FLAGS(source) & BUILDER))
				return 0;

			if (tp_secure_teleport && Typeof(dest) == TYPE_ROOM) {
				if ((dest != HOME) && (!controls(owner, source))
					&& ((FLAGS(source) & JUMP_OK) == 0)) {
					return 0;
				}
			}
		}
	}

	return (eval_boolexp(descr, player, GETLOCK(thing), thing));
}

int
test_lock(int descr, dbref player, dbref thing, const char *lockprop)
{
	struct boolexp *lokptr;

	lokptr = get_property_lock(thing, lockprop);
	return (eval_boolexp(descr, player, lokptr, thing));
}

int
test_lock_false_default(int descr, dbref player, dbref thing, const char *lockprop)
{
	struct boolexp *lok = get_property_lock(thing, lockprop);

	if (lok == TRUE_BOOLEXP)
		return 0;
	return (eval_boolexp(descr, player, lok, thing));
}

int
can_doit(int descr, dbref player, dbref thing, const char *default_fail_msg)
{
	dbref loc;

	if ((loc = getloc(player)) == NOTHING)
		return 0;

	if (!Wizard(OWNER(player)) && Typeof(player) == TYPE_THING && (FLAGS(thing) & ZOMBIE)) {
		notify(player, "Sorry, but zombies can't do that.");
		return 0;
	}
	if (!could_doit(descr, player, thing)) {
		/* can't do it */
		if (GETFAIL(thing)) {
			exec_or_notify_prop(descr, player, thing, MESGPROP_FAIL, "(@Fail)");
		} else if (default_fail_msg) {
			notify(player, default_fail_msg);
		}
		if (GETOFAIL(thing) && !Dark(player)) {
			parse_oprop(descr, player, loc, thing, MESGPROP_OFAIL, NAME(player), "(@Ofail)");
		}
		return 0;
	} else {
		/* can do it */
		if (GETSUCC(thing)) {
			exec_or_notify_prop(descr, player, thing, MESGPROP_SUCC, "(@Succ)");
		}
		if (GETOSUCC(thing) && !Dark(player)) {
			parse_oprop(descr, player, loc, thing, MESGPROP_OSUCC, NAME(player), "(@Osucc)");
		}
		return 1;
	}
}

int
can_see(dbref player, dbref thing, int can_see_loc)
{
	if (player == thing || Typeof(thing) == TYPE_EXIT || Typeof(thing) == TYPE_ROOM)
		return 0;

	if (can_see_loc) {
		switch (Typeof(thing)) {
		case TYPE_PROGRAM:
			return ((FLAGS(thing) & LINK_OK) || controls(player, thing));
		case TYPE_PLAYER:
			if (tp_dark_sleepers) {
				return (!Dark(thing) && online(thing));
			}
		default:
			return (!Dark(thing) || (controls(player, thing) && !(FLAGS(player) & STICKY)));
		}
	} else {
		/* can't see loc */
		return (controls(player, thing) && !(FLAGS(player) & STICKY));
	}
}

int
controls(dbref who, dbref what)
{
	dbref index;

	/* No one controls invalid objects */
	if (what < 0 || what >= db_top)
		return 0;

	/* No one controls garbage */
	if (Typeof(what) == TYPE_GARBAGE)
		return 0;

	if (Typeof(who) != TYPE_PLAYER)
		who = OWNER(who);

	/* Wizard controls everything else */
	if (Wizard(who))
		return 1;

	if (tp_realms_control) {
		/* Realm Owner controls everything under his environment. */
		for (index = what; index != NOTHING; index = getloc(index)) {
			if ((OWNER(index) == who) && (Typeof(index) == TYPE_ROOM)
				&& Wizard(index))
				return 1;
		}
	}

	/* exits are also controlled by the owners of the source and destination */
	/* ACTUALLY, THEY AREN'T.  IT OPENS A BAD MPI SECURITY HOLE. */
	/*
	 * if (Typeof(what) == TYPE_EXIT) {
	 *    int     i = DBFETCH(what)->sp.exit.ndest;
	 *
	 *    while (i > 0) {
	 *        if (who == OWNER(DBFETCH(what)->sp.exit.dest[--i]))
	 *            return 1;
	 *    }
	 *    if (who == OWNER(DBFETCH(what)->location))
	 *        return 1;
	 * }
	 */

	/* owners control their own stuff */
	return (who == OWNER(what));
}

int
restricted(dbref player, dbref thing, object_flag_type flag)
{
	switch (flag) {
	case ABODE:
		return (!TrueWizard(OWNER(player)) && (Typeof(thing) == TYPE_PROGRAM));
		/* NOTREACHED */
		break;
	case ZOMBIE:
		if (Typeof(thing) == TYPE_PLAYER)
			return (!(Wizard(OWNER(player))));
		if ((Typeof(thing) == TYPE_THING) && (FLAGS(OWNER(player)) & ZOMBIE))
			return (!(Wizard(OWNER(player))));
		return (0);
	case VEHICLE:
		if (Typeof(thing) == TYPE_PLAYER)
			return (!(Wizard(OWNER(player))));
		if (tp_wiz_vehicles) {
			if (Typeof(thing) == TYPE_THING)
				return (!(Wizard(OWNER(player))));
		} else {
			if ((Typeof(thing) == TYPE_THING) && (FLAGS(player) & VEHICLE))
				return (!(Wizard(OWNER(player))));
		}
		return (0);
	case DARK:
		if (!Wizard(OWNER(player))) {
			if (Typeof(thing) == TYPE_PLAYER)
				return (1);
			if (!tp_exit_darking && Typeof(thing) == TYPE_EXIT)
				return (1);
			if (!tp_thing_darking && Typeof(thing) == TYPE_THING)
				return (1);
		}
		return (0);

		/* NOTREACHED */
		break;
	case QUELL:
		/* You cannot quell or unquell another wizard. */
		return (TrueWizard(thing) && (thing != player) && (Typeof(thing) == TYPE_PLAYER));
		/* NOTREACHED */
		break;
	case MUCKER:
	case SMUCKER:
	case BUILDER:
		return (!Wizard(OWNER(player)));
		/* NOTREACHED */
		break;
	case WIZARD:
		if (Wizard(OWNER(player))) {

#ifdef GOD_PRIV
			return ((Typeof(thing) == TYPE_PLAYER) && !God(player));
#else							/* !GOD_PRIV */
			return 0;
#endif							/* GOD_PRIV */
		} else
			return 1;
		/* NOTREACHED */
		break;
	default:
		return 0;
		/* NOTREACHED */
		break;
	}
}

int
payfor(dbref who, int cost)
{
	who = OWNER(who);
	if (Wizard(who)) {
		return 1;
	} else if (GETVALUE(who) >= cost) {
		SETVALUE(who, GETVALUE(who) - cost);
		DBDIRTY(who);
		return 1;
	} else {
		return 0;
	}
}

int
word_start(const char *str, const char let)
{
	int chk;

	for (chk = 1; *str; str++) {
		if (chk && *str == let)
			return 1;
		chk = *str == ' ';
	}
	return 0;
}

int
ok_name(const char *name)
{
	return (name && *name && *name != LOOKUP_TOKEN && *name != REGISTERED_TOKEN &&
			*name != NUMBER_TOKEN && !index(name, ARG_DELIMITER)
			&& !index(name, AND_TOKEN)
			&& !index(name, OR_TOKEN)
			&& !index(name, '\r')
			&& !index(name, ESCAPE_CHAR)
			&& !word_start(name, NOT_TOKEN)
			&& string_compare(name, "me")
			&& string_compare(name, "home")
			&& string_compare(name, "here")
			&& (!*tp_reserved_names || !equalstr((char *) tp_reserved_names, (char *) name)
			));
}

int
ok_player_name(const char *name)
{
	const char *scan;

	if (!ok_name(name) || strlen(name) > PLAYER_NAME_LIMIT)
		return 0;

	for (scan = name; *scan; scan++) {
		if (!(isprint(*scan) && !isspace(*scan)) && *scan != '(' && *scan != ')') {
			/* was isgraph(*scan) */
			return 0;
		}
	}

	/* Check the name isn't reserved */
	if (*tp_reserved_player_names &&
		equalstr((char *) tp_reserved_player_names, (char *) name))
		return 0;

	/* lookup name to avoid conflicts */
	return (lookup_player(name) == NOTHING);
}

int
ok_password(const char *password)
{
	const char *scan;

	if (*password == '\0')
		return 0;

	for (scan = password; *scan; scan++) {
		if (!(isprint(*scan) && !isspace(*scan))) {
			return 0;
		}
	}

	return 1;
}

int
isancestor(dbref parent, dbref child)
{
	while (child != NOTHING && child != parent) {
		child = getparent(child);
	}
	return child == parent;
}

static dbref
getparent_logic(dbref obj)
{
	if (obj == NOTHING)
		return NOTHING;
	if (Typeof(obj) == TYPE_THING && (FLAGS(obj) & VEHICLE)) {
		obj = THING_HOME(obj);
		if (obj != NOTHING && Typeof(obj) == TYPE_PLAYER) {
			obj = PLAYER_HOME(obj);
		}
	} else {
		obj = getloc(obj);
	}
	return obj;
}

dbref
getparent(dbref obj)
{
	dbref ptr, oldptr;

	if (tp_thing_movement) {
		obj = getloc(obj);
	} else {
		ptr = getparent_logic(obj);
		do {
			obj = getparent_logic(obj);
		} while (obj != (oldptr = ptr = getparent_logic(ptr)) &&
				 obj != (ptr = getparent_logic(ptr)) && obj != NOTHING &&
				 Typeof(obj) == TYPE_THING);
		if (obj != NOTHING && (obj == oldptr || obj == ptr)) {
			obj = GLOBAL_ENVIRONMENT;
		}
	}
	return obj;
}
