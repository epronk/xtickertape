/***************************************************************

  Copyright (C) DSTC Pty Ltd (ACN 052 372 577) 1995.
  Unpublished work.  All Rights Reserved.

  The software contained on this media is the property of the
  DSTC Pty Ltd.  Use of this software is strictly in accordance
  with the license agreement in the accompanying LICENSE.DOC
  file.  If your distribution of this software does not contain
  a LICENSE.DOC file then you have no rights to use this
  software in any manner and should contact DSTC at the address
  below to determine an appropriate licensing arrangement.

     DSTC Pty Ltd
     Level 7, Gehrmann Labs
     University of Queensland
     St Lucia, 4072
     Australia
     Tel: +61 7 3365 4310
     Fax: +61 7 3365 4311
     Email: enquiries@dstc.edu.au

  This software is being provided "AS IS" without warranty of
  any kind.  In no event shall DSTC Pty Ltd be liable for
  damage of any kind arising out of or in connection with
  the use or performance of this software.

****************************************************************/

#ifndef lint
static const char cvsid[] = "$Id: usenet_parser.c,v 1.4 1999/10/04 03:17:34 phelps Exp $";
#endif /* lint */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "usenet_parser.h"

#define INITIAL_TOKEN_SIZE 64
#define INITIAL_EXPR_MAX 8

/* The type of a lexer state */
typedef int (*lexer_state_t)(usenet_parser_t self, int ch);

#define T_BODY "BODY"
#define T_FROM "from"
#define T_EMAIL "email"
#define T_SUBJECT "subject"
#define T_KEYWORDS "keywords"
#define T_XPOSTS "x-posts"

#define T_MATCHES "matches"
#define T_NOT "not"
#define T_EQ "="
#define T_NEQ "!="
#define T_LT "<"
#define T_GT ">"
#define T_LTEQ "<="
#define T_GTEQ ">="

/* The structure of a usenet_parser */
struct usenet_parser
{
    /* The callback for when we've parsed a line */
    usenet_parser_callback_t callback;

    /* The user data for the callback */
    void *rock;

    /* The tag for error messages */
    char *tag;

    /* The buffer in which the token is assembled */
    char *token;

    /* The next character in the token buffer */
    char *token_pointer;

    /* A reference point within the token */
    char *token_mark;

    /* The end of the token buffer */
    char *token_end;

    /* The current lexical state */
    lexer_state_t state;

    /* The current line-number within the input */
    int line_num;

    /* This is set if the group pattern is negated */
    int has_not;

    /* The name of the usenet group whose entry we're currently building */
    char *group;

    /* The field name of the current expression */
    field_name_t field;

    /* The operator of the current expression */
    op_name_t operator;

    /* The usenet_expr patterns of the current group */
    struct usenet_expr *expressions;

    /* The next expression in the array */
    struct usenet_expr *expr_pointer;

    /* The end of the expression array */
    struct usenet_expr *expr_end;
};


/* Lexer state declarations */
static int lex_start(usenet_parser_t self, int ch);
static int lex_comment(usenet_parser_t self, int ch);
static int lex_group(usenet_parser_t self, int ch);
static int lex_group_ws(usenet_parser_t self, int ch);
static int lex_field_start(usenet_parser_t self, int ch);
static int lex_field(usenet_parser_t self, int ch);
static int lex_op_start(usenet_parser_t self, int ch);
static int lex_op(usenet_parser_t self, int ch);
static int lex_pattern_start(usenet_parser_t self, int ch);
static int lex_pattern(usenet_parser_t self, int ch);
static int lex_pattern_ws(usenet_parser_t self, int ch);

/* Prints a consistent error message */
static void parse_error(usenet_parser_t self, char *message)
{
    fprintf(stderr, "%s: parse error line %d: %s\n", self -> tag, self -> line_num, message);
}

