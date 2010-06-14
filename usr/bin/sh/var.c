/*
 * Copyright (c) 2009, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

extern char **environ;

/*
 * Shell variables.
 */

#define VTABSIZE	20
#define MAXVARNAME	32

struct var {
	char	*name;
	char	*val;
};

static struct var vartab[VTABSIZE];

static int
is_name(char c)
{

	if ((c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') || c == '_')
		return 1;
	return 0;
}

static int
is_validname(char *name)
{
	char *p;

	for (p = name; *p != '\0'; p++) {
		if (!is_name(*p))
			return 0;
	}
	return 1;
}

static struct var *
lookupvar(char *name)
{
	struct var *var;
	int i;

	var = &vartab[0];
	for (i = 0; i < VTABSIZE; i++) {
		if (var->name != NULL && !strcmp(var->name, name))
			return var;
		var++;
	}
	return NULL;
}

void
setvar(char *name, char *val)
{
	struct var *var, *free;
	int i;

	/*
	 * Lookup existing name in variable table.
	 */
	var = lookupvar(name);

	if (var == NULL) {
		/*
		 * Not found. Find empty slot.
		 */
		free = NULL;
		var = &vartab[0];
		for (i = 0; i < VTABSIZE; i++) {
			if (var->name == NULL) {
				free = var;
				break;
			}
			var++;
		}
		if (free == NULL) {
			fprintf(stderr, "too many variables\n");
			return;
		}
		var = free;
	}

	if (!is_validname(name)) {
		fprintf(stderr, "%s: bad vairable name\n", name);
		return;
	}
	if ((var->name = strdup(name)) == NULL) {
		fprintf(stderr, "out of memory\n");
		return;
	}
	var->val = strdup(val);
}

void
unsetvar(char *name)
{
	struct var *var;

	/* Find target slot in variable table */
	var = lookupvar(name);
	if (var == NULL)
		return;

	free(var->name);
	free(var->val);
	var->name = NULL;
	var->val = NULL;
}

void
setvareq(char *str)
{
	char name[MAXVARNAME];
	char *p, *s;
	int i;

	s = str;
	p = name;
	for (i = 0; i < MAXVARNAME - 1; i++) {
		*p++ = *s++;
		if (*s == '=')
			break;
	}
	*p = '\0';
	p = strchr(str, '=');
	setvar(name, p + 1);
}

int
cmd_showvars(int argc, char *argv[])
{
	struct var *var;
	int i;

	var = &vartab[0];
	for (i = 0; i < VTABSIZE; i++) {
		if (var->name != NULL)
			printf("%s=%s\n", var->name, var->val);
		var++;
	}
	return 0;
}

int
cmd_unsetvar(int argc, char *argv[])
{

	if (argc != 2) {
		fprintf(stderr, "usage: unset name\n");
		return 0;
	}

	unsetvar(argv[1]);
	return 0;
}

int
cmd_export(int argc, char *argv[])
{
	struct var *var;
	int i;

	if (argc == 1) {
		fprintf(stderr, "usage: export name\n");
		return 0;
	}
	for (i = 1; i < argc; i++) {
		var = lookupvar(argv[i]);
		if (var != NULL)
			setenv(var->name, var->val, 1);
	}
	return 0;
}

void
initvar(void)
{
	char **envp;
	int i;

	for (i = 0; i < VTABSIZE; i++)
		vartab[i].name = NULL;

	for (envp = environ; *envp; envp++) {
		if (strchr(*envp, '='))
			setvareq(*envp);
	}
}
