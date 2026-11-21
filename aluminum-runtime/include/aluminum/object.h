/*
 * MIT License
 *
 * Copyright (c) 2026 stringdotjar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Memory layout of Java objects and arrays.
 *
 * This header is the single source of truth for how objects look in memory.
 * The runtime reads objects through these structs, and the compiler must lay
 * out the objects it creates the same way. Nothing else should hard-code a
 * header size or a field offset.
 */
#ifndef ALUMINUM_OBJECT_H
#define ALUMINUM_OBJECT_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A plain object, such as an instance of a class. */
#define ALM_KIND_INSTANCE 0U

/* An array whose elements are references to other objects. */
#define ALM_KIND_REF_ARRAY 1U

/* An array whose elements are primitives, such as int[] or double[]. */
#define ALM_KIND_PRIM_ARRAY 2U

/*
 * Describes one type of object so the collector knows its size and where its
 * references are.
 *
 * The compiler emits one constant alm_type for every class and every array
 * type in the program. Every object points at its type, which is how the
 * collector finds out what it is looking at.
 */
typedef struct alm_type {
  /* Human-readable type name, used only for debugging. May be NULL. */
  const char *name;

  /* One of the ALM_KIND_ constants. */
  uint32_t kind;

  /*
   * Size of an instance in bytes, header included. Only used for
   * ALM_KIND_INSTANCE.
   */
  uint32_t size;

  /*
   * Size of one array element in bytes. Only used for arrays. Reference arrays
   * must use sizeof(alm_object *).
   */
  uint32_t elem_size;

  /* Number of entries in refs. */
  uint32_t nrefs;

  /*
   * Byte offsets, from the start of the object, of every field that holds a
   * reference, inherited fields included. Only used for ALM_KIND_INSTANCE.
   */
  const uint32_t *refs;
} alm_type;

/*
 * The header at the start of every object.
 *
 * The fields of a class follow directly after it, starting at
 * ALM_OBJECT_HEADER_SIZE.
 */
typedef struct alm_object {
  /* The type of this object. Set once by the allocator, never changed. */
  const alm_type *type;

  /*
   * Bits owned by the garbage collector, such as the mark bit. Compiled code
   * must never read or write them.
   */
  uintptr_t gc;
} alm_object;

/*
 * The header at the start of every array.
 *
 * The elements follow at ALM_ARRAY_DATA_OFFSET. Use alm_array_data() to reach
 * them.
 */
typedef struct alm_array {
  alm_object header;

  /* Number of elements. Never negative, never changed after allocation. */
  int32_t length;
} alm_array;

/* Size of alm_object in bytes, which is where the first field of a class goes.
 */
#define ALM_OBJECT_HEADER_SIZE 16U

/* Offset of the length field inside an array. */
#define ALM_ARRAY_LENGTH_OFFSET 16U

/*
 * Offset of the first array element. It is 8-byte aligned, so long and double
 * elements are aligned too.
 */
#define ALM_ARRAY_DATA_OFFSET 24U

static_assert(sizeof(alm_object) == ALM_OBJECT_HEADER_SIZE,
              "object header size changed");
static_assert(offsetof(alm_array, length) == ALM_ARRAY_LENGTH_OFFSET,
              "array length offset changed");
static_assert(sizeof(alm_array) <= ALM_ARRAY_DATA_OFFSET,
              "array header overlaps the elements");

/* Returns a pointer to the first element of an array. */
static inline void *alm_array_data(alm_array *array) {
  return (unsigned char *)array + ALM_ARRAY_DATA_OFFSET;
}

#ifdef __cplusplus
}
#endif

#endif
