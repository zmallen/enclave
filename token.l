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
#define YY_NO_UNPUT
#include "grammar.tab.h"

int lineno = 1;
%}

tokenchar	[^[:blank:];,<>"\n]
tokenstring	{tokenchar}+

%option nounput

%%
\n              lineno++;
[ \t]+          /* ignore whitespace */
\#.*            /* ignore comments */
edge		return (EDGE);
host		return (HOST);
family		return (FAMILY);
v4		return (IPV4);
v6		return (IPV6);
any		return (ANY);
\=		return (EQUAL);
port		return (PORT);
socket		return (SOCKET);
logfile		return (LOGFILE);
vcl		return (VCL);
backend		return (BACKEND);
occupant	return (OCCUPANT);
\;		return (SEMICOLON);
\{		return (OBRACE);
\}		return (EBRACE);
\"              {
			char buf[1024], *ptr, c;
			int escaped;

			escaped = 0;
			bzero(buf, sizeof(buf));
			for (ptr = buf; strlen(buf) < (sizeof(buf) - 1);
			    ptr++) {
				c = input();
				if (escaped && c != EOF) {
					*ptr = c;
					escaped = 0;
					continue;
				}
				switch (c) {
				case '\\':
					ptr--;
					escaped = 1;
					break;
				case '\n':
					*ptr = ' ';
					lineno++;
					break;
				case '"':       /* end quote */
					ptr = NULL;
					break;
				case EOF:
					(void) fprintf(stderr, "unmatched \"\n");
					exit(1);
				default:
					*ptr = c;
					break;
				}
				if (ptr == NULL)
					break;
			}
			yylval.str = strdup(buf);
			return (QSTRING);
		}
{tokenstring}	{
			yylval.str = strdup(yytext);
			return (QSTRING);
		}
