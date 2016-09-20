%{
/*-
 * Copyright (c) 2016 Christian S.J. Peron
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>

extern int	 yylex(void);
extern void	 yyerror(const char *);
extern char	*yytext;

void
yyerror(const char *str)
{
	extern int lineno;

	(void) fprintf(stderr, "file: %d syntax error near '%s'\n", lineno, yytext);
	exit(1);
}
int
yywrap()
{
	return (1);
}
%}
%union {
	char		*str;
	unsigned int	 mask;
}

%type	<str> QSTRING
%type	<mask> famspec
%token	OCCUPANT SOCKET LOGFILE BACKEND
%token	EDGE OBRACE EBRACE VCL PORT EQUAL QSTRING SEMICOLON FAMILY 
%token	HOST IPV4 IPV6 ANY

%%
root	: /* empty */
	| root cmd
	;
cmd	:
	edge_def
	| occupant_def
        ;

backend_params:
	HOST EQUAL QSTRING SEMICOLON
	| PORT EQUAL QSTRING SEMICOLON
	;

backend_block:
	| backend_block backend_params
	;

backend_def:
	BACKEND OBRACE backend_block EBRACE
	;

occupant_params:
	LOGFILE EQUAL QSTRING SEMICOLON
	| SOCKET EQUAL QSTRING SEMICOLON
	| backend_def
	;

occupant_block:
	| occupant_block occupant_params
	;

occupant_def:
	OCCUPANT QSTRING OBRACE occupant_block EBRACE
	;

famspec:
	IPV4
	{
		$$ = PF_INET;
	}
	| IPV6
	{
		$$ = PF_INET6;
	}
	| ANY
	{
		$$ = 0;
	}
	;

edge_params:
	HOST EQUAL ANY SEMICOLON
	| HOST EQUAL QSTRING SEMICOLON
	| FAMILY EQUAL famspec SEMICOLON
	| PORT EQUAL QSTRING SEMICOLON
	;

edge_block:
	| edge_block edge_params
	;

edge_def:
	EDGE OBRACE edge_block EBRACE
	;
