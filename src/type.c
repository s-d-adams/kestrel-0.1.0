/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "parse.h"
#include "type.h"

static
int
str_contains (char const * str, char const c)
{
	for (; *str; ++str)
	{
		if (c == *str)
		{
			return 1;
		}
	}
	return 0;
}

static
char const decimalChars[] = "0123456789";

// 128-bit ints are not required by the standard.
// Explicitly-sized floats are not required by the standard.
//FIXME: I have commented-out the explicitly sized types for now.  Type precedence is an issue.
//size_t const kNumberOfPrimativeTypes = 21;
static
size_t const kNumberOfPrimativeTypes = 12;

static
char const * primativeCTypeNames[] =
{
	"int",
	"char", "unsigned char",
	"short", "unsigned short",
	"int", "unsigned int",
	"long int", "unsigned long int",
	"float", "double", "long double",
//	"size_t",
//	"int8_t", "uint8_t",
//	"int16_t", "uint16_t",
//	"int32_t", "uint32_t",
//	"int64_t", "uint64_t",
	0
};

static
char const * primativeKTypeNames[] =
{
	"int",
	"char", "byte",
	"short", "ushort",
	"int", "uint",
	"long", "ulong",
	"float", "double", "long_double",
//	"size",
//	"i8", "u8",
//	"i16", "u16",
//	"i32", "u32",
//	"i64", "u64",
	0
};

static
char const escapeChars[] = "0abfnrtv\\?'\"";

static
char const octalChars[] = "01234567";

static
char const hexChars[] = "0123456789ABCDEFabcdef";

static
char const * binops[] =
{
	"+", "-", "*", "/", "%", "&&", "||", "==", "!=", "<", ">", "<=", ">=","&", "|", "^", 0
};

static
char const * booleanBinops[] =
{
	"&&", "||", "==", "!=", "<", ">", "<=", ">=", 0
};

static
char const * unops[] =
{
	"!", "-", "+", "~", 0
};

static
char const * booleanUnopsp[] =
{
	"!", 0
};

//FIXME: Might want to remove the bool type.
static
int
isBool (struct token const str)
{
	if ((str.size < 4) || (str.size > 5))
	{
		return 0;
	}
	return ((! t_strcmp (str, "true")) || (! t_strcmp (str, "false")));
}

static
int
isChar (struct token const str)
{
	if (str.size < 3)
	{
		return 0;
	}
	if (*str.bytes != '\'' || str.bytes[str.size - 1] !='\'')
	{
		return 0;
	}
	if (str.bytes[1] != '\\')
	{
		return str.size == 3 && str.bytes[1] != '\'';
	}
	else
	{
		if (str.size == 4)
			return str_contains(escapeChars, str.bytes[2]);
		else if (str.size == 6) {
			char const esc = str.bytes[2];
			if ('x' == esc) {
				return
					str_contains(hexChars, str.bytes[3])
					&& str_contains(hexChars, str.bytes[4]);
			}
			else if ('o' == esc) {
				return
					str_contains(octalChars, str.bytes[3])
					&& str_contains(octalChars, str.bytes[4]);
			}
			else
				return 0;
		}
		else
			return 0;
	}
}

//FIXME: wide char literals not yet supported (shorts and ints)
//FIXME: automatic cast to long not yet supported
static
int
isInt(struct token const t)
{
	char const * str = t.bytes;
	size_t len = t.size;
	if (!len)
		return 0;
	if (*str == '+' || *str == '-') {
		++str;
		--len;
		if (!len)
			return 0;
	}
	if (*str == '0') {
		if (len > 2 && str[1] == 'x') {
			for (unsigned i = 2; i < len; ++i) {
				if (!str_contains(hexChars, str[i]))
					return 0;
			}
			return 1;
		}
		else
		{
			for (unsigned i = 1; i < len; ++i)
			{
				if (! str_contains (octalChars, str[i]))
				{
					return 0;
				}
			}
			return 1;
		}
	}
	else
	{
		for (unsigned i = 0; i < len; ++i)
		{
			if (!str_contains(decimalChars, str[i]))
			{
				return 0;
			}
		}
		return 1;
	}
}

