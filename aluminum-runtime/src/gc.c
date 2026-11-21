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
 * The heap: a size-class allocator plus a mark-sweep garbage collector.
 *
 * Small objects (up to SMALL_MAX bytes) live in blocks. Each block is carved
 * into equal cells of one size class, and a bitmap in the block header records
 * which cells are in use. Larger objects get their own allocation from the
 * system and sit on a linked list.
 *
 * A collection marks every object reachable from the roots by setting the mark
 * bit in its header, then sweeps: unmarked cells go back to their block's
 * bitmap, blocks with no live cells go back to the system, and unmarked large
 * objects are freed.
 */
#include "aluminum/gc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aluminum/object.h"
#include "heap.h"

/*
 * When AddressSanitizer is on, free cells are poisoned so that any read or
 * write through a pointer to a collected object is reported right away.
 * Without it, these macros do nothing.
 */
#ifdef __has_feature
#if __has_feature(address_sanitizer)
#define ALM_ASAN 1
#endif
#endif
#ifdef __SANITIZE_ADDRESS__
#define ALM_ASAN 1
#endif

#ifdef ALM_ASAN
#include <sanitizer/asan_interface.h>
#define POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
#define UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#else
#define POISON(addr, size) ((void)(addr), (void)(size))
#define UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif

enum {
  /* Every object size is rounded up to a multiple of this. */
  GRANULE = 16,

  /* Objects bigger than this skip the blocks and become large objects. */
  SMALL_MAX = 2048,

  /* Bytes of cell space in one block. */
  BLOCK_BYTES = 64 * 1024,

  /* Words in a block's bitmap: enough bits for the smallest cell size. */
  BITMAP_WORDS = BLOCK_BYTES / GRANULE / 64,

  /* Number of size classes. Must match the length of class_sizes. */
  CLASS_COUNT = 24,
};

/* The heap may grow to this size before the first collection. */
#define MIN_TRIGGER ((size_t)4 * 1024 * 1024)

/* Set in alm_object.gc while an object is known to be reachable. */
#define MARK_BIT ((uintptr_t)1)

/*
 * Cell sizes of the size classes, smallest first. Steps are small for small
 * sizes and grow by a quarter of the size later, so no class wastes more than
 * about 20% of a cell to rounding.
 */
static const uint32_t class_sizes[CLASS_COUNT] = {
    16,  32,  48,  64,  80,  96,  112, 128,  160,  192,  224,  256,
    320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048,
};

/* A chunk of memory holding cells of one size class. */
struct block {
  /* Next block of the same size class. */
  struct block *next;

  /* Size of each cell in bytes. */
  uint32_t cell_size;

  /* Number of cells in this block. */
  uint32_t cells;

  /* First bitmap word that might still have a free cell. */
  uint32_t cursor;

  /* One bit per cell, set when the cell holds an object. */
  uint64_t used[BITMAP_WORDS];

  /* The cells themselves. max_align_t keeps them aligned for any type. */
  max_align_t data[];
};

/* All blocks of one size class. */
struct size_class {
  /* Every block of this class. */
  struct block *blocks;

  /* The block to allocate from next. Blocks before it are known to be full. */
  struct block *cur;
};

/* An object too big for a block, allocated on its own. */
struct large {
  /* Next large object. */
  struct large *next;

  /* Size of the object in bytes. */
  size_t size;

  /* The object itself. */
  max_align_t data[];
};

/* Everything the heap knows. There is one heap per program. */
struct heap {
  struct size_class classes[CLASS_COUNT];
  struct large *large;

  /* Size class index for each object size, in units of GRANULE. */
  uint8_t class_of[(SMALL_MAX / GRANULE) + 1];

  /* Newest pushed root frame. */
  alm_frame *top;

  /* Global root slots registered through alm_add_root(). */
  alm_object ***roots;
  size_t nroots;
  size_t roots_cap;

  /* Objects marked but not yet scanned for references. */
  alm_object **stack;
  size_t sp;
  size_t stack_cap;

  /* Bytes taken from the system. */
  size_t heap_bytes;

  /* A collection runs before the heap grows past this. */
  size_t next_gc;

  /* Hard limit on heap_bytes, or 0 for none. */
  size_t max_heap;

