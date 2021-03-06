/* $Header$ */

#include "copyright.h"
#include "config.h"
#include "params.h"

#include "db.h"
#include "tune.h"
#include "mpi.h"
#include "props.h"
#include "externs.h"
#include "interface.h"
#include <string.h>
#include <math.h>

/* property.c
   A whole new lachesis mod.
   Adds property manipulation routines to TinyMUCK.   */

/* Completely rewritten by darkfox and Foxen, for propdirs and other things */



void
set_property_nofetch(dbref player, const char *pname, PData * dat)
{
	PropPtr p;
	char buf[BUFFER_LEN];
	char *n, *w;

	/* Make sure that we are passed a valid property name */
	if (!pname)
		return;

	while (*pname == PROPDIR_DELIMITER)
		pname++;
	if ((!(FLAGS(player) & LISTENER)) &&
		(string_prefix(pname, "_listen") ||
		 string_prefix(pname, "~listen") || string_prefix(pname, "~olisten"))) {
		FLAGS(player) |= LISTENER;
	}

	w = strcpyn(buf, sizeof(buf), pname);

	/* truncate propnames with a ':' in them at the ':' */
	n = index(buf, PROP_DELIMITER);
	if (n)
		*n = '\0';
	if (!*buf)
		return;

	p = propdir_new_elem(&(DBFETCH(player)->properties), w);

	/* free up any old values */
	clear_propnode(p);

	SetPFlagsRaw(p, dat->flags);
	if (PropFlags(p) & PROP_ISUNLOADED) {
		SetPDataUnion(p, dat->data);
		return;
	}
	switch (PropType(p)) {
	case PROP_STRTYP:
		if (!dat->data.str || !*(dat->data.str)) {
			SetPType(p, PROP_DIRTYP);
			SetPDataStr(p, NULL);
			if (!PropDir(p)) {
				remove_property_nofetch(player, pname);
			}
		} else {
			SetPDataStr(p, alloc_string(dat->data.str));
		}
		break;
	case PROP_INTTYP:
		SetPDataVal(p, dat->data.val);
		if (!dat->data.val) {
			SetPType(p, PROP_DIRTYP);
			if (!PropDir(p)) {
				remove_property_nofetch(player, pname);
			}
		}
		break;
	case PROP_FLTTYP:
		SetPDataFVal(p, dat->data.fval);
		if (dat->data.fval == 0.0) {
			SetPType(p, PROP_DIRTYP);
			if (!PropDir(p)) {
				remove_property_nofetch(player, pname);
			}
		}
		break;
	case PROP_REFTYP:
		SetPDataRef(p, dat->data.ref);
		if (dat->data.ref == NOTHING) {
			SetPType(p, PROP_DIRTYP);
			SetPDataRef(p, 0);
			if (!PropDir(p)) {
				remove_property_nofetch(player, pname);
			}
		}
		break;
	case PROP_LOKTYP:
		SetPDataLok(p, dat->data.lok);
		break;
	case PROP_DIRTYP:
		SetPDataVal(p, 0);
		if (!PropDir(p)) {
			remove_property_nofetch(player, pname);
		}
		break;
	}
}


void
set_property(dbref player, const char *name, PData * dat)
{
#ifdef DISKBASE
	fetchprops(player, propdir_name(name));
	set_property_nofetch(player, name, dat);
	dirtyprops(player);
#else
	set_property_nofetch(player, name, dat);
#endif
	DBDIRTY(player);
}

void
set_lock_property(dbref player, const char *pname, const char *lok)
{
	PData mydat;

	mydat.flags = PROP_LOKTYP;
	if (!lok || !*lok) {
		mydat.data.lok = TRUE_BOOLEXP;
	} else {
		mydat.data.lok = parse_boolexp(-1, (dbref) 1, lok, 1);
	}
	set_property(player, pname, &mydat);
}



/* adds a new property to an object */
void
add_prop_nofetch(dbref player, const char *pname, const char *strval, int value)
{
	PData mydat;

	if (strval && *strval) {
		mydat.flags = PROP_STRTYP;
		mydat.data.str = (char *) strval;
	} else if (value) {
		mydat.flags = PROP_INTTYP;
		mydat.data.val = value;
	} else {
		mydat.flags = PROP_DIRTYP;
		mydat.data.str = NULL;
	}
	set_property_nofetch(player, pname, &mydat);
}