/* This is called when a complete group entry has been read */
static int accept_group(usenet_parser_t self, char *group)
{
    int result;
    struct usenet_expr *pointer;

    /* Call the callback */
    result = (self -> callback)(
	self -> rock, group, self -> expressions,
	self -> expr_pointer - self -> expressions);

    /* Clean up */
    for (pointer = self -> expressions; pointer < self -> expr_pointer; pointer++)
    {
	free(pointer -> pattern);
    }

    self -> expr_pointer = self -> expressions;
    return result;
}


/* This is called when a complete expression has been read */
static int accept_expression(
    usenet_parser_t self,
    field_name_t field,
    op_name_t operator,
    char *pattern)
{
    /* Make sure there's room in the expressions table */
    if (! (self -> expr_pointer < self -> expr_end))
    {
	/* Grow the expressions array */
	struct usenet_expr *new_array;
	ssize_t length = (self -> expr_end - self -> expressions) * 2;

	/* Try to allocate more memory */
	if ((new_array = (struct usenet_expr *)realloc(
	    self -> expressions, sizeof(struct usenet_expr) * length)) == NULL)
	{
	    return -1;
	}

	/* Update the other pointers */
	self -> expr_pointer = new_array + (self -> expr_pointer - self -> expressions);
	self -> expressions = new_array;
	self -> expr_end = self -> expressions + length;
    }

    /* Plug in the values */
    self -> expr_pointer -> field = field;
    self -> expr_pointer -> operator = operator;
    self -> expr_pointer -> pattern = strdup(pattern);
    self -> expr_pointer++;
    return 0;
}

/* Appends a character to the end of the token */
static int append_char(usenet_parser_t self, int ch)
{
    /* Grow the token buffer if necessary */
    if (! (self -> token_pointer < self -> token_end))
    {
	char *new_token;
	size_t length = (self -> token_end - self -> token) * 2;

	/* Try to allocate more memory */
	if ((new_token = (char *)realloc(self -> token, length)) == NULL)
	{
	    return -1;
	}

	/* Update the other pointers */
	self -> token_pointer = new_token + (self -> token_pointer - self -> token);
	self -> token_mark = new_token + (self -> token_mark - self -> token);
	self -> token = new_token;
	self -> token_end = self -> token + length;
    }

    *(self -> token_pointer++) = ch;
    return 0;
}

/* Answers the field_name_t corresponding to the given string (or F_NONE if none match) */
static field_name_t translate_field(char *field)
{
    switch (*field)
    {
	/* BODY */
	case 'B':
	{
	    if (strcmp(field, T_BODY) == 0)
	    {
		return F_BODY;
	    }

	    break;
	}

	/* email */
	case 'e':
	{
	    if (strcmp(field, T_EMAIL) == 0)
	    {
		return F_EMAIL;
	    }

	    break;
	}

	/* from */
	case 'f':
	{
	    if (strcmp(field, T_FROM) == 0)
	    {
		return F_FROM;
	    }

	    break;
	}

	/* keywords */
	case 'k':
	{
	    if (strcmp(field, T_KEYWORDS) == 0)
	    {
		return F_KEYWORDS;
	    }

	    break;
	}

	/* subject */
	case 's':
	{
	    if (strcmp(field, T_SUBJECT) == 0)
	    {
		return F_SUBJECT;
	    }

	    break;
	}

	/* x-posts */
	case 'x':
	{
	    if (strcmp(field, T_XPOSTS) == 0)
	    {
		return F_XPOSTS;
	    }

	    break;
	}
    }

    /* Not a valid field name */
    return F_NONE;
}

/* Answers the op_name_t corresponding to the given string (or O_NONE if none match) */
static op_name_t translate_op(char *operator)
{
    switch (*operator)
    {
	/* != */
	case '!':
	{
	    if (strcmp(operator, T_NEQ) == 0)
	    {
		return O_NEQ;
	    }

	    break;
	}

	/* < or <= */
	case '<':
	{
	    if (strcmp(operator, T_LT) == 0)
	    {
		return O_LT;
	    }

	    if (strcmp(operator, T_LTEQ) == 0)
	    {
		return O_LTEQ;
	    }
	}

	/* = */
	case '=':
	{
	    if (strcmp(operator, T_EQ) == 0)
	    {
		return O_EQ;
	    }

	    break;
	}

	/* > or >= */
	case '>':
	{
	    if (strcmp(operator, T_GT) == 0)
	    {
		return O_GT;
	    }

	    if (strcmp(operator, T_GTEQ) == 0)
	    {
		return O_GTEQ;
	    }

	    break;
	}

	/* matches */
	case 'm':
	{
	    if (strcmp(operator, T_MATCHES) == 0)
	    {
		return O_MATCHES;
	    }

	    break;
	}

	/* not */
	case 'n':
	{
	    if (strcmp(operator, T_NOT) == 0)
	    {
		return O_NOT;
	    }

	    break;
	}
    }

    /* Not a valid operator name */
    return O_NONE;
}

