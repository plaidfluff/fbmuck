/*
 *  debugger.c   (sort of a dbx for MUF.)
 */

#include "copyright.h"
#include "config.h"

#include "db.h"
#include "props.h"
#include "interface.h"
#include "inst.h"
#include "externs.h"
#include "params.h"
#include "tune.h"
#include "match.h"
#include "interp.h"
#include <ctype.h>
#include <time.h>


void
list_proglines(dbref player, dbref program, struct frame *fr, int start, int end)
{
    int     range[2];
    int     argc;

    if (start == end || end == 0) {
        range[0] = start;
        range[1] = start;
        argc = 1;
    } else {
        range[0] = start;
        range[1] = end;
        argc = 2;
    }
    if (!fr->brkpt.proglines || program != fr->brkpt.lastproglisted) {
        free_prog_text(fr->brkpt.proglines);
        fr->brkpt.proglines = (struct line *)read_program(program);
        fr->brkpt.lastproglisted = program;
    }

    {
        struct line *tmpline = PROGRAM_FIRST(program);
        PROGRAM_SET_FIRST(program, fr->brkpt.proglines);

        {
            int tmpflg = (FLAGS(player) & INTERNAL);
            FLAGS(player) |= INTERNAL;

            do_list(player, program, range, argc);

            if (!tmpflg) {
                FLAGS(player) &= ~INTERNAL;
            }
        }

        PROGRAM_SET_FIRST(program, tmpline);
    }
    return;
}


char *
show_line_prims(dbref program, struct inst *pc, int maxprims, int markpc)
{
    static char buf[BUFFER_LEN];
    static char buf2[BUFFER_LEN];
    int maxback;
    int thisline = pc->line;
    struct inst *code, *end, *linestart, *lineend;

    code = PROGRAM_CODE(program);
    end = code + PROGRAM_SIZ(program);
    buf[0] = '\0';

    for (linestart = pc, maxback = maxprims; linestart > code &&
            linestart->line == thisline && linestart->type != PROG_FUNCTION &&
            --maxback; --linestart);
    if (linestart->line < thisline)
        ++linestart;

    for (lineend = pc + 1, maxback = maxprims; lineend < end &&
            lineend->line == thisline && lineend->type != PROG_FUNCTION &&
            --maxback; ++lineend);
    if (lineend >= end || lineend->line > thisline ||
            lineend->type == PROG_FUNCTION)
        --lineend;

    if (lineend - linestart >= maxprims) {
        if (pc - (maxprims - 1) / 2 > linestart)
            linestart = pc - (maxprims - 1) / 2;
        if (linestart + maxprims - 1 < lineend)
            lineend = linestart + maxprims - 1;
    }

    if (linestart > code && (linestart - 1)->line == thisline)
        strcpy(buf, "...");
    maxback = maxprims;
    while (linestart <= lineend) {
        if (strlen(buf) < BUFFER_LEN / 2) {
            if (*buf) strcat(buf, " ");
            if (pc == linestart && markpc) {
                strcat(buf, " {{");
                strcat(buf, insttotext(linestart, buf2, sizeof(buf2), 30, program));
                strcat(buf, "}} ");
            } else {
                strcat(buf, insttotext(linestart, buf2, sizeof(buf2), 30, program));
            }
        } else {
            break;
        }
        linestart++;
    }
    if (lineend < end && (lineend + 1)->line == thisline)
        strcat(buf, " ...");
    return buf;
}


struct inst *
funcname_to_pc(dbref program, const char *name)
{
    int i, siz;
    struct inst *code;
    code = PROGRAM_CODE(program);
    siz = PROGRAM_SIZ(program);
    for (i = 0; i < siz; i++) {
        if ((code[i].type == PROG_FUNCTION) &&
                !string_compare(name, code[i].data.string->data)) {
            return (code + i);
        }
    }
    return(NULL);
}