/* adds a new property to an object */
void
add_property(dbref player, const char *pname, const char *strval, int value)
{

#ifdef DISKBASE
	fetchprops(player, propdir_name(pname));
	add_prop_nofetch(player, pname, strval, value);
	dirtyprops(player);
#else
	add_prop_nofetch(player, pname, strval, value);
#endif
	DBDIRTY(player);
}


void
remove_proplist_item(dbref player, PropPtr p, int allp)
{
	const char *ptr;

	if (!p)
		return;
	ptr = PropName(p);

	if (Prop_System(ptr))
		return;

	if (!allp) {
		if (*ptr == PROP_HIDDEN)
			return;
		if (*ptr == PROP_SEEONLY)
			return;
		if (ptr[0] == '_' && ptr[1] == '\0')
			return;
		if (PropFlags(p) & PROP_SYSPERMS)
			return;
	}
	remove_property(player, ptr);
}


/* removes property list --- if it's not there then ignore */
void
remove_property_list(dbref player, int all)
{
	PropPtr l;
	PropPtr p;
	PropPtr n;

#ifdef DISKBASE
	fetchprops(player, NULL);
#endif

	if ((l = DBFETCH(player)->properties) != NULL) {
		p = first_node(l);
		while (p) {
			n = next_node(l, PropName(p));
			remove_proplist_item(player, p, all);
			l = DBFETCH(player)->properties;
			p = n;
		}
	}

#ifdef DISKBASE
	dirtyprops(player);
#endif

	DBDIRTY(player);
}

/* removes property --- if it's not there then ignore */
void
remove_property_nofetch(dbref player, const char *pname)
{
	PropPtr l;
	char buf[BUFFER_LEN];
	char *w;

	w = strcpyn(buf, sizeof(buf), pname);

	l = DBFETCH(player)->properties;
	l = propdir_delete_elem(l, w);
	DBFETCH(player)->properties = l;
	DBDIRTY(player);
}


void
remove_property(dbref player, const char *pname)
{
#ifdef DISKBASE
	fetchprops(player, propdir_name(pname));
#endif

	remove_property_nofetch(player, pname);

#ifdef DISKBASE
	dirtyprops(player);
#endif
}


PropPtr
get_property(dbref player, const char *pname)
{
	PropPtr p;
	char buf[BUFFER_LEN];
	char *w;

#ifdef DISKBASE
	fetchprops(player, propdir_name(pname));
#endif

	w = strcpyn(buf, sizeof(buf), pname);

	p = propdir_get_elem(DBFETCH(player)->properties, w);
	return (p);
}


/* checks if object has property, returning 1 if it or any of its contents has
   the property stated                                                      */
int
has_property(int descr, dbref player, dbref what, const char *pname, const char *strval,
			 int value)
{
	dbref things;

	if (has_property_strict(descr, player, what, pname, strval, value))
		return 1;
	for (things = DBFETCH(what)->contents; things != NOTHING; things = DBFETCH(things)->next) {
		if (has_property(descr, player, things, pname, strval, value))
			return 1;
	}
	if (tp_lock_envcheck) {
		things = getparent(what);
		while (things != NOTHING) {
			if (has_property_strict(descr, player, things, pname, strval, value))
				return 1;
			things = getparent(things);
		}
	}
	return 0;
}


static int has_prop_recursion_limit = 2;
/* checks if object has property, returns 1 if it has the property */
int
has_property_strict(int descr, dbref player, dbref what, const char *pname, const char *strval,
					int value)
{
	PropPtr p;
	const char *str;
	char *ptr;
	char buf[BUFFER_LEN];

	p = get_property(what, pname);

	if (p) {
#ifdef DISKBASE
		propfetch(what, p);
#endif
		switch (PropType(p)) {
		    case PROP_STRTYP:
			str = DoNull(PropDataStr(p));

			if (has_prop_recursion_limit-->0) {
				ptr = do_parse_mesg(descr, player, what, str, "(Lock)", buf, sizeof(buf),
									(MPI_ISPRIVATE | MPI_ISLOCK |
									((PropFlags(p) & PROP_BLESSED)? MPI_ISBLESSED : 0)));
			} else {
				strcpyn(buf, sizeof(buf), str); 
				ptr = buf;
			}
			has_prop_recursion_limit++;
			return (equalstr((char *) strval, ptr));

		    case PROP_INTTYP:
			return (value == PropDataVal(p));
		    case PROP_FLTTYP:
			return (value == (int) PropDataFVal(p));
		    default:
			/* assume other types don't match */
			return 0;
		}
	}
	return 0;
}

