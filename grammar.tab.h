/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     OCCUPANT = 258,
     SOCKET = 259,
     LOGFILE = 260,
     BACKEND = 261,
     EDGE = 262,
     OBRACE = 263,
     EBRACE = 264,
     VCL = 265,
     PORT = 266,
     EQUAL = 267,
     QSTRING = 268,
     SEMICOLON = 269,
     FAMILY = 270,
     HOST = 271,
     IPV4 = 272,
     IPV6 = 273,
     ANY = 274
   };
#endif
/* Tokens.  */
#define OCCUPANT 258
#define SOCKET 259
#define LOGFILE 260
#define BACKEND 261
#define EDGE 262
#define OBRACE 263
#define EBRACE 264
#define VCL 265
#define PORT 266
#define EQUAL 267
#define QSTRING 268
#define SEMICOLON 269
#define FAMILY 270
#define HOST 271
#define IPV4 272
#define IPV6 273
#define ANY 274




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 52 "grammar.y"
{
	char		*str;
	unsigned int	 mask;
}
/* Line 1529 of yacc.c.  */
#line 92 "grammar.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