static
int
isFooWithSuffix (struct token const t, char const * const suffix, int (* isFoo) (struct token))
{
	char const * str = t.bytes;
	size_t const len = t.size;
	if (! len)
	{
		return 0;
	}
	int suffixPos = len - strlen (suffix);
	if (suffixPos < 1)
	{
		return 0;
	}
	if (strcmp (str + suffixPos, suffix))
	{
		return 0;
	}
	char * strMod = strndup(str, len);
	strMod[suffixPos] = '\0';
	int const result = isFoo (make_token (token_type_name, strMod, suffixPos, 0, 0, 0));
	free (strMod);
	return result;
}

/* Forces the use of upper case for clarity. */
static
int
isLongInt (struct token const str)
{
	return isFooWithSuffix(str, "L", isInt);
}

static
int
isUnsignedInt (struct token const str)
{
	return isFooWithSuffix(str, "U", isInt);
}

static
int
isUnsignedLongInt (struct token const str)
{
	return isFooWithSuffix(str, "UL", isInt);
}

static
int
isDouble (struct token const t)
{
	char const * str = t.bytes;
	size_t const len = t.size;
	if (!len)
		return 0;
	unsigned ePos = len;
	unsigned dPos = len;
	for (unsigned i = 0; i < len; ++i)
	{
		if (str[i] == '.')
		{
			dPos = i;
		}
		else if (str[i] == 'E')
		{
			ePos = i;
			break;
		}
	}
	if (ePos < len)
	{
		if (ePos == 0)
		{
			return 0;
		}
		else
		{
			struct token t2 = subtoken (t, ePos + 1, NULL);
			int rc = isInt (t2);
			free (t2.bytes);
			if (! rc)
			{
				return 0;
			}
		}
	}
	if (dPos < len)
	{
		if (str[dPos] == '+' || str[dPos] == '-')
		{
			return 0;
		}
		struct token t3 = subtoken (t, dPos + 1, NULL);
		int rc = isInt (t3);
		free (t3.bytes);
		if (! rc)
		{
			return 0;
		}
	}
	size_t const t4size = (dPos < ePos ? dPos : ePos);
	struct token t4 = subtoken (t, 0, & t4size);
	int result = isInt (t4);
	free (t4.bytes);
	return result;
}

static
int
isFloat (struct token const str)
{
	return isFooWithSuffix(str, "F", isDouble);
}

static
int
isLongDouble (struct token const str)
{
	return isFooWithSuffix(str, "L", isDouble);
}

static
int
list_contains (char const *list[], struct token const str)
{
	for (char const ** it = list; *it; ++it)
	{
		if (strlen (* it) == str.size)
		{
			if (! strncmp(*it, str.bytes, str.size))
			{
				return 1;
			}
		}
	}
	return 0;
}

static
int
indexOf (char const *list[], struct token const str)
{
	for (int i=0; list[i]; ++i)
	{
		if (! strncmp(list[i], str.bytes, str.size))
			return i;
	}
	return -1;
}

int
is_type_primative (struct syntax_tree * const type)
{
	if (is_type_mutable (type))
	{
		return is_type_primative (st_at (type->sub_tree, 1));
	}
	return (type->token.type == token_type_name) && list_contains (primativeKTypeNames, type->token);
}

static
char const *
primative_to_c_type_name (struct syntax_tree * const type)
{
	assert (is_type_primative (type));
	for (unsigned i = 0; i < kNumberOfPrimativeTypes; ++ i)
	{
		if (! t_strcmp (type->token, primativeKTypeNames[i]))
		{
			return primativeCTypeNames[i];
		}
	}
	assert (0);
	return 0;
}

static
int
false_predicate (struct token const t)
{
	(void) t;
	return 0;
}

