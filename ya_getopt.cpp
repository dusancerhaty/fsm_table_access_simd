/*
 * ya_getopt  - Yet another getopt
 * https://github.com/kubo/ya_getopt
 *
 * Ya_getopt is a replacement of [GNU C library getopt](http://man7.org/linux/man-pages/man3/
 * getopt.3.html). `getopt()`, `getopt_long()` and `getopt_long_only()`are implemented
 * excluding the following GNU extension features:
 *
 * 1. If *optstring* contains **W** followed by a semicolon, then **-W** **foo** is
   treated as the long option **--foo**.
 *
 * 2. _<PID>_GNU_nonoption_argv_flags_
 *
 * The license is 2-clause BSD-style license. You can use the Linux getopt compatible function
 * under Windows, Solaris and so on without having to worry about license issue.
 *
 * Copyright 2015 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 *
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "ya_getopt.h"



static void ya_getopt_error(int ya_opterr, const char *optstring, const char *format, ...);
static void check_gnu_extension(ya_context* context, const char *optstring);
static int ya_getopt_internal(ya_context* context, int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex, int long_only);
static int ya_getopt_shortopts(ya_context* context, int argc, char * const argv[], const char *optstring, int long_only);
static int ya_getopt_longopts(ya_context* context, int argc, char * const argv[], char *arg, const char *optstring, const struct option *longopts, int *longindex, int *long_only_flag);

static void ya_getopt_error(int ya_opterr, const char *optstring, const char *format, ...)
{
    if (ya_opterr && optstring[0] != ':') {
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
    }
}

static void check_gnu_extension(ya_context* context, const char *optstring)
{
    if (optstring[0] == '+' || getenv("POSIXLY_CORRECT") != NULL) {
    	context->posixly_correct = 1;
    } else {
    	context->posixly_correct = 0;
    }
    if (optstring[0] == '-') {
    	context->handle_nonopt_argv = 1;
    } else {
    	context->handle_nonopt_argv = 0;
    }
}

void ya_context_initx( ya_context* context )
{
	if( ! context )
	{
		return;
	}

	context->ya_optarg = NULL;
	context->ya_optind = 1;
	context->ya_opterr = 1;
	context->ya_optopt = '?';
	context->ya_optnext = NULL;
	context->posixly_correct = -1;
	context->handle_nonopt_argv = 0;
}

int ya_getopt(ya_context* context, int argc, char * const argv[], const char *optstring)
{
    return ya_getopt_internal(context, argc, argv, optstring, NULL, NULL, 0);
}

int ya_getopt_long(ya_context* context, int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
    return ya_getopt_internal(context, argc, argv, optstring, longopts, longindex, 0);
}

int ya_getopt_long_only(ya_context* context, int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
    return ya_getopt_internal(context, argc, argv, optstring, longopts, longindex, 1);
}

static int ya_getopt_internal(ya_context* context, int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex, int long_only)
{
    static int start, end;

    if (context->ya_optopt == '?') {
        context->ya_optopt = 0;
    }

    if (context->posixly_correct == -1) {
        check_gnu_extension(context, optstring);
    }

    if (context->ya_optind == 0) {
        check_gnu_extension(context, optstring);
        context->ya_optind = 1;
        context->ya_optnext = NULL;
    }

    switch (optstring[0]) {
    case '+':
    case '-':
        optstring++;
    }

    if (context->ya_optnext == NULL && start != 0) {
        int last_pos = context->ya_optind - 1;

        context->ya_optind -= end - start;
        if (context->ya_optind <= 0) {
            context->ya_optind = 1;
        }
        while (start < end--) {
            int i;
            char *arg = argv[end];

            for (i = end; i < last_pos; i++) {
                ((char **)argv)[i] = argv[i + 1];
            }
            ((char const **)argv)[i] = arg;
            last_pos--;
        }
        start = 0;
    }

    if (context->ya_optind >= argc) {
        context->ya_optarg = NULL;
        return -1;
    }
    if (context->ya_optnext == NULL) {
        const char *arg = argv[context->ya_optind];
        if (*arg != '-') {
            if (context->handle_nonopt_argv) {
                context->ya_optarg = argv[context->ya_optind++];
                start = 0;
                return 1;
            } else if (context->posixly_correct) {
                context->ya_optarg = NULL;
                return -1;
            } else {
                int i;

                start = context->ya_optind;
                for (i = context->ya_optind + 1; i < argc; i++) {
                    if (argv[i][0] == '-') {
                        end = i;
                        break;
                    }
                }
                if (i == argc) {
                    context->ya_optarg = NULL;
                    return -1;
                }
                context->ya_optind = i;
                arg = argv[context->ya_optind];
            }
        }
        if (strcmp(arg, "--") == 0) {
            context->ya_optind++;
            return -1;
        }
        if (longopts != NULL && arg[1] == '-') {
            return ya_getopt_longopts(context, argc, argv, argv[context->ya_optind] + 2, optstring, longopts, longindex, NULL);
        }
    }

    if (context->ya_optnext == NULL) {
        context->ya_optnext = argv[context->ya_optind] + 1;
    }
    if (long_only) {
        int long_only_flag = 0;
        int rv = ya_getopt_longopts(context, argc, argv, context->ya_optnext, optstring, longopts, longindex, &long_only_flag);
        if (!long_only_flag) {
            context->ya_optnext = NULL;
            return rv;
        }
    }

    return ya_getopt_shortopts(context, argc, argv, optstring, long_only);
}

static int ya_getopt_shortopts(ya_context* context, int argc, char * const argv[], const char *optstring, int long_only)
{
    int opt = *context->ya_optnext;
    const char *os = strchr(optstring, opt);

    if (os == NULL) {
        context->ya_optarg = NULL;
        if (long_only) {
            ya_getopt_error(context->ya_opterr, optstring, "%s: unrecognized option '-%s'\n", argv[0], context->ya_optnext);
            context->ya_optind++;
            context->ya_optnext = NULL;
        } else {
            context->ya_optopt = opt;
            ya_getopt_error(context->ya_opterr, optstring, "%s: invalid option -- '%c'\n", argv[0], opt);
            if (*(++context->ya_optnext) == 0) {
                context->ya_optind++;
                context->ya_optnext = NULL;
            }
        }
        return '?';
    }
    if (os[1] == ':') {
        if (context->ya_optnext[1] == 0) {
            context->ya_optind++;
            if (os[2] == ':') {
                /* optional argument */
                context->ya_optarg = NULL;
            } else {
                if (context->ya_optind == argc) {
                    context->ya_optarg = NULL;
                    context->ya_optopt = opt;
                    ya_getopt_error(context->ya_opterr, optstring, "%s: option requires an argument -- '%c'\n", argv[0], opt);
                    if (optstring[0] == ':') {
                        return ':';
                    } else {
                        return '?';
                    }
                }
                context->ya_optarg = argv[context->ya_optind];
                context->ya_optind++;
            }
        } else {
            context->ya_optarg = context->ya_optnext + 1;
            context->ya_optind++;
        }
        context->ya_optnext = NULL;
    } else {
        context->ya_optarg = NULL;
        if (context->ya_optnext[1] == 0) {
            context->ya_optnext = NULL;
            context->ya_optind++;
        } else {
            context->ya_optnext++;
        }
    }
    return opt;
}

