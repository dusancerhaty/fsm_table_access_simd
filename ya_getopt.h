/* -*- indent-tabs-mode: nil -*-
 *
 * ya_getopt  - Yet another getopt
 * https://github.com/kubo/ya_getopt
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
#ifndef YA_GETOPT_H
#define YA_GETOPT_H 1



#define ya_no_argument        0
#define ya_required_argument  1
#define ya_optional_argument  2

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

typedef struct {
	char* ya_optarg;
	int   ya_optind;
	int   ya_opterr;
	int   ya_optopt;
	char* ya_optnext;         //static
	int   posixly_correct;    //static
	int   handle_nonopt_argv; //static
} ya_context;

void ya_context_initx( ya_context* context );
int ya_getopt(ya_context* context, int argc, char * const argv[], const char *optstring);
int ya_getopt_long(ya_context* context, int argc, char * const argv[], const char *optstring,
                   const struct option *longopts, int *longindex);
int ya_getopt_long_only(ya_context* context, int argc, char * const argv[], const char *optstring,
                        const struct option *longopts, int *longindex);




#endif