  size_t live_bytes;
  size_t live_objects;
  uint64_t collections;
  bool ready;
};

static struct heap heap;

/* Reports a problem the runtime cannot recover from, and stops the program. */
static void fatal(const char *message) {
  (void)fprintf(stderr, "aluminum: fatal: %s\n", message);
  abort();
}

/* Returns the index of the lowest set bit. x must not be 0. */
static unsigned lowest_bit(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return (unsigned)__builtin_ctzll(x);
#else
  unsigned n = 0;
  while ((x & 1U) == 0) {
    x >>= 1U;
    n++;
  }
  return n;
#endif
}

/* Returns true if the heap may take bytes more from the system. */
static bool fits(size_t bytes) {
  if (heap.max_heap == 0) {
    return true;
  }
  if (heap.heap_bytes > heap.max_heap) {
    return false;
  }
  return bytes <= heap.max_heap - heap.heap_bytes;
}

static unsigned char *cell_at(struct block *b, size_t index) {
  return (unsigned char *)b->data + (index * b->cell_size);
}

/* Returns the bits of a bitmap word that stand for real cells. */
static uint64_t valid_bits(const struct block *b, uint32_t word) {
  uint32_t first = word * 64U;
  if (b->cells - first >= 64U) {
    return UINT64_MAX;
  }
  return (UINT64_C(1) << (b->cells - first)) - 1U;
}

static uint32_t bitmap_words(const struct block *b) {
  return (b->cells + 63U) / 64U;
}

/* Claims a free cell in a block, or returns NULL if the block is full. */
static void *block_take(struct block *b) {
  uint32_t words = bitmap_words(b);
  for (uint32_t w = b->cursor; w < words; w++) {
    uint64_t free_bits = ~b->used[w] & valid_bits(b, w);
    if (free_bits != 0) {
      unsigned bit = lowest_bit(free_bits);
      b->used[w] |= UINT64_C(1) << bit;
      b->cursor = w;
      return cell_at(b, ((size_t)w * 64U) + bit);
    }
  }
  b->cursor = words;
  return NULL;
}

/* Claims a free cell from any block of a size class. */
static void *take_cell(struct size_class *sc) {
  for (struct block *b = sc->cur; b != NULL; b = b->next) {
    void *cell = block_take(b);
    if (cell != NULL) {
      sc->cur = b;
      return cell;
    }
  }
  sc->cur = NULL;
  return NULL;
}

/* Adds a fresh, empty block to a size class. Returns false on failure. */
static bool add_block(struct size_class *sc, uint32_t cell_size) {
  if (!fits(BLOCK_BYTES)) {
    return false;
  }
  struct block *b = malloc(sizeof(struct block) + BLOCK_BYTES);
  if (b == NULL) {
    return false;
  }
  b->cell_size = cell_size;
  b->cells = BLOCK_BYTES / cell_size;
  b->cursor = 0;
  memset(b->used, 0, sizeof(b->used));
  POISON(b->data, BLOCK_BYTES);

  b->next = sc->blocks;
  sc->blocks = b;
  sc->cur = b;
  heap.heap_bytes += BLOCK_BYTES;
  return true;
}

static void *alloc_small(size_t size) {
  uint8_t index = heap.class_of[size / GRANULE];
  struct size_class *sc = &heap.classes[index];
  uint32_t cell_size = class_sizes[index];

  void *cell = take_cell(sc);
  if (cell == NULL && heap.heap_bytes >= heap.next_gc) {
    alm_gc();
    cell = take_cell(sc);
  }
  if (cell == NULL && add_block(sc, cell_size)) {
    cell = take_cell(sc);
  }
  if (cell == NULL) {
    /* Last resort before giving up: collect and try once more. */
    alm_gc();
    cell = take_cell(sc);
    if (cell == NULL && add_block(sc, cell_size)) {
      cell = take_cell(sc);
    }
  }
  if (cell != NULL) {
    UNPOISON(cell, cell_size);
    memset(cell, 0, cell_size);
  }
  return cell;
}

/* Allocates and links one large object. Returns NULL on failure. */
static void *add_large(size_t size) {
  if (!fits(size) || size > SIZE_MAX - sizeof(struct large)) {
    return NULL;
  }
  struct large *l = calloc(1, sizeof(struct large) + size);
  if (l == NULL) {
    return NULL;
  }
  l->size = size;
  l->next = heap.large;
  heap.large = l;
  heap.heap_bytes += size;
  return l->data;
}