/* Awaiting the first character of a usenet subscription */
static int lex_start(usenet_parser_t self, int ch)
{
    /* Watch for EOF */
    if (ch == EOF)
    {
	printf("<EOF>\n");
	self -> state = lex_start;
	return 0;
    }

    /* Watch for comments */
    if (ch == '#')
    {
	self -> state = lex_comment;
	return 0;
    }

    /* Watch for blank lines */
    if (ch == '\n')
    {
	self -> state = lex_start;
	self -> line_num++;
	return 0;
    }

    /* Skip other whitespace */
    if (isspace(ch))
    {
	self -> state = lex_start;
	return 0;
    }

    /* Anything else is part of the group pattern */
    self -> has_not = 0;
    self -> token_pointer = self -> token;
    self -> state = lex_group;
    return append_char(self, ch);
}

/* Reading a comment (to end-of-line) */
static int lex_comment(usenet_parser_t self, int ch)
{
    /* Watch for end-of-file */
    if (ch == EOF)
    {
	return lex_start(self, ch);
    }

    /* Watch for end of line */
    if (ch == '\n')
    {
	self -> state = lex_start;
	self -> line_num++;
	return 0;
    }

    /* Ignore everything else */
    return 0;
}

/* Reading the group (or its preceding `not') */
static int lex_group(usenet_parser_t self, int ch)
{
    /* Watch for EOF or newline */
    if ((ch == EOF) || (ch == '\n'))
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* This is a nice, short group entry */
	if (accept_group(self, self -> token) < 0)
	{
	    return -1;
	}

	return lex_start(self, ch);
    }

    /* Watch for other whitespace */
    if (isspace(ch))
    {
	self -> state = lex_group_ws;

	/* Null-terminate the token */
	return append_char(self, '\0');
    }

    /* Watch for the separator character */
    if (ch == '/')
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* Record the group name for later use */
	self -> group = strdup(self -> token);
	self -> token_pointer = self -> token;
	self -> state = lex_field_start;
	return 0;
    }

    /* Anything else is part of the group pattern */
    return append_char(self, ch);
}

/* We've got some whitespace in the group pattern.  It could mean that 
 * the "not" keyword was used or it could mean that we've just got
 * some space between the pattern and the separator character */
static int lex_group_ws(usenet_parser_t self, int ch)
{
    /* Watch for EOF or newline */
    if ((ch == EOF) || (ch == '\n'))
    {
	/* This is a nice, short group entry */
	if (accept_group(self, self -> token) < 0)
	{
	    return -1;
	}

	return lex_start(self, ch);
    }

    /* Skip additional whitespace */
    if (isspace(ch))
    {
	return 0;
    }

    /* Watch for the separator character */
    if (ch == '/')
    {
	/* Record the group name for later */
	self -> group = strdup(self -> token);
	self -> token_pointer = self -> token;
	self -> state = lex_field_start;
	return 0;
    }

    /* Anything else wants to be part of the pattern.  Make sure that
     * the token we've already read is "not" and that we're not doing
     * the double-negative thing */
    if (strcmp(self -> token, T_NOT) != 0)
    {
	parse_error(self, "expected '/' or newline");
	return -1;
    }

    /* Watch for double-negative */
    if (self -> has_not)
    {
	parse_error(self, "double negative not permitted");
	return -1;
    }

    /* This is a negated group pattern */
    self -> has_not = 1;
    self -> token_pointer = self -> token;

    /* Anything else is part of the group pattern */
    self -> state = lex_group;
    return append_char(self, ch);
}

