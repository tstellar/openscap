/*
 * Copyright 2009--2013 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Peter Vrabec" <pvrabec@redhat.com>
 *      Šimon Lukašík
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "results/oval_evr_string_impl.h"

#ifdef HAVE_RPMVERCMP
#include <rpm/rpmlib.h>
#else
#if !defined(__FreeBSD__)
#include <alloca.h>
#endif
static int rpmvercmp(const char *a, const char *b);
static int risdigit(int c) {
	// locale independent
	return (c >= '0' && c <= '9');
}
#endif

static int compare_values(const char *str1, const char *str2);
static void parseEVR(char *evr, const char **ep, const char **vp, const char **rp);

int oval_evr_string_cmp(const char *a, const char *b)
{
	/* This mimics rpmevrcmp which is not exported by rpmlib version 4.
	 * Code inspired by rpm.labelCompare() from rpm4/python/header-py.c
	 */
	const char *a_epoch, *a_version, *a_release;
	const char *b_epoch, *b_version, *b_release;
	char *a_copy, *b_copy;
	int result;

	a_copy = oscap_strdup(a);
	b_copy = oscap_strdup(b);
	parseEVR(a_copy, &a_epoch, &a_version, &a_release);
	parseEVR(b_copy, &b_epoch, &b_version, &b_release);

	result = compare_values(a_epoch, b_epoch);
	if (!result) {
		result = compare_values(a_version, b_version);
		if (!result)
			result = compare_values(a_release, b_release);
	}

	oscap_free(a_copy);
	oscap_free(b_copy);
	return result;
}

static int compare_values(const char *str1, const char *str2)
{
	/*
	 * Code copied from rpm4/python/header-py.c
	 */
	if (!str1 && !str2)
		return 0;
	else if (str1 && !str2)
		return 1;
	else if (!str1 && str2)
		return -1;
	return rpmvercmp(str1, str2);
}

static void parseEVR(char *evr, const char **ep, const char **vp, const char **rp)
{
	/*
	 * Code copied from rpm4/lib/rpmds.c
	 */
	const char *epoch;
	const char *version;			/* assume only version is present */
	const char *release;
	char *s, *se;

	s = evr;
	while (*s && risdigit(*s)) s++;		/* s points to epoch terminator */
	se = strrchr(s, '-');			/* se points to version terminator */

	if (*s == ':') {
		epoch = evr;
		*s++ = '\0';
		version = s;
		if (*epoch == '\0')
			epoch = "0";
	} else {
		epoch = NULL;			/* XXX disable epoch compare if missing */
		version = evr;
	}
	if (se) {
		*se++ = '\0';
		release = se;
	} else {
		release = NULL;
	}

	if (ep) *ep = epoch;
	if (vp) *vp = version;
	if (rp) *rp = release;
}

#ifndef HAVE_RPMVERCMP
/*
 * code from http://rpm.org/api/4.4.2.2/rpmvercmp_8c-source.html
 */

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
static int rpmvercmp(const char *a, const char *b)
{
	char oldch1, oldch2;
	char *str1, *str2;
	char *one, *two;
	int rc;
	int isnum;

	/* easy comparison to see if versions are identical */
	if (!strcmp(a, b))
		return 0;

	/* TODO: make new oscap_alloca */
	str1 = alloca(strlen(a) + 1);
	str2 = alloca(strlen(b) + 1);

	strcpy(str1, a);
	strcpy(str2, b);

	one = str1;
	two = str2;

	/* loop through each version segment of str1 and str2 and compare them */
	while (*one && *two) {
		while (*one && !isalnum(*one))
			one++;
		while (*two && !isalnum(*two))
			two++;

		/* If we ran to the end of either, we are finished with the loop */
		if (!(*one && *two))
			break;

		str1 = one;
		str2 = two;

		/* grab first completely alpha or completely numeric segment */
		/* leave one and two pointing to the start of the alpha or numeric */
		/* segment and walk str1 and str2 to end of segment */
		if (isdigit(*str1)) {
			while (*str1 && isdigit(*str1))
				str1++;
			while (*str2 && isdigit(*str2))
				str2++;
			isnum = 1;
		} else {
			while (*str1 && isalpha(*str1))
				str1++;
			while (*str2 && isalpha(*str2))
				str2++;
			isnum = 0;
		}

		/* save character at the end of the alpha or numeric segment */
		/* so that they can be restored after the comparison */
		oldch1 = *str1;
		*str1 = '\0';
		oldch2 = *str2;
		*str2 = '\0';

		/* this cannot happen, as we previously tested to make sure that */
		/* the first string has a non-null segment */
		if (one == str1)
			return -1;	/* arbitrary */

		/* take care of the case where the two version segments are */
		/* different types: one numeric, the other alpha (i.e. empty) */
		/* numeric segments are always newer than alpha segments */
		/* result_test See patch #60884 (and details) from bugzilla #50977. */
		if (two == str2)
			return (isnum ? 1 : -1);

		if (isnum) {
			/* this used to be done by converting the digit segments */
			/* to ints using atoi() - it's changed because long  */
			/* digit segments can overflow an int - this should fix that. */

			/* throw away any leading zeros - it's a number, right? */
			while (*one == '0')
				one++;
			while (*two == '0')
				two++;

			/* whichever number has more digits wins */
			if (strlen(one) > strlen(two))
				return 1;
			if (strlen(two) > strlen(one))
				return -1;
		}

		/* strcmp will return which one is greater - even if the two */
		/* segments are alpha or if they are numeric.  don't return  */
		/* if they are equal because there might be more segments to */
		/* compare */
		rc = strcmp(one, two);
		if (rc)
			return (rc < 1 ? -1 : 1);

		/* restore character that was replaced by null above */
		*str1 = oldch1;
		one = str1;
		*str2 = oldch2;
		two = str2;
	}
	/* this catches the case where all numeric and alpha segments have */
	/* compared identically but the segment sepparating characters were */
	/* different */
	if ((!*one) && (!*two))
		return 0;

	/* whichever version still has characters left over wins */
	if (!*one)
		return -1;
	else
		return 1;
}
#endif