static void *alloc_large(size_t size) {
  if (heap.heap_bytes >= heap.next_gc) {
    alm_gc();
  }
  void *obj = add_large(size);
  if (obj == NULL) {
    alm_gc();
    obj = add_large(size);
  }
  return obj;
}

/* Allocates size bytes of zeroed object memory. */
static alm_object *alloc(size_t size) {
  if (size > SIZE_MAX - (GRANULE - 1)) {
    return NULL;
  }
  size = (size + GRANULE - 1) & ~(size_t)(GRANULE - 1);
  if (size <= SMALL_MAX) {
    return alloc_small(size);
  }
  return alloc_large(size);
}

alm_object *alm_new(const alm_type *type) {
  if (!heap.ready || type == NULL || type->kind != ALM_KIND_INSTANCE ||
      type->size < sizeof(alm_object)) {
    return NULL;
  }
  alm_object *obj = alloc(type->size);
  if (obj != NULL) {
    obj->type = type;
  }
  return obj;
}

alm_array *alm_new_array(const alm_type *type, int32_t length) {
  if (!heap.ready || type == NULL || length < 0) {
    return NULL;
  }
  if (type->kind == ALM_KIND_REF_ARRAY) {
    if (type->elem_size != sizeof(alm_object *)) {
      return NULL;
    }
  } else if (type->kind != ALM_KIND_PRIM_ARRAY || type->elem_size == 0) {
    return NULL;
  }

  size_t count = (size_t)length;
  if (count > (SIZE_MAX - ALM_ARRAY_DATA_OFFSET) / type->elem_size) {
    return NULL;
  }
  alm_array *array =
      (alm_array *)alloc(ALM_ARRAY_DATA_OFFSET + (count * type->elem_size));
  if (array != NULL) {
    array->header.type = type;
    array->length = length;
  }
  return array;
}

/* Marks an object reachable and queues it to have its references scanned. */
static void mark(alm_object *obj) {
  if (obj == NULL || (obj->gc & MARK_BIT) != 0) {
    return;
  }
  obj->gc |= MARK_BIT;

  if (heap.sp == heap.stack_cap) {
    size_t cap = heap.stack_cap == 0 ? 256 : heap.stack_cap * 2;
    alm_object **stack =
        (alm_object **)realloc((void *)heap.stack, cap * sizeof(alm_object *));
    if (stack == NULL) {
      fatal("out of memory while marking the heap");
    }
    heap.stack = stack;
    heap.stack_cap = cap;
  }
  heap.stack[heap.sp++] = obj;
}

/* Marks everything an object points at. */
static void scan(alm_object *obj) {
  const alm_type *type = obj->type;
  if (type->kind == ALM_KIND_INSTANCE) {
    for (uint32_t i = 0; i < type->nrefs; i++) {
      mark(*(alm_object **)((unsigned char *)obj + type->refs[i]));
    }
  } else if (type->kind == ALM_KIND_REF_ARRAY) {
    alm_array *array = (alm_array *)obj;
    alm_object **elems = (alm_object **)alm_array_data(array);
    for (int32_t i = 0; i < array->length; i++) {
      mark(elems[i]);
    }
  }
}

/*
 * Marks every object reachable from the roots.
 *
 * Uses an explicit stack instead of recursion, so a very long chain of objects
 * (such as a linked list with a million nodes) cannot overflow the C stack.
 */
static void mark_all(void) {
  for (alm_frame *f = heap.top; f != NULL; f = f->prev) {
    for (uint32_t i = 0; i < f->count; i++) {
      mark(f->slots[i]);
    }
  }
  for (size_t i = 0; i < heap.nroots; i++) {
    mark(*heap.roots[i]);
  }
  while (heap.sp > 0) {
    scan(heap.stack[--heap.sp]);
  }
}

/*
 * Frees the unmarked cells of a block and clears the marks of the rest.
 *
 * Returns the number of cells still in use.
 */