/* We've just read the '/' after the group pattern */
static int lex_field_start(usenet_parser_t self, int ch)
{
    /* Skip other whitespace */
    if (isspace(ch))
    {
	return 0;
    }

    /* Anything else is the start of the field name */
    self -> state = lex_field;
    return lex_field(self, ch);
}

/* We're reading the field portion of an expression */
static int lex_field(usenet_parser_t self, int ch)
{
    /* Watch for EOF or LF in the middle of the field */
    if ((ch == EOF) || (ch == '\n'))
    {
	parse_error(self, "unexpected end of line");
	return -1;
    }

    /* Watch for a stray separator */
    if (ch == '/')
    {
	parse_error(self, "incomplete expression");
	return -1;
    }

    /* Watch for whitespace */
    if (isspace(ch))
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* Look up the field */
	if ((self -> field = translate_field(self -> token)) == F_NONE)
	{
	    char *buffer = (char *)malloc(sizeof("unknown field \"\"") + strlen(self -> token));
	    sprintf(buffer, "unknown field \"%s\"\n", self -> token);
	    parse_error(self, buffer);
	    free(buffer);
	    return -1;
	}

	self -> token_pointer = self -> token;
	self -> state = lex_op_start;
	return 0;
    }

    /* Anything else is part of the field name */
    return append_char(self, ch);
}

/* Reading the whitespace before an operator */
static int lex_op_start(usenet_parser_t self, int ch)
{
    /* Skip additional whitespace */
    if (isspace(ch))
    {
	return 0;
    }

    /* Anything else is part of the operator name */
    self -> state = lex_op;
    return lex_op(self, ch);
}

/* Reading the operator name */
static int lex_op(usenet_parser_t self, int ch)
{
    /* Watch for EOF or linefeed in the middle of the expression */
    if ((ch == EOF) || (ch == '\n'))
    {
	parse_error(self, "unexpected end of line");
	return -1;
    }

    /* Watch for a stray separator */
    if (ch == '/')
    {
	parse_error(self, "incomplete expression");
	return -1;
    }

    /* Watch for more whitespace */
    if (isspace(ch))
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* Look up the operator */
	if ((self -> operator = translate_op(self -> token)) == O_NONE)
	{
	    char *buffer = (char *)malloc(sizeof("unknown operator \"\"") + strlen(self -> token));
	    sprintf(buffer, "unknown operator \"%s\"\n", self -> token);
	    parse_error(self, buffer);
	    free(buffer);
	    return -1;
	}

	self -> token_pointer = self -> token;
	self -> state = lex_pattern_start;
	return 0;
    }

    /* Anything else is part of the operator */
    return append_char(self, ch);
}

/* Reading whitespace before the pattern */
static int lex_pattern_start(usenet_parser_t self, int ch)
{
    /* Skip additional whitespace */
    if (isspace(ch))
    {
	return 0;
    }

    /* Anything else is part of the operator name */
    self -> state = lex_pattern;
    return lex_pattern(self, ch);
}

/* Reading the pattern itself */
static int lex_pattern(usenet_parser_t self, int ch)
{
    /* Watch for EOF or linefeed */
    if ((ch == EOF) || (ch == '\n'))
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* Do something interesting with the expression */
	if (accept_expression(self, self -> field, self -> operator, self -> token) < 0)
	{
	    return -1;
	}

	/* This is also the end of the group entry */
	if (accept_group(self, self -> group) < 0)
	{
	    return -1;
	}

	free(self -> group);

	/* Let the start state deal with the EOF or linefeed */
	return lex_start(self, ch);
    }

    /* Watch for other whitespace */
    if (isspace(ch))
    {
	/* Record the position of the whitespace for later trimming */
	self -> token_mark = self -> token_pointer;
	self -> state = lex_pattern_ws;
	return append_char(self, ch);
    }

    /* Watch for a separator */
    if (ch == '/')
    {
	/* Null-terminate the token */
	if (append_char(self, '\0') < 0)
	{
	    return -1;
	}

	/* Do something interesting with the expression */
	self -> token_pointer = self -> token;
	self -> state = lex_field_start;
	return accept_expression(self, self -> field, self -> operator, self -> token);
    }

    /* Anything else is part of the pattern */
    return append_char(self, ch);
}