/* return string value of property */
const char *
get_property_class(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);
	if (p) {
#ifdef DISKBASE
		propfetch(player, p);
#endif
		if (PropType(p) != PROP_STRTYP)
			return (char *) NULL;
		return (PropDataStr(p));
	} else {
		return (char *) NULL;
	}
}

/* return value of property */
int
get_property_value(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);

	if (p) {
#ifdef DISKBASE
		propfetch(player, p);
#endif
		if (PropType(p) != PROP_INTTYP)
			return 0;
		return (PropDataVal(p));
	} else {
		return 0;
	}
}

/* return float value of a property */
double
get_property_fvalue(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);
	if (p) {
#ifdef DISKBASE
		propfetch(player, p);
#endif
		if (PropType(p) != PROP_FLTTYP)
			return 0.0;
		return (PropDataFVal(p));
	} else {
		return 0.0;
	}
}

/* return boolexp lock of property */
dbref
get_property_dbref(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);
	if (!p)
		return NOTHING;
#ifdef DISKBASE
	propfetch(player, p);
#endif
	if (PropType(p) != PROP_REFTYP)
		return NOTHING;
	return PropDataRef(p);
}


/* return boolexp lock of property */
struct boolexp *
get_property_lock(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);
	if (!p)
		return TRUE_BOOLEXP;
#ifdef DISKBASE
	propfetch(player, p);
	if (PropFlags(p) & PROP_ISUNLOADED)
		return TRUE_BOOLEXP;
#endif
	if (PropType(p) != PROP_LOKTYP)
		return TRUE_BOOLEXP;
	return PropDataLok(p);
}


/* return flags of property */
int
get_property_flags(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);

	if (p) {
		return (PropFlags(p));
	} else {
		return 0;
	}
}


/* return flags of property */
void
clear_property_flags(dbref player, const char *pname, int flags)
{
	PropPtr p;

	flags &= ~PROP_TYPMASK;
	p = get_property(player, pname);
	if (p) {
		SetPFlags(p, (PropFlags(p) & ~flags));
#ifdef DISKBASE
		dirtyprops(player);
#endif
	}
}


/* return flags of property */
void
set_property_flags(dbref player, const char *pname, int flags)
{
	PropPtr p;

	flags &= ~PROP_TYPMASK;
	p = get_property(player, pname);
	if (p) {
		SetPFlags(p, (PropFlags(p) | flags));
#ifdef DISKBASE
		dirtyprops(player);
#endif
	}
}


/* return type of property */
int
get_property_type(dbref player, const char *pname)
{
	PropPtr p;

	p = get_property(player, pname);

	if (p) {
		return (PropType(p));
	} else {
		return 0;
	}
}



PropPtr
copy_prop(dbref old)
{
	PropPtr p, n = NULL;

#ifdef DISKBASE
	fetchprops(old, NULL);
#endif

	p = DBFETCH(old)->properties;
	copy_proplist(old, &n, p);
	return (n);
}

/* Return a pointer to the first property in a propdir and duplicates the
   property name into 'name'.  Returns NULL if the property list is empty
   or does not exist. */
PropPtr
first_prop_nofetch(dbref player, const char *dir, PropPtr * list, char *name, int maxlen)
{
	char buf[BUFFER_LEN];
	PropPtr p;

	if (dir) {
		while (*dir && *dir == PROPDIR_DELIMITER) {
			dir++;
		}
	}
	if (!dir || !*dir) {
		*list = DBFETCH(player)->properties;
		p = first_node(*list);
		if (p) {
			strcpyn(name, maxlen, PropName(p));
		} else {
			*name = '\0';
		}
		return (p);
	}

	strcpyn(buf, sizeof(buf), dir);
	*list = p = propdir_get_elem(DBFETCH(player)->properties, buf);
	if (!p) {
		*name = '\0';
		return NULL;
	}
	*list = PropDir(p);
	p = first_node(*list);
	if (p) {
		strcpyn(name, maxlen, PropName(p));
	} else {
		*name = '\0';
	}

	return (p);
}


