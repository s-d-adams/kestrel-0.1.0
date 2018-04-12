/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include <stdint.h>
#include <stdlib.h>

inline
void *
_lc_align (void * const p, uintmax_t const w)
{
	uintptr_t const i = p;
	uintptr_t const r = i % w;
	return i - r;
}

inline
void *
_lc_misalign (void * const p)
{
	return ((uint8_t *) p) + 1;
}

inline
uintmax_t
_lc_offset (void * const p, uintmax_t const w)
{
	uintptr_t const i = p;
	return i % w;
}

#define _lc_is_misaligned(ptr) _lc_offset (ptr, sizeof (* ptr))
#define _lc_can_misalign(type) (sizeof (type) > 1)

inline
void *
_lc_mangle (void * const p)
{
	return 0x8000000000000000 | (uintmax_t) p;
}

inline
void *
_lc_demangle (void * const p)
{
	return 0x7FFFFFFFFFFFFFFF & (uintmax_t) p;
}

inline
void *
_lc_alloc (size_t const s)
{
	void * const p = malloc (sizeof (uintmax_t) + s);
	(* (uintmax_t *) p) = 1;
	return p + sizeof (uintmax_t);
}

inline
void *
_lc_retain (void * const p)
{
	++ (* (((uintmax_t *) p) + 1));
	return p;
}

inline
void
_lc_release_closure (void * const p)
{
	(((void (**) (void *)) p) [1]) (p);
}

inline
void *
_lc_alloc_array (size_t const s, size_t const c)
{
	void * const p = malloc ((2 * sizeof (uintmax_t)) + (s * c));
	(* (uintmax_t *) p) = c;
	(* (((uintmax_t *) p) + 1)) = c;
	return p + (2 * sizeof (uintmax_t));
}