char const *
type_of_literal (struct token const str)
{
//FIXME: See if there are any other literals I can support.
	int (* fp) (struct token);
	fp = false_predicate;
	int(*preds[])(struct token) =
	{
		isBool,
		isChar, fp,
		fp, fp,
		isInt, isUnsignedInt,
		isLongInt, isUnsignedLongInt,
		isFloat, isDouble, isLongDouble,
		fp,
		fp, fp,
		fp, fp,
		fp, fp,
		fp, fp,
		0
	};
	for (unsigned i = 0; i < kNumberOfPrimativeTypes; ++i)
	{
		if (preds[i](str))
		{
			return primativeKTypeNames[i];
		}
	}
	return 0;
}

int
is_unop (struct token const str)
{
	return list_contains (unops, str);
}

int
is_boolean_unop (struct token const str)
{
	return list_contains (booleanUnopsp, str);
}

int
is_binop (struct token const str)
{
	return list_contains (binops, str);
}

int
is_boolean_binop (struct token const str)
{
	return list_contains (booleanBinops, str);
}

static
int
max (int const a, int const b)
{
	return a > b ? a : b;
}

//FIXME: I will want to revisit implicit type conversion.
//FIXME: Also this implementation has a whole load of missed cases.
//FIXME: Nowhere in the project is the name "unsigned" used for either a C or a K type.
//FIXME: This function doesnt make it clear whether it is talking in terms of C types or K types.
// From its usage it seems like it's talking in K terms, not C terms.
char const *
precedent_type (struct token const type1, struct token const type2)
{
	/* bool < char < short < int < long < float < double < long_double
	   if one is unsigned and the other is long or less return long */

	char const * uintOrder[] = {
		"bool", "byte", "ushort", "uint", "ulong", 0
	};

	char const * intOrder[] = {
		"char", "short", "int", "long", 0
	};

	char const * floatOrder[] = {
		"float", "double", "long_double", 0
	};

	int const
	type1IndexUint  = indexOf(uintOrder, type1),
	type1IndexInt   = indexOf(intOrder, type1),
	type1IndexFloat = indexOf(floatOrder, type1),
	type2IndexUint  = indexOf(uintOrder, type2),
	type2IndexInt   = indexOf(intOrder, type2),
	type2IndexFloat = indexOf(floatOrder, type2);

	int const
	highestUint  = max (type1IndexUint, type2IndexUint),
	highestInt   = max (type1IndexInt, type2IndexInt),
	highestFloat = max (type1IndexFloat, type2IndexFloat);

	int const classCount
	=	((highestUint != -1) ? 1 : 0)
	+	((highestInt != -1) ? 1 : 0)
	+	((highestFloat != -1) ? 1 : 0);

	if (classCount == 0)
	{
		return NULL;
	}

	if (classCount == 1)
	{
		if (highestInt != -1)
		{
			return intOrder[highestInt];
		}
		if (highestUint != -1)
		{
			return uintOrder[highestUint];
		}
		if (highestFloat != -1)
		{
			return floatOrder[highestFloat];
		}
	}
	else if (classCount == 2) {
//FIXME: The following type deductions are wrong in a lot of cases.
//FIXME: I might even want to wrap operator calls in explicit casts just to make sure.
		if (highestInt == -1)
		{
			return floatOrder[highestFloat];
		}
		if (highestUint == -1)
		{
			return floatOrder[highestFloat];
		}
		if (highestFloat == -1)
		{
			return intOrder[highestInt];
		}
	}

	return NULL;
}

int
is_type_void (struct syntax_tree * const type)
{
	return (token_type_name == type->token.type && ! t_strcmp (type->token, "void"));
}

int
is_type_mutable (struct syntax_tree * const node)
{
	return (node->token.type == token_type_open) && (st_count (node->sub_tree) == 2) && (! t_strcmp (st_at (node->sub_tree, 0)->token, "mutable"));
}

int
is_type_null (struct syntax_tree * const type)
{
	return (type->token.type == token_type_name) && (! t_strcmp (type->token, "Null"));
}

int
is_type_pointer (struct syntax_tree * type)
{
	if (is_type_mutable (type))
	{
		return is_type_pointer (st_at (type->sub_tree, 1));
	}
	return (token_type_open == type->token.type && st_count (type->sub_tree) == 2 && ! t_strcmp (st_at (type->sub_tree, 0)->token, "ptr"));
}