/* first_prop() returns a pointer to the first property.
 * player    dbref of object that the properties are on.
 * dir       pointer to string name of the propdir
 * list      pointer to a proplist pointer.  Returns the root node.
 * name      printer to a string.  Returns the name of the first node.
 */

PropPtr
first_prop(dbref player, const char *dir, PropPtr * list, char *name, int maxlen)
{

#ifdef DISKBASE
	fetchprops(player, (char *) dir);
#endif

	return (first_prop_nofetch(player, dir, list, name, maxlen));
}



/* next_prop() returns a pointer to the next property node.
 * list    Pointer to the root node of the list.
 * prop    Pointer to the previous prop.
 * name    Pointer to a string.  Returns the name of the next property.
 */

PropPtr
next_prop(PropPtr list, PropPtr prop, char *name, int maxlen)
{
	PropPtr p = prop;

	if (!p || !(p = next_node(list, PropName(p))))
		return ((PropPtr) 0);

	strcpyn(name, maxlen, PropName(p));
	return (p);
}



/* next_prop_name() returns a ptr to the string name of the next property.
 * player   object the properties are on.
 * outbuf   pointer to buffer to return the next prop's name in.
 * name     pointer to the name of the previous property.
 *
 * Returns null if propdir doesn't exist, or if no more properties in list.
 * Call with name set to "" to get the first property of the root propdir.
 */

char *
next_prop_name(dbref player, char *outbuf, int outbuflen, char *name)
{
	char *ptr;
	char buf[BUFFER_LEN];
	PropPtr p, l;

#ifdef DISKBASE
	fetchprops(player, propdir_name(name));
#endif

	strcpyn(buf, sizeof(buf), name);
	if (!*name || name[strlen(name) - 1] == PROPDIR_DELIMITER) {
		l = DBFETCH(player)->properties;
		p = propdir_first_elem(l, buf);
		if (!p) {
			*outbuf = '\0';
			return NULL;
		}
		strcpyn(outbuf, outbuflen, name);
		strcatn(outbuf, outbuflen, PropName(p));
	} else {
		l = DBFETCH(player)->properties;
		p = propdir_next_elem(l, buf);
		if (!p) {
			*outbuf = '\0';
			return NULL;
		}
		strcpyn(outbuf, outbuflen, name);
		ptr = rindex(outbuf, PROPDIR_DELIMITER);
		if (!ptr)
			ptr = outbuf;
		*(ptr++) = PROPDIR_DELIMITER;
		strcpyn(ptr, outbuflen - (ptr-outbuf), PropName(p));
	}
	return outbuf;
}


long
size_properties(dbref player, int load)
{
#ifdef DISKBASE
	if (load) {
		fetchprops(player, NULL);
		fetch_propvals(player, "/");
	}
#endif
	return size_proplist(DBFETCH(player)->properties);
}


/* return true if a property contains a propdir */
int
is_propdir_nofetch(dbref player, const char *pname)
{
	PropPtr p;
	char w[BUFFER_LEN];

	strcpyn(w, sizeof(w), pname);
	p = propdir_get_elem(DBFETCH(player)->properties, w);
	if (!p)
		return 0;
	return (PropDir(p) != (PropPtr) NULL);
}


int
is_propdir(dbref player, const char *pname)
{

#ifdef DISKBASE
	fetchprops(player, propdir_name(pname));
#endif

	return (is_propdir_nofetch(player, pname));
}


PropPtr
envprop(dbref * where, const char *propname, int typ)
{
	PropPtr temp;

	while (*where != NOTHING) {
		temp = get_property(*where, propname);
#ifdef DISKBASE
		if (temp)
			propfetch(*where, temp);
#endif
		if (temp && (!typ || PropType(temp) == typ))
			return temp;
		*where = getparent(*where);
	}
	return NULL;
}


const char *
envpropstr(dbref * where, const char *propname)
{
	PropPtr temp;

	temp = envprop(where, propname, PROP_STRTYP);
	if (!temp)
		return NULL;
	if (PropType(temp) == PROP_STRTYP)
		return (PropDataStr(temp));
	return NULL;
}