static size_t sweep_block(struct block *b) {
  size_t live = 0;
  uint32_t words = bitmap_words(b);
  for (uint32_t w = 0; w < words; w++) {
    uint64_t bits = b->used[w];
    while (bits != 0) {
      unsigned bit = lowest_bit(bits);
      bits &= bits - 1U;
      alm_object *obj = (alm_object *)cell_at(b, ((size_t)w * 64U) + bit);
      if ((obj->gc & MARK_BIT) != 0) {
        obj->gc &= ~MARK_BIT;
        live++;
      } else {
        b->used[w] &= ~(UINT64_C(1) << bit);
        POISON(obj, b->cell_size);
      }
    }
  }
  b->cursor = 0;
  return live;
}

static void sweep_class(struct size_class *sc) {
  struct block **link = &sc->blocks;
  while (*link != NULL) {
    struct block *b = *link;
    size_t live = sweep_block(b);
    if (live == 0) {
      *link = b->next;
      UNPOISON(b->data, BLOCK_BYTES);
      free(b);
      heap.heap_bytes -= BLOCK_BYTES;
    } else {
      heap.live_objects += live;
      heap.live_bytes += live * b->cell_size;
      link = &b->next;
    }
  }
  sc->cur = sc->blocks;
}

static void sweep_large(void) {
  struct large **link = &heap.large;
  while (*link != NULL) {
    struct large *l = *link;
    alm_object *obj = (alm_object *)l->data;
    if ((obj->gc & MARK_BIT) != 0) {
      obj->gc &= ~MARK_BIT;
      heap.live_objects++;
      heap.live_bytes += l->size;
      link = &l->next;
    } else {
      *link = l->next;
      heap.heap_bytes -= l->size;
      free(l);
    }
  }
}

void alm_gc(void) {
  if (!heap.ready) {
    return;
  }
  mark_all();

  heap.live_bytes = 0;
  heap.live_objects = 0;
  for (size_t i = 0; i < CLASS_COUNT; i++) {
    sweep_class(&heap.classes[i]);
  }
  sweep_large();

  /* Let the heap double before the next collection, so collections stay rare
   * when lots of memory is in use. */
  heap.next_gc =
      heap.heap_bytes > MIN_TRIGGER / 2 ? heap.heap_bytes * 2 : MIN_TRIGGER;
  heap.collections++;
}

void alm_push_frame(alm_frame *frame) {
  frame->prev = heap.top;
  heap.top = frame;
}

void alm_pop_frame(alm_frame *frame) { heap.top = frame->prev; }

int alm_add_root(alm_object **slot) {
  if (heap.nroots == heap.roots_cap) {
    size_t cap = heap.roots_cap == 0 ? 16 : heap.roots_cap * 2;
    alm_object ***roots =
        (alm_object ***)realloc((void *)heap.roots, cap * sizeof(*roots));
    if (roots == NULL) {
      return 1;
    }
    heap.roots = roots;
    heap.roots_cap = cap;
  }
  heap.roots[heap.nroots++] = slot;
  return 0;
}

void alm_get_gc_stats(alm_gc_stats *stats) {
  stats->collections = heap.collections;
  stats->heap_bytes = heap.heap_bytes;
  stats->live_bytes = heap.live_bytes;
  stats->live_objects = heap.live_objects;
}

int alm_heap_init(const alm_config *config) {
  memset(&heap, 0, sizeof(heap));
  if (config != NULL) {
    heap.max_heap = config->max_heap;
  }
  heap.next_gc = MIN_TRIGGER;

  uint8_t index = 0;
  for (size_t g = 0; g <= SMALL_MAX / GRANULE; g++) {
    while (class_sizes[index] < g * GRANULE) {
      index++;
    }
    heap.class_of[g] = index;
  }
  heap.ready = true;
  return 0;
}

void alm_heap_shutdown(void) {
  for (size_t i = 0; i < CLASS_COUNT; i++) {
    struct block *b = heap.classes[i].blocks;
    while (b != NULL) {
      struct block *next = b->next;
      UNPOISON(b->data, BLOCK_BYTES);
      free(b);
      b = next;
    }
  }
  struct large *l = heap.large;
  while (l != NULL) {
    struct large *next = l->next;
    free(l);
    l = next;
  }
  free((void *)heap.roots);
  free((void *)heap.stack);
  memset(&heap, 0, sizeof(heap));
}