static int ya_getopt_longopts(ya_context* context, int argc, char * const argv[], char *arg, const char *optstring, const struct option *longopts, int *longindex, int *long_only_flag)
{
    char *val = NULL;
    const struct option *opt;
    size_t namelen;
    int idx;

    for (idx = 0; longopts[idx].name != NULL; idx++) {
        opt = &longopts[idx];
        namelen = strlen(opt->name);
        if (strncmp(arg, opt->name, namelen) == 0) {
            switch (arg[namelen]) {
            case '\0':
                switch (opt->has_arg) {
                case ya_required_argument:
                    context->ya_optind++;
                    if (context->ya_optind == argc) {
                        context->ya_optarg = NULL;
                        context->ya_optopt = opt->val;
                        ya_getopt_error(context->ya_opterr, optstring, "%s: option '--%s' requires an argument\n", argv[0], opt->name);
                        if (optstring[0] == ':') {
                            return ':';
                        } else {
                            return '?';
                        }
                    }
                    val = argv[context->ya_optind];
                    break;
                }
                goto found;
            case '=':
                if (opt->has_arg == ya_no_argument) {
                    const char *hyphens = (argv[context->ya_optind][1] == '-') ? "--" : "-";

                    context->ya_optind++;
                    context->ya_optarg = NULL;
                    context->ya_optopt = opt->val;
                    ya_getopt_error(context->ya_opterr, optstring, "%s: option '%s%s' doesn't allow an argument\n", argv[0], hyphens, opt->name);
                    return '?';
                }
                val = arg + namelen + 1;
                goto found;
            }
        }
    }
    if (long_only_flag) {
        *long_only_flag = 1;
    } else {
        ya_getopt_error(context->ya_opterr, optstring, "%s: unrecognized option '%s'\n", argv[0], argv[context->ya_optind]);
        context->ya_optind++;
    }
    return '?';
found:
    context->ya_optarg = val;
    context->ya_optind++;
    if (opt->flag) {
        *opt->flag = opt->val;
    }
    if (longindex) {
        *longindex = idx;
    }
    return opt->flag ? 0 : opt->val;
}