char *
displayprop(dbref player, dbref obj, const char *name, char *buf, size_t bufsiz)
{
	char mybuf[BUFFER_LEN];
	int pdflag;
	char blesschar = '-';
	PropPtr p = get_property(obj, name);

	if (!p) {
		snprintf(buf, bufsiz, "%s: No such property.", name);
		return buf;
	}
#ifdef DISKBASE
	propfetch(obj, p);
#endif
	if (PropFlags(p) & PROP_BLESSED)
		blesschar = 'B';
	pdflag = (PropDir(p) != NULL);
	snprintf(mybuf, bufsiz, "%.*s%c", (BUFFER_LEN / 4), name, (pdflag) ? PROPDIR_DELIMITER : '\0');
	switch (PropType(p)) {
	case PROP_STRTYP:
		snprintf(buf, bufsiz, "%c str %s:%.*s", blesschar, mybuf, (BUFFER_LEN / 2), PropDataStr(p));
		break;
	case PROP_REFTYP:
		snprintf(buf, bufsiz, "%c ref %s:%s", blesschar, mybuf, unparse_object(player, PropDataRef(p)));
		break;
	case PROP_INTTYP:
		snprintf(buf, bufsiz, "%c int %s:%d", blesschar, mybuf, PropDataVal(p));
		break;
	case PROP_FLTTYP:
		snprintf(buf, bufsiz, "%c flt %s:%.17g", blesschar, mybuf, PropDataFVal(p));
		break;
	case PROP_LOKTYP:
		if (PropFlags(p) & PROP_ISUNLOADED) {
			snprintf(buf, bufsiz, "%c lok %s:*UNLOCKED*", blesschar, mybuf);
		} else {
			snprintf(buf, bufsiz, "%c lok %s:%.*s", blesschar, mybuf, (BUFFER_LEN / 2),
					unparse_boolexp(player, PropDataLok(p), 1));
		}
		break;
	case PROP_DIRTYP:
		snprintf(buf, bufsiz, "%c dir %s:(no value)", blesschar, mybuf);
		break;
	}
	return buf;
}



#define DOWNCASE(x) (tolower(x))

extern short db_conversion_flag;