int
is_type_array (struct syntax_tree * type)
{
	if (is_type_mutable (type))
	{
		return is_type_array (st_at (type->sub_tree, 1));
	}
	return (token_type_open == type->token.type && st_count (type->sub_tree) == 2 && ! t_strcmp (st_at (type->sub_tree, 0)->token, "array"));
}

int
is_type_function (struct syntax_tree * type)
{
	if (is_type_mutable (type))
	{
		return is_type_function (st_at (type->sub_tree, 1));
	}

	return (token_type_open == type->token.type)
	       && (! is_type_array (type))
	       && (! is_type_pointer (type));
}

int
is_type_struct_name (struct context * const ctx, struct syntax_tree * const type)
{
	if (is_type_mutable (type))
	{
		return is_type_struct_name (ctx, st_at (type->sub_tree, 1));
	}
	return (type->token.type == token_type_name) && context_findStruct (ctx, type->token);
}

int
is_type_reference_counted (struct context * const ctx, struct syntax_tree * const type)
{
	return is_type_array (type) || is_type_struct_name (ctx, type) || is_type_function (type);
}

int
types_equal_modulo_mutability (struct syntax_tree * const a_orig, struct syntax_tree * const b_orig)
{
	struct syntax_tree * const a = is_type_mutable (a_orig) ? st_at (a_orig->sub_tree, 1) : a_orig;
	struct syntax_tree * const b = is_type_mutable (b_orig) ? st_at (b_orig->sub_tree, 1) : b_orig;

	assert (! ((a == NULL) && (b == NULL)));
	if ((a == NULL) ^ (b == NULL))
	{
		return 0;
	}
	if (! token_equal (a->token, b->token))
	{
		return 0;
	}
	if (a->sub_tree)
	{
		if (! b->sub_tree)
		{
			return 0;
		}
		if (st_count (a->sub_tree) != st_count (b->sub_tree))
		{
			return 0;
		}
	}
	else if (b->sub_tree)
	{
		return 0;
	}
	size_t i;
	if (a->sub_tree == NULL)
	{
		return b->sub_tree == NULL;
	}
	size_t const size = st_count (a->sub_tree);
	for (i = 0; i < size; ++ i)
	{
		if (! types_equal_modulo_mutability (st_at (a->sub_tree, i), st_at (b->sub_tree, i)))
		{
			return 0;
		}
	}
	return 1;
}

struct string_list *
eval_type (struct context * const ctx, struct syntax_tree * pn, int const as_mutable)
{
	if (is_type_mutable (pn))
	{
		return eval_type (ctx, st_at (pn->sub_tree, 1), 1);
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	if (pn->token.type == token_type_name)
	{
		if (is_type_void (pn))
		{
			end = sl_copy_append (end, "void");
		}
		else if (is_type_primative (pn))
		{
			end = sl_copy_append (end, primative_to_c_type_name (pn));
		}
		else if (context_findStruct (ctx, pn->token))
		{
			end = sl_copy_append (end, "struct ");
			end = sl_copy_append_t (end, pn->token);
			end = sl_copy_append (end, " const*");
		}
		else
		{
			// Name not recognised.
			// Do we allow this in impure functions?
			// Let's say not until it becomes apparent it's needed.
			context_set_error (ctx, "Type not recognised.", pn);
			return NULL;
		}
	}
	else if (token_type_open == pn->token.type)
	{
		if (is_type_pointer (pn) || is_type_array (pn))
		{
			end = sl_splice (end, eval_type (ctx, st_at (pn->sub_tree, 1), 0));
			if (ctx->has_error)
			{
				return NULL;
			}
			end = sl_copy_append (end, "*");
		}
		else
		{
			if (is_type_function (pn))
			{
				return sl_copy_str ("void*");
			}
			else
			{
				assert (0);
			}
		}
	}
	else
	{
		context_set_error (ctx, "Type not recognised.", pn);
		return NULL;
	}

	if (as_mutable)
	{
		end = sl_copy_append (end, " ");
	}
	else
	{
		end = sl_copy_append (end, " const ");
	}

	return begin;
}