struct inst *
linenum_to_pc(dbref program, int whatline)
{
    int i, siz;
    struct inst *code;
    code = PROGRAM_CODE(program);
    siz = PROGRAM_SIZ(program);
    for (i = 0; i < siz; i++) {
        if (code[i].line == whatline) {
            return (code + i);
        }
    }
    return(NULL);
}


char *
unparse_sysreturn(dbref *program, struct inst *pc)
{
    static char buf[BUFFER_LEN];
    struct inst *ptr;
    char *fname;

    buf[0] = '\0';
    for (ptr = pc-1; ptr >= PROGRAM_CODE(*program); ptr--) {
        if (ptr->type == PROG_FUNCTION) {
            break;
        }
    }
    if (ptr->type == PROG_FUNCTION) {
        fname = ptr->data.string->data;
    } else {
        fname = "???";
    }
    sprintf(buf, "line %d, in function %s", pc->line, fname);
    return buf;
}


char *
unparse_breakpoint(struct frame *fr, int brk)
{
    static char buf[BUFFER_LEN];
    char buf2[BUFFER_LEN];
    dbref ref;
    sprintf(buf, "%2d) break", brk + 1);
    if (fr->brkpt.line[brk] != -1) {
        sprintf(buf2, " in line %d", fr->brkpt.line[brk]);
        strcat(buf, buf2);
    }
    if (fr->brkpt.pc[brk] != NULL) {
        ref = fr->brkpt.prog[brk];
        sprintf(buf2, " at %s", unparse_sysreturn(&ref, fr->brkpt.pc[brk]+1));
        strcat(buf, buf2);
    }
    if (fr->brkpt.linecount[brk] != -2) {
        sprintf(buf2, " after %d line(s)", fr->brkpt.linecount[brk]);
        strcat(buf, buf2);
    }
    if (fr->brkpt.pccount[brk] != -2) {
        sprintf(buf2, " after %d instruction(s)", fr->brkpt.pccount[brk]);
        strcat(buf, buf2);
    }
    if (fr->brkpt.prog[brk] != NOTHING) {
        sprintf(buf2, " in %s(#%d)", NAME(fr->brkpt.prog[brk]),
            fr->brkpt.prog[brk]);
        strcat(buf, buf2);
    }
    if (fr->brkpt.level[brk] != -1) {
        sprintf(buf2, " on call level %d", fr->brkpt.level[brk]);
        strcat(buf, buf2);
    }
    return buf;
}

void
muf_backtrace(dbref player, dbref program, int count, struct frame *fr)
{
    char buf[BUFFER_LEN];
    char *ptr;
    dbref ref;
    int i, j, cnt, flag;
    struct inst *pinst, *lastinst;

    notify_nolisten(player, "System stack backtrace:", 1);
    i = count;
    if (!i) i = STACK_SIZE;
    ref = program;
    pinst = NULL;
    j = fr->system.top+1;
    while (j>1 && i-->0) {
        cnt = 0;
        do {
            lastinst = pinst;
            if (--j == fr->system.top) {
                pinst = fr->pc+1;
            } else {
                ref = fr->system.st[j].progref;
                pinst = fr->system.st[j].offset;
            }
            ptr = unparse_sysreturn(&ref, pinst);
            cnt++;
        } while (pinst == lastinst && j>1);
        if (cnt > 1) {
            sprintf(buf, "     [repeats %d times]", cnt);
            notify_nolisten(player, buf, 1);
        }
        if (pinst != lastinst) {
            sprintf(buf, "%3d) %s(#%d) %s:", j, NAME(ref), ref, ptr);
            notify_nolisten(player, buf, 1);
            flag = ((FLAGS(player) & INTERNAL)? 1 : 0);
            FLAGS(player) &= ~INTERNAL;
            list_proglines(player, ref, fr, (pinst-1)->line, 0);
            if (flag) {
                FLAGS(player) |= INTERNAL;
            }
        }
    }
    notify_nolisten(player, "*done*", 1);
}