int
db_get_single_prop(FILE * f, dbref obj, long pos, PropPtr pnode, const char *pdir)
{
	char getprop_buf[BUFFER_LEN * 3];
	char *name, *flags, *value, *p;
	int flg;
	long tpos=0L;
	struct boolexp *lok;
	short do_diskbase_propvals;
	PData mydat;

#ifdef DISKBASE
	do_diskbase_propvals = tp_diskbase_propvals;
#else
	do_diskbase_propvals = 0;
#endif

	if (pos) {
		fseek(f, pos, 0);
	} else if (do_diskbase_propvals) {
		tpos = ftell(f);
	}
	name = fgets(getprop_buf, sizeof(getprop_buf), f);
	if (!name) {
		wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
		wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
		wall_wizards("##   logfile for technical details.");
		log_sanity("Failed to read property from disk: Failed disk read.  obj = #%d, pos = %ld, pdir = %s", obj, pos, pdir);
		return -1;
	}
	if (*name == '*') {
		if (!strcmp(name, "*End*\n")) {
			return 0;
		}
		if (name[1] == '\n') {
			return 2;
		}
	}

	flags = index(name, PROP_DELIMITER);
	if (!flags) {
		wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
		wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
		wall_wizards("##   logfile for technical details.");
		log_sanity("Failed to read property from disk: Corrupt property, flag delimiter not found.  obj = #%d, pos = %ld, pdir = %s, data = %s", obj, pos, pdir, name);
		return -1;
	}
	*flags++ = '\0';

	value = index(flags, PROP_DELIMITER);
	if (!value) {
		wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
		wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
		wall_wizards("##   logfile for technical details.");
		log_sanity("Failed to read property from disk: Corrupt property, value delimiter not found.  obj = #%d, pos = %ld, pdir = %s, data = %s:%s", obj, pos, pdir, name, flags);
		return -1;
	}
	*value++ = '\0';

	p = index(value, '\n');
	if (p) {
		*p = '\0';
	}

	if (!number(flags)) {
		wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
		wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
		wall_wizards("##   logfile for technical details.");
		log_sanity("Failed to read property from disk: Corrupt property flags.  obj = #%d, pos = %ld, pdir = %s, data = %s:%s:%s", obj, pos, pdir, name, flags, value);
		return -1;
	}
	flg = atoi(flags);

	switch (flg & PROP_TYPMASK) {
	case PROP_STRTYP:
		if (!do_diskbase_propvals || pos) {
			flg &= ~PROP_ISUNLOADED;
			if (pnode) {
				SetPDataStr(pnode, alloc_string(value));
				SetPFlagsRaw(pnode, flg);
			} else {
				mydat.flags = flg;
				mydat.data.str = value;
				set_property_nofetch(obj, name, &mydat);
			}
		} else {
			flg |= PROP_ISUNLOADED;
			mydat.flags = flg;
			mydat.data.val = tpos;
			set_property_nofetch(obj, name, &mydat);
		}
		break;
	case PROP_LOKTYP:
		if (!do_diskbase_propvals || pos) {
			lok = parse_boolexp(-1, (dbref) 1, value, 32767);
			flg &= ~PROP_ISUNLOADED;
			if (pnode) {
				SetPDataLok(pnode, lok);
				SetPFlagsRaw(pnode, flg);
			} else {
				mydat.flags = flg;
				mydat.data.lok = lok;
				set_property_nofetch(obj, name, &mydat);
			}
		} else {
			flg |= PROP_ISUNLOADED;
			mydat.flags = flg;
			mydat.data.val = tpos;
			set_property_nofetch(obj, name, &mydat);
		}
		break;
	case PROP_INTTYP:
		if (!number(value)) {
			wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
			wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
			wall_wizards("##   logfile for technical details.");
			log_sanity("Failed to read property from disk: Corrupt integer value.  obj = #%d, pos = %ld, pdir = %s, data = %s:%s:%s", obj, pos, pdir, name, flags, value);
			return -1;
		}
		mydat.flags = flg;
		mydat.data.val = atoi(value);
		set_property_nofetch(obj, name, &mydat);
		break;
	case PROP_FLTTYP:
		mydat.flags = flg;
		if (!number(value) && !ifloat(value)) {
			char *tpnt = value;
			int dtemp = 0;

			if ((*tpnt == '+') || (*tpnt == '-')) {
				if (*tpnt == '+') {
					dtemp = 0;
				} else {
					dtemp = 1;
				}
				tpnt++;
			}
			tpnt[0] = toupper(tpnt[0]);
			tpnt[1] = toupper(tpnt[1]);
			tpnt[2] = toupper(tpnt[2]);
			if (!strncmp(tpnt, "INF", 3)) {
				if (!dtemp) {
					mydat.data.fval = INF;
				} else {
					mydat.data.fval = NINF;
				}
			} else {
				if (!strncmp(tpnt, "NAN", 3)) {
					/* FIXME: This should be NaN. */
					mydat.data.fval = INF;
				}
			}
		} else {
			sscanf(value, "%lg", &mydat.data.fval);
		}
		set_property_nofetch(obj, name, &mydat);
		break;
	case PROP_REFTYP:
		if (!number(value)) {
			wall_wizards("## WARNING! A corrupt property was found while trying to read it from disk.");
			wall_wizards("##   This property has been skipped and will not be loaded.  See the sanity");
			wall_wizards("##   logfile for technical details.");
			log_sanity("Failed to read property from disk: Corrupt dbref value.  obj = #%d, pos = %ld, pdir = %s, data = %s:%s:%s", obj, pos, pdir, name, flags, value);
			return -1;
		}
		mydat.flags = flg;
		mydat.data.ref = atoi(value);
		set_property_nofetch(obj, name, &mydat);
		break;
	case PROP_DIRTYP:
		break;
	}
	return 1;
}

void
db_getprops(FILE * f, dbref obj, const char *pdir)
{
	while (db_get_single_prop(f, obj, 0L, (PropPtr) NULL, pdir)) ;
}