/* Reading trailing whitespace after a pattern */
static int lex_pattern_ws(usenet_parser_t self, int ch)
{
    /* Watch for EOF and newline */
    if ((ch == EOF) || (ch == '\n'))
    {
	/* Trim off the trailing whitespace */
	*self -> token_mark = '\0';

	/* Do something interesting with the expression */
	if (accept_expression(self, self -> field, self -> operator, self -> token) < 0)
	{
	    return -1;
	}

	/* This is also the end of the group entry */
	if (accept_group(self, self -> group) < 0)
	{
	    return -1;
	}

	free(self -> group);

	/* Let the start state deal with the EOF or newline */
	return lex_start(self, ch);
    }

    /* Skip any other whitespace */
    if (isspace(ch))
    {
	return append_char(self, ch);
    }

    /* Watch for a separator */
    if (ch == '/')
    {
	/* Trim off the trailing whitespace */
	*self -> token_mark = '\0';

	/* Do something interesting with the expression */
	self -> token_pointer = self -> token;
	self -> state = lex_field_start;
	return accept_expression(self, self -> field, self -> operator, self -> token);
    }

    /* Anything else is more of the pattern */
    self -> state = lex_pattern;
    return append_char(self, ch);
}


/* Allocates and initializes a new usenet file parser */
usenet_parser_t usenet_parser_alloc(usenet_parser_callback_t callback, void *rock, char *tag)
{
    usenet_parser_t self;

    /* Allocate memory for the new usenet_parser */
    if ((self = (usenet_parser_t)malloc(sizeof(struct usenet_parser))) == NULL)
    {
	return NULL;
    }

    /* Copy the tag */
    if ((self -> tag = strdup(tag)) == NULL)
    {
	free(self);
	return NULL;
    }

    /* Allocate room for the token buffer */
    if ((self -> token = (char *)malloc(INITIAL_TOKEN_SIZE)) == NULL)
    {
	free(self -> tag);
	free(self);
	return NULL;
    }

    /* Allocate room for the expression array */
    if ((self -> expressions = (struct usenet_expr *)malloc(
	sizeof(struct usenet_expr) * INITIAL_EXPR_MAX)) == NULL)
    {
	free(self -> token);
	free(self -> tag);
	free(self);
	return NULL;
    }

    /* Initialize everything else to a sane value */
    self -> callback = callback;
    self -> rock = rock;
    self -> token_pointer = self -> token;
    self -> token_mark = self -> token;
    self -> token_end = self -> token + INITIAL_TOKEN_SIZE;
    self -> state = lex_start;
    self -> line_num = 1;
    self -> has_not = 0;
    self -> group = NULL;
    self -> field = F_NONE;
    self -> operator = O_NONE;
    self -> expr_pointer = self -> expressions;
    self -> expr_end = self -> expressions + INITIAL_EXPR_MAX;
    return self;
}

/* Frees the resources consumed by the receiver */
void usenet_parser_free(usenet_parser_t self)
{
    free(self -> tag);
    free(self -> token);
    free(self -> expressions);
    free(self);
}

/* Parses the given buffer, calling callbacks for each usenet
 * subscription expression that is successfully read.  A zero-length
 * buffer is interpreted as an end-of-input marker */
int usenet_parser_parse(usenet_parser_t self, char *buffer, ssize_t length)
{
    char *pointer;

    /* Length of 0 indicates EOF */
    if (length == 0)
    {
	return (self -> state)(self, EOF);
    }

    /* Parse the buffer */
    for (pointer = buffer; pointer < buffer + length; pointer++)
    {
	if ((self -> state)(self, *pointer) < 0)
	{
	    return -1;
	}
    }

    return 0;
}