void
list_program_functions(dbref player, dbref program, char *arg)
{
    struct inst *ptr;
    int count;

    ptr = PROGRAM_CODE(program);
    count = PROGRAM_SIZ(program);
    notify_nolisten(player, "*function words*", 1);
    while (count-- > 0) {
        if (ptr->type == PROG_FUNCTION) {
            if (ptr->data.string) {
                if (!*arg || equalstr(arg, ptr->data.string->data)) {
                    notify_nolisten(player, ptr->data.string->data, 1);
                }
            }
        }
        ptr++;
    }
    notify_nolisten(player, "*done*", 1);
}

#define CurrVar (*(fr->varset.st[fr->varset.top]))

static void
debug_printvar(dbref player, struct frame *fr, const char *arg)
{
    int i;
    int lflag = 0;
    int sflag = 0;
	char buf[BUFFER_LEN];

    if (!arg || !*arg) {
        notify_nolisten(player, "I don't know which variable you mean.", 1);
        return;
    }
    if (*arg == 'L' || *arg == 'l') {
	    arg++, lflag = 1;
    } else if (*arg == 'S' || *arg == 's') {
	    arg++, sflag = 1;
	}
    if (*arg == 'V' || *arg == 'v') arg++;
    if (!number(arg)) {
        notify_nolisten(player, "I don't know which variable you mean.", 1);
        return;
    }
    i = atoi(arg);
    if (i >= MAX_VAR || i < 0) {
        notify_nolisten(player, "Variable number out of range.", 1);
        return;
    }
	if (sflag) {
        struct inst *tmp = scopedvar_get(fr, i);
		if (!tmp) {
			notify_nolisten(player, "Scoped variable number out of range.", 1);
			return;
		}
        notify_nolisten(player, insttotext(tmp, buf, sizeof(buf), 4000, -1), 1);
	} else if (lflag) {
        notify_nolisten(player, insttotext(&(CurrVar[i]), buf, sizeof(buf), 4000, -1), 1);
    } else {
        notify_nolisten(player, insttotext(&(fr->variables[i]), buf, sizeof(buf), 4000, -1), 1);
    }
}

static void
push_arg(dbref player, struct frame *fr, const char *arg)
{
    int num, lflag = 0;
    int sflag = 0;
    float inum;

    if (fr->argument.top >= STACK_SIZE) {
        notify_nolisten(player, "That would overflow the stack.", 1);
        return;
    }
    if (number(arg)) {
        /* push a number */
        num = atoi(arg);
        push(fr->argument.st, &fr->argument.top, PROG_INTEGER, MIPSCAST &num);
        notify_nolisten(player, "Integer pushed.", 1);
    } else if (ifloat(arg)) {
        /* push a float */
        inum = (float) atof(arg);
        push(fr->argument.st, &fr->argument.top, PROG_FLOAT, MIPSCAST &inum);
        notify_nolisten(player, "Float pushed.", 1);
    } else if (*arg == '#') {
        /* push a dbref */
        if (!number(arg+1)) {
            notify_nolisten(player, "I don't understand that dbref.", 1);
            return;
        }
        num = atoi(arg+1);
        push(fr->argument.st, &fr->argument.top, PROG_OBJECT, MIPSCAST &num);
        notify_nolisten(player, "Dbref pushed.", 1);
    } else if (*arg == 'L' || *arg == 'V' || *arg == 'l' || *arg == 'v') {
        if (*arg == 'S' || *arg == 's') {
            arg++;
            sflag = 1;
        } else if (*arg == 'L' || *arg == 'l') {
            arg++;
            lflag = 1;
        }
        if (*arg == 'V' || *arg == 'v') arg++;
        if (!number(arg)) {
            notify_nolisten(player, "I don't understand which variable you mean.", 1);
            return;
        }
        num = atoi(arg);
        if (lflag) {
            push(fr->argument.st, &fr->argument.top, PROG_LVAR, MIPSCAST &num);
            notify_nolisten(player, "Local variable pushed.", 1);
        } else if (sflag) {
            push(fr->argument.st, &fr->argument.top, PROG_SVAR, MIPSCAST &num);
            notify_nolisten(player, "Scoped variable pushed.", 1);
        } else {
            push(fr->argument.st, &fr->argument.top, PROG_VAR, MIPSCAST &num);
            notify_nolisten(player, "Global variable pushed.", 1);
        }
    } else if (*arg == '"') {
        /* push a string */
        char buf[BUFFER_LEN];
        char *ptr;
        const char *ptr2;

        for (ptr = buf, ptr2 = arg+1; *ptr2; ptr2++) {
            if (*ptr2 == '\\') {
                if (!*(++ptr2)) break;
                *ptr++ = *ptr2;
            } else if (*ptr2 == '"') {
                break;
            } else {
                *ptr++ = *ptr2;
            }
        }
        *ptr = '\0';
        push(fr->argument.st, &fr->argument.top, PROG_STRING,
                MIPSCAST alloc_prog_string(buf));
        notify_nolisten(player, "String pushed.", 1);
    } else {
        notify_nolisten(player, "I don't know that data type.", 1);
    }
}