void
db_putprop(FILE * f, const char *dir, PropPtr p)
{
	char buf[BUFFER_LEN * 2];
	const char *ptr2;
	char tbuf[50];
	int outflags = (PropFlagsRaw(p) & ~(PROP_TOUCHED | PROP_ISUNLOADED | PROP_DIRUNLOADED));

	if (PropType(p) == PROP_DIRTYP)
		return;

	ptr2 = "";
	switch (PropType(p)) {
	case PROP_INTTYP:
		if (!PropDataVal(p))
			return;
		ptr2 = intostr(PropDataVal(p));
		break;
	case PROP_FLTTYP:
		if (!PropDataFVal(p))
			return;
		snprintf(tbuf, sizeof(tbuf), "%.17g", PropDataFVal(p));
		ptr2 = tbuf;
		break;
	case PROP_REFTYP:
		if (PropDataRef(p) == NOTHING)
			return;
		ptr2 = intostr(PropDataRef(p));
		break;
	case PROP_STRTYP:
		if (!*PropDataStr(p))
			return;
		ptr2 = PropDataStr(p);
		break;
	case PROP_LOKTYP:
		if (PropFlags(p) & PROP_ISUNLOADED)
			return;
		if (PropDataLok(p) == TRUE_BOOLEXP)
			return;
		ptr2 = unparse_boolexp((dbref) 1, PropDataLok(p), 0);
		break;
	}

	snprintf(buf, sizeof(buf), "%s%s%c%d%c%s\n",
			dir+1, PropName(p), PROP_DELIMITER,
			outflags, PROP_DELIMITER, ptr2);

	if (fputs(buf, f) == EOF) {
		log_sanity("Failed to write out property db_putprop(dir = %s)", dir);
		abort();
	}
}


int
db_dump_props_rec(dbref obj, FILE * f, const char *dir, PropPtr p)
{
	char buf[BUFFER_LEN];
#ifdef DISKBASE
	long tpos=0L;
	int flg;
	short wastouched = 0;
#endif
	int count = 0;
	int pdcount;

	if (!p)
		return 0;

	count += db_dump_props_rec(obj, f, dir, AVL_LF(p));

#ifdef DISKBASE
	wastouched = (PropFlags(p) & PROP_TOUCHED);
	if (tp_diskbase_propvals) {
		tpos = ftell(f);
	}
	if (wastouched) {
		count++;
	}
	if (propfetch(obj, p)) {
		fseek(f, 0L, 2);
	}
#endif

	db_putprop(f, dir, p);

#ifdef DISKBASE
	if (tp_diskbase_propvals && !wastouched) {
		if (PropType(p) == PROP_STRTYP || PropType(p) == PROP_LOKTYP) {
			flg = PropFlagsRaw(p) | PROP_ISUNLOADED;
			clear_propnode(p);
			SetPFlagsRaw(p, flg);
			SetPDataVal(p, tpos);
		}
	}
#endif

	if (PropDir(p)) {
		const char *iptr;
		char *optr;

		for (iptr = dir, optr = buf; *iptr;)
			*optr++ = *iptr++;
		for (iptr = PropName(p); *iptr;)
			*optr++ = *iptr++;
		*optr++ = PROPDIR_DELIMITER;
		*optr++ = '\0';

		pdcount = db_dump_props_rec(obj, f, buf, PropDir(p));
		count += pdcount;
	}

	count += db_dump_props_rec(obj, f, dir, AVL_RT(p));

	return count;
}


void
db_dump_props(FILE * f, dbref obj)
{
	db_dump_props_rec(obj, f, "/", DBFETCH(obj)->properties);
}


void
untouchprop_rec(PropPtr p)
{
	if (!p)
		return;
	SetPFlags(p, (PropFlags(p) & ~PROP_TOUCHED));
	untouchprop_rec(AVL_LF(p));
	untouchprop_rec(AVL_RT(p));
	untouchprop_rec(PropDir(p));
}

static dbref untouch_lastdone = 0;
void
untouchprops_incremental(int limit)
{
	PropPtr p;

	while (untouch_lastdone < db_top) {
		/* clear the touch flags */
		p = DBFETCH(untouch_lastdone)->properties;
		if (p) {
			if (!limit--)
				return;
			untouchprop_rec(p);
		}
		untouch_lastdone++;
	}
	untouch_lastdone = 0;
}


void
reflist_add(dbref obj, const char* propname, dbref toadd)
{
	PropPtr ptr;
	const char *temp;
	const char *list;
	int count = 0;
	int charcount = 0;
	char buf[BUFFER_LEN];
	char outbuf[BUFFER_LEN];

	ptr = get_property(obj, propname);
	if (ptr) {
		const char *pat = NULL;

#ifdef DISKBASE
		propfetch(obj, ptr);
#endif
		switch (PropType(ptr)) {
		case PROP_STRTYP:
			*outbuf = '\0';
			list = temp = PropDataStr(ptr);
			snprintf(buf, sizeof(buf), "%d", toadd);
			while (*temp) {
				if (*temp == '#') {
					pat = buf;
					count++;
					charcount = temp - list;
				} else if (pat) {
					if (!*pat) {
						if (!*temp || *temp == ' ') {
							break;
						}
						pat = NULL;
					} else if (*pat != *temp) {
						pat = NULL;
					} else {
						pat++;
					}
				}
				temp++;
			}
			if (pat && !*pat) {
				if (charcount > 0) {
					strcpyn(outbuf, charcount, list);
				}
				strcatn(outbuf, sizeof(outbuf), temp);
			} else {
				strcpyn(outbuf, sizeof(outbuf), list);
			}
			snprintf(buf, sizeof(buf), " #%d", toadd);
			if (strlen(outbuf) + strlen(buf) < BUFFER_LEN) {
				strcatn(outbuf, sizeof(outbuf), buf);
				for (temp = outbuf; isspace(*temp); temp++);
				add_property(obj, propname, temp, 0);
			}
			break;
		case PROP_REFTYP:
			if (PropDataRef(ptr) != toadd) {
				snprintf(outbuf, sizeof(outbuf), "#%d #%d", PropDataRef(ptr), toadd);
				add_property(obj, propname, outbuf, 0);
			}
			break;
		default:
			snprintf(outbuf, sizeof(outbuf), "#%d", toadd);
			add_property(obj, propname, outbuf, 0);
			break;
		}
	} else {
		snprintf(outbuf, sizeof(outbuf), "#%d", toadd);
		add_property(obj, propname, outbuf, 0);
	}
}


void
reflist_del(dbref obj, const char* propname, dbref todel)
{
	PropPtr ptr;
	const char *temp;
	const char *list;
	int count = 0;
	int charcount = 0;
	char buf[BUFFER_LEN];
	char outbuf[BUFFER_LEN];

	ptr = get_property(obj, propname);
	if (ptr) {
		const char *pat = NULL;

#ifdef DISKBASE
		propfetch(obj, ptr);
#endif
		switch (PropType(ptr)) {
		case PROP_STRTYP:
			*outbuf = '\0';
			list = temp = PropDataStr(ptr);
			snprintf(buf, sizeof(buf), "%d", todel);
			while (*temp) {
				if (*temp == '#') {
					pat = buf;
					count++;
					charcount = temp - list;
				} else if (pat) {
					if (!*pat) {
						if (!*temp || *temp == ' ') {
							break;
						}
						pat = NULL;
					} else if (*pat != *temp) {
						pat = NULL;
					} else {
						pat++;
					}
				}
				temp++;
			}
			if (pat && !*pat) {
				if (charcount > 0) {
					strcpyn(outbuf, charcount, list);
				}
				strcatn(outbuf, sizeof(outbuf), temp);
				for (temp = outbuf; isspace(*temp); temp++);
				add_property(obj, propname, temp, 0);
			}
			break;
		case PROP_REFTYP:
			if (PropDataRef(ptr) == todel) {
				add_property(obj, propname, "", 0);
			}
			break;
		default:
			break;
		}
	}
}


int
reflist_find(dbref obj, const char* propname, dbref tofind)
{
	PropPtr ptr;
	const char *temp;
	int pos = 0;
	int count = 0;
	char buf[BUFFER_LEN];

	ptr = get_property(obj, propname);
	if (ptr) {
		const char *pat = NULL;

#ifdef DISKBASE
		propfetch(obj, ptr);
#endif
		switch (PropType(ptr)) {
		case PROP_STRTYP:
			temp = PropDataStr(ptr);
			snprintf(buf, sizeof(buf), "%d", tofind);
			while (*temp) {
				if (*temp == '#') {
					pat = buf;
					count++;
				} else if (pat) {
					if (!*pat) {
						if (!*temp || *temp == ' ') {
							break;
						}
						pat = NULL;
					} else if (*pat != *temp) {
						pat = NULL;
					} else {
						pat++;
					}
				}
				temp++;
			}
			if (pat && !*pat) {
				pos = count;
			}
			break;
		case PROP_REFTYP:
			if (PropDataRef(ptr) == tofind)
				pos = 1;
			break;
		default:
			break;
		}
	}
	return pos;
}


static const char *property_c_version = "$RCSfile$ $Revision: 1.30 $";
const char *get_property_c_version(void) { return property_c_version; }