extern int primitive(const char *token);

struct inst primset[5];
struct shared_string shstr;

int
muf_debugger(int descr, dbref player, dbref program, const char *text, struct frame *fr)
{
    char cmd[BUFFER_LEN];
    char buf[BUFFER_LEN];
    char buf2[BUFFER_LEN];
    char *ptr, *ptr2, *arg;
    struct inst *pinst;
    dbref ref;
    int i, j, cnt;

    while (isspace(*text)) text++;
    strcpy(cmd, text);
    ptr = cmd + strlen(cmd);
    if (ptr > cmd) ptr--;
    while (ptr >= cmd && isspace(*ptr)) *ptr-- = '\0';
    for (arg = cmd; *arg && !isspace(*arg); arg++);
    if (*arg) *arg++ = '\0';
    if (!*cmd && fr->brkpt.lastcmd) {
        strcpy(cmd, fr->brkpt.lastcmd);
    } else {
        if (fr->brkpt.lastcmd)
            free(fr->brkpt.lastcmd);
        if (*cmd)
            fr->brkpt.lastcmd = string_dup(cmd);
    }
    /* delete triggering breakpoint, if it's only temp. */
    j = fr->brkpt.breaknum;
    if (j >= 0 && fr->brkpt.temp[j]) {
        for (j++; j < fr->brkpt.count; j++) {
            fr->brkpt.temp[j-1]         = fr->brkpt.temp[j];
            fr->brkpt.level[j-1]        = fr->brkpt.level[j];
            fr->brkpt.line[j-1]         = fr->brkpt.line[j];
            fr->brkpt.linecount[j-1]    = fr->brkpt.linecount[j];
            fr->brkpt.pc[j-1]           = fr->brkpt.pc[j];
            fr->brkpt.pccount[j-1]      = fr->brkpt.pccount[j];
            fr->brkpt.prog[j-1]         = fr->brkpt.prog[j];
        }
        fr->brkpt.count--;
    }
    fr->brkpt.breaknum = -1;
    
    if (!string_compare(cmd, "cont")) {
    } else if (!string_compare(cmd, "finish")) {
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot finish because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = fr->system.top - 1;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = program;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "stepi")) {
        i = atoi(arg);
        if (!i) i = 1;
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot stepi because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = -1;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = i;
        fr->brkpt.prog[j] = NOTHING;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "step")) {
        i = atoi(arg);
        if (!i) i = 1;
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot step because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = -1;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = i;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = NOTHING;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "nexti")) {
        i = atoi(arg);
        if (!i) i = 1;
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot nexti because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = fr->system.top;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = i;
        fr->brkpt.prog[j] = program;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "next")) {
        i = atoi(arg);
        if (!i) i = 1;
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot next because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = fr->system.top;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = i;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = program;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "exec")) {
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot finish because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        if (!(pinst = funcname_to_pc(program, arg))) {
            notify_nolisten(player, "I don't know a function by that name.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        if (fr->system.top >= STACK_SIZE) {
            notify_nolisten(player, "That would exceed the system stack size for this program.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        fr->system.st[fr->system.top].progref = program;
        fr->system.st[fr->system.top++].offset = fr->pc;
        fr->pc = pinst;
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = fr->system.top - 1;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = program;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "prim")) {
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Cannot finish because there are too many breakpoints set.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        if (!primitive(arg)) {
            notify_nolisten(player, "I don't recognize that primitive.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }
        if (fr->system.top >= STACK_SIZE) {
            notify_nolisten(player, "That would exceed the system stack size for this program.", 1);
            add_muf_read_event(descr, player, program, fr);
            return 0;
        }

        shstr.data[0] = '\0';
        shstr.links = 1;
        shstr.length= strlen(shstr.data);
        primset[0].type = PROG_FUNCTION;
        primset[0].line = 0;
        primset[0].data.string = &shstr;
        primset[1].type = PROG_DECLVAR;
        primset[1].line = 0;
        primset[1].data.number = 0;
        primset[2].type = PROG_PRIMITIVE;
        primset[2].line = 0;
        primset[2].data.number = get_primitive(arg);
        primset[3].type = PROG_PRIMITIVE;
        primset[3].line = 0;
        primset[3].data.number = IN_RET;
        /* primset[3].data.number = primitive("EXIT"); */

        fr->system.st[fr->system.top].progref = program;
        fr->system.st[fr->system.top++].offset = fr->pc;
        fr->pc = primset;
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 1;
        fr->brkpt.level[j] = fr->system.top - 1;
        fr->brkpt.line[j] = -1;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = program;
        fr->brkpt.bypass = 1;
        return 0;
    } else if (!string_compare(cmd, "break")) {
        add_muf_read_event(descr, player, program, fr);
        if (fr->brkpt.count >= MAX_BREAKS) {
            notify_nolisten(player, "Too many breakpoints set.", 1);
            return 0;
        }
        if (number(arg)) {
            i = atoi(arg);
        } else {
            if (!(pinst = funcname_to_pc(program, arg))) {
                notify_nolisten(player, "I don't know a function by that name.", 1);
                return 0;
            } else {
                i = pinst->line;
            }
        }
        if (!i) i = fr->pc->line;
        j = fr->brkpt.count++;
        fr->brkpt.temp[j] = 0;
        fr->brkpt.level[j] = -1;
        fr->brkpt.line[j] = i;
        fr->brkpt.linecount[j] = -2;
        fr->brkpt.pc[j] = NULL;
        fr->brkpt.pccount[j] = -2;
        fr->brkpt.prog[j] = program;
        notify_nolisten(player, "Breakpoint set.", 1);
        return 0;
    } else if (!string_compare(cmd, "delete")) {
        add_muf_read_event(descr, player, program, fr);
        i = atoi(arg);
        if (!i) {
            notify_nolisten(player, "Which breakpoint did you want to delete?", 1);
            return 0;
        }
        if (i < 1 || i > fr->brkpt.count) {
            notify_nolisten(player, "No such breakpoint.", 1);
            return 0;
        }
        j = i - 1;
        for (j++; j < fr->brkpt.count; j++) {
            fr->brkpt.temp[j-1]      = fr->brkpt.temp[j];
            fr->brkpt.level[j-1]     = fr->brkpt.level[j];
            fr->brkpt.line[j-1]      = fr->brkpt.line[j];
            fr->brkpt.linecount[j-1] = fr->brkpt.linecount[j];
            fr->brkpt.pc[j-1]        = fr->brkpt.pc[j];
            fr->brkpt.pccount[j-1]   = fr->brkpt.pccount[j];
            fr->brkpt.prog[j-1]      = fr->brkpt.prog[j];
        }
        fr->brkpt.count--;
        notify_nolisten(player, "Breakpoint deleted.", 1);
        return 0;
    } else if (!string_compare(cmd, "breaks")) {
        notify_nolisten(player, "Breakpoints:", 1);
        for (i = 0; i < fr->brkpt.count; i++) {
            ptr = unparse_breakpoint(fr, i);
            notify_nolisten(player, ptr, 1);
        }
        notify_nolisten(player, "*done*", 1);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "where")) {
        i = atoi(arg);
        muf_backtrace(player, program, i, fr);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "stack")) {
        notify_nolisten(player, "*Argument stack top*", 1);
        i = atoi(arg);
        if (!i) i = STACK_SIZE;
        ptr = "";
        ref = program;
        for (j = fr->argument.top; j>0 && i-->0;) {
            cnt = 0;
            do {
                strcpy(buf, ptr);
                ptr = insttotext(&fr->argument.st[--j], buf2, sizeof(buf2), 4000, program);
                cnt++;
            } while (!string_compare(ptr, buf) && j>0);
            if (cnt > 1)
                notify_fmt(player, "     [repeats %d times]", cnt);
            if (string_compare(ptr, buf))
                notify_fmt(player, "%3d) %s", j+1, ptr);
        }
        notify_nolisten(player, "*done*", 1);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "list") ||
               !string_compare(cmd, "listi")) {
        int startline, endline;
        startline = endline = 0;
        add_muf_read_event(descr, player, program, fr);
        if ((ptr2 = (char *)index(arg, ','))) {
            *ptr2++ = '\0';
        } else {
            ptr2 = "";
        }
        if (!*arg) {
            if (fr->brkpt.lastlisted) {
                startline = fr->brkpt.lastlisted + 1;
            } else {
                startline = fr->pc->line;
            }
            endline = startline + 15;
        } else {
            if (!number(arg)) {
                if (!(pinst = funcname_to_pc(program, arg))) {
                    notify_nolisten(player, "I don't know a function by that name. (starting arg, 1)", 1);
                    return 0;
                } else {
                    startline = pinst->line;
                    endline = startline + 15;
                }
            } else {
                if (*ptr2) {
                    endline = startline = atoi(arg);
                } else {
                    startline = atoi(arg) - 7;
                    endline = startline + 15;
                }
            }
        }
        if (*ptr2) {
            if (!number(ptr2)) {
                if (!(pinst = funcname_to_pc(program, ptr2))) {
                    notify_nolisten(player, "I don't know a function by that name. (ending arg, 1)", 1);
                    return 0;
                } else {
                    endline = pinst->line;
                }
            } else {
                endline = atoi(ptr2);
            }
        }
        i = (PROGRAM_CODE(program)+
                PROGRAM_SIZ(program)- 1)->line;
        if (startline > i) {
            notify_nolisten(player, "Starting line is beyond end of program.", 1);
            return 0;
        }
        if (startline < 1) startline = 1;
        if (endline > i) endline = i;
        if (endline < startline) endline = startline;
        notify_nolisten(player, "Listing:", 1);
        if (!string_compare(cmd, "listi")) {
            for (i = startline; i <= endline; i++) {
                pinst = linenum_to_pc(program, i);
                if (pinst) {
                    sprintf(buf, "line %d: %s", i, (i == fr->pc->line) ?
                            show_line_prims(program, fr->pc, STACK_SIZE, 1) :
                            show_line_prims(program, pinst, STACK_SIZE, 0));
                    notify_nolisten(player, buf, 1);
                }
            }
        } else {
            list_proglines(player, program, fr, startline, endline);
        }
        fr->brkpt.lastlisted = endline;
        notify_nolisten(player, "*done*", 1);
        return 0;
    } else if (!string_compare(cmd, "quit")) {
        notify_nolisten(player, "Halting execution.", 1);
        return 1;
    } else if (!string_compare(cmd, "trace")) {
        add_muf_read_event(descr, player, program, fr);
        if (!string_compare(arg, "on")) {
            fr->brkpt.showstack = 1;
            notify_nolisten(player, "Trace turned on.", 1);
        } else if (!string_compare(arg, "off")) {
            fr->brkpt.showstack = 0;
            notify_nolisten(player, "Trace turned off.", 1);
        } else {
            sprintf(buf, "Trace is currently %s.",
                    fr->brkpt.showstack? "on" : "off");
            notify_nolisten(player, buf, 1);
        }
        return 0;
    } else if (!string_compare(cmd, "words")) {
        list_program_functions(player, program, arg);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "print")) {
        debug_printvar(player, fr, arg);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "push")) {
        push_arg(player, fr, arg);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else if (!string_compare(cmd, "pop")) {
        add_muf_read_event(descr, player, program, fr);
        if (fr->argument.top < 1) {
            notify_nolisten(player, "Nothing to pop.", 1);
            return 0;
        }
        fr->argument.top--;
        CLEAR(fr->argument.st + fr->argument.top);
        notify_nolisten(player, "Stack item popped.", 1);
        return 0;
    } else if (!string_compare(cmd, "help")) {
notify_nolisten(player, "cont            continues execution until a breakpoint is hit.", 1);
notify_nolisten(player, "finish          completes execution of current function.", 1);
notify_nolisten(player, "step [NUM]      executes one (or NUM, 1) lines of muf.", 1);
notify_nolisten(player, "stepi [NUM]     executes one (or NUM, 1) muf instructions.", 1);
notify_nolisten(player, "next [NUM]      like step, except skips CALL and EXECUTE.", 1);
notify_nolisten(player, "nexti [NUM]     like stepi, except skips CALL and EXECUTE.", 1);
notify_nolisten(player, "break LINE#     sets breakpoint at given LINE number.", 1);
notify_nolisten(player, "break FUNCNAME  sets breakpoint at start of given function.", 1);
notify_nolisten(player, "breaks          lists all currently set breakpoints.", 1);
notify_nolisten(player, "delete NUM      deletes breakpoint by NUM, as listed by 'breaks'", 1);
notify_nolisten(player, "where [LEVS]    displays function call backtrace of up to num levels deep.", 1);
notify_nolisten(player, "stack [NUM]     shows the top num items on the stack.", 1);
notify_nolisten(player, "print v#        displays the value of given global variable #.", 1);
notify_nolisten(player, "print lv#       displays the value of given local variable #.", 1);
notify_nolisten(player, "trace [on|off]  turns on/off debug stack tracing.", 1);
notify_nolisten(player, "list [L1,[L2]]  lists source code of given line range.", 1);
notify_nolisten(player, "list FUNCNAME   lists source code of given function.", 1);
notify_nolisten(player, "listi [L1,[L2]] lists instructions in given line range.", 1);
notify_nolisten(player, "listi FUNCNAME  lists instructions in given function.", 1);
notify_nolisten(player, "words           lists all function word names in program.", 1);
notify_nolisten(player, "words PATTERN   lists all function word names that match PATTERN.", 1);
notify_nolisten(player, "exec FUNCNAME   calls given function with the current stack data.", 1);
notify_nolisten(player, "prim PRIMITIVE  executes given primitive with current stack data.", 1);
notify_nolisten(player, "push DATA       pushes an int, dbref, var, or string onto the stack.", 1);
notify_nolisten(player, "pop             pops top data item off the stack.", 1);
notify_nolisten(player, "help            displays this help screen.", 1);
notify_nolisten(player, "quit            stop execution here.", 1);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    } else {
        notify_nolisten(player, "I don't understand that debugger command. Type 'help' for help.", 1);
        add_muf_read_event(descr, player, program, fr);
        return 0;
    }
    return 0;
}


