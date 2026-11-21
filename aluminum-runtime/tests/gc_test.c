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
 * Tests for the allocator and garbage collector, written the way compiled code
 * will use them: types are constant alm_type tables, and live pointers sit in
 * root frames.
 *
 * Run under AddressSanitizer, any read of an object the collector freed too
 * early is reported, because the collector poisons freed cells.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "aluminum/gc.h"
#include "aluminum/object.h"
#include "aluminum/runtime.h"

#ifdef __has_feature
#if __has_feature(address_sanitizer)
#define TEST_ASAN 1
#endif
#endif
#ifdef __SANITIZE_ADDRESS__
#define TEST_ASAN 1
#endif
#ifdef TEST_ASAN
#include <sanitizer/asan_interface.h>
#endif

/*
 * Fails the whole run with a message when a condition does not hold.
 *
 * Stopping at the first failure keeps later checks from crashing on a NULL
 * pointer an earlier check already caught.
 */
#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

static void check(bool ok, const char *expr, const char *file, int line) {
  if (!ok) {
    (void)fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expr);
    exit(1);
  }
}

/* A linked list node, laid out the way the compiler would lay out a class. */
typedef struct node {
  alm_object header;
  struct node *next;
  alm_object *payload;
  int64_t value;
  uint64_t check;
} node;

static const uint32_t node_refs[] = {
    offsetof(node, next),
    offsetof(node, payload),
};

static const alm_type node_type = {
    .name = "Node",
    .kind = ALM_KIND_INSTANCE,
    .size = sizeof(node),
    .nrefs = 2,
    .refs = node_refs,
};

/* Like java.lang.Object: just a header and no fields. */
static const alm_type object_type = {
    .name = "Object",
    .kind = ALM_KIND_INSTANCE,
    .size = sizeof(alm_object),
};

/* An object too big for a block, with one reference at the end. */
typedef struct big {
  alm_object header;
  int64_t data[512];
  alm_object *ref;
} big;

static const uint32_t big_refs[] = {
    offsetof(big, ref),
};

static const alm_type big_type = {
    .name = "Big",
    .kind = ALM_KIND_INSTANCE,
    .size = sizeof(big),
    .nrefs = 1,
    .refs = big_refs,
};

static const alm_type ref_array_type = {
    .name = "Object[]",
    .kind = ALM_KIND_REF_ARRAY,
    .elem_size = sizeof(alm_object *),
};

static const alm_type long_array_type = {
    .name = "long[]",
    .kind = ALM_KIND_PRIM_ARRAY,
    .elem_size = sizeof(int64_t),
};

static alm_gc_stats stats(void) {
  alm_gc_stats s;
  alm_get_gc_stats(&s);
  return s;
}

/* Allocates a node whose check field matches its value. */
static node *new_node(int64_t value) {
  node *n = (node *)alm_new(&node_type);
  if (n != NULL) {
    n->value = value;
    n->check = ~(uint64_t)value;
  }
  return n;
}

/* Like new_node(), but returns the node as a plain object pointer. */
static alm_object *new_ref(int64_t value) {
  node *n = new_node(value);
  return n == NULL ? NULL : &n->header;
}

static void test_lifecycle(void) {
  CHECK(alm_new(&object_type) == NULL);
  CHECK(alm_new_array(&long_array_type, 1) == NULL);
  alm_gc();

  CHECK(alm_init(NULL) == 0);
  CHECK(alm_new(&object_type) != NULL);
  alm_shutdown();
  CHECK(alm_new(&object_type) == NULL);
}

static void test_bad_types(void) {
  CHECK(alm_init(NULL) == 0);
  CHECK(alm_new(NULL) == NULL);
  CHECK(alm_new(&long_array_type) == NULL);
  CHECK(alm_new_array(&node_type, 1) == NULL);
  CHECK(alm_new_array(&long_array_type, -1) == NULL);

  static const alm_type bad_ref_array = {
      .kind = ALM_KIND_REF_ARRAY,
      .elem_size = 4,
  };
  CHECK(alm_new_array(&bad_ref_array, 1) == NULL);
  alm_shutdown();
}

static void test_new_is_zeroed(void) {
  CHECK(alm_init(NULL) == 0);
  node *a = (node *)alm_new(&node_type);
  CHECK(a != NULL);
  CHECK(a->header.type == &node_type);
  CHECK(a->next == NULL && a->payload == NULL && a->value == 0);
  CHECK((uintptr_t)a % 16 == 0);

  /* Fill a node with junk, let it die, and check its reused cell is clean. */
  a->value = -1;
  a->check = UINT64_MAX;
  alm_gc();
  node *b = (node *)alm_new(&node_type);
  CHECK(b != NULL);
  CHECK(b->value == 0 && b->check == 0U);
  alm_shutdown();
}

static void test_garbage_is_freed(void) {
  CHECK(alm_init(NULL) == 0);
  for (int i = 0; i < 10000; i++) {
    CHECK(new_node(i) != NULL);
  }
  alm_gc();
  alm_gc_stats s = stats();
  CHECK(s.live_objects == 0);
  CHECK(s.live_bytes == 0);
  CHECK(s.heap_bytes == 0);
  alm_shutdown();
}

static void test_rooted_list_survives(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[2] = {NULL, NULL};
  alm_frame frame = {.slots = slots, .count = 2};
  alm_push_frame(&frame);

  enum { COUNT = 10000 };
  for (int i = 0; i < COUNT; i++) {
    /* The payload must be rooted while the node is allocated. */
    slots[1] = alm_new(&object_type);
    CHECK(slots[1] != NULL);
    node *n = new_node(i);
    CHECK(n != NULL);
    n->payload = slots[1];
    n->next = (node *)slots[0];
    slots[0] = &n->header;
  }
  slots[1] = NULL;
  alm_gc();
  CHECK(stats().live_objects == (size_t)2 * COUNT);

  int64_t expect = COUNT - 1;
  for (node *n = (node *)slots[0]; n != NULL; n = n->next) {
    CHECK(n->value == expect && n->check == ~(uint64_t)expect);
    CHECK(n->payload != NULL && n->payload->type == &object_type);
    expect--;
  }
  CHECK(expect == -1);

  alm_pop_frame(&frame);
  alm_gc();
  CHECK(stats().live_objects == 0);
  alm_shutdown();
}

static void test_cycles(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[1] = {NULL};
  alm_frame frame = {.slots = slots, .count = 1};
  alm_push_frame(&frame);

  node *a = new_node(1);
  CHECK(a != NULL);
  slots[0] = &a->header;
  node *b = new_node(2);
  CHECK(b != NULL);
  a->next = b;
  b->next = a;

  alm_gc();
  CHECK(stats().live_objects == 2);
  CHECK(a->next == b && b->next == a && b->value == 2);

  /* Mark-sweep frees cycles, unlike reference counting. */
  slots[0] = NULL;
  alm_gc();
  CHECK(stats().live_objects == 0);

  alm_pop_frame(&frame);
  alm_shutdown();
}

static void test_arrays(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[2] = {NULL, NULL};
  alm_frame frame = {.slots = slots, .count = 2};
  alm_push_frame(&frame);

  enum { LEN = 300 };
  alm_array *refs = alm_new_array(&ref_array_type, LEN);
  CHECK(refs != NULL);
  CHECK(refs->length == LEN);
  CHECK(refs->header.type == &ref_array_type);
  slots[0] = &refs->header;
  alm_object **elems = (alm_object **)alm_array_data(refs);
  for (int i = 0; i < LEN; i++) {
    CHECK(elems[i] == NULL);
    elems[i] = new_ref(i);
    CHECK(elems[i] != NULL);
  }

  alm_array *longs = alm_new_array(&long_array_type, LEN);
  CHECK(longs != NULL);
  slots[1] = &longs->header;
  int64_t *values = alm_array_data(longs);
  CHECK((uintptr_t)values % 8 == 0);
  for (int i = 0; i < LEN; i++) {
    CHECK(values[i] == 0);
    values[i] = (int64_t)i * 3;
  }

  alm_array *empty = alm_new_array(&ref_array_type, 0);
  CHECK(empty != NULL && empty->length == 0);

  alm_gc();
  /* The ref array, its nodes, and the long array survive; empty does not. */
  CHECK(stats().live_objects == LEN + 2);
  for (int i = 0; i < LEN; i++) {
    node *n = (node *)elems[i];
    CHECK(n->value == i && n->check == ~(uint64_t)i);
    CHECK(values[i] == (int64_t)i * 3);
  }

  alm_pop_frame(&frame);
  alm_shutdown();
}

static void test_large_objects(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[1] = {NULL};
  alm_frame frame = {.slots = slots, .count = 1};
  alm_push_frame(&frame);

  for (int i = 0; i < 100; i++) {
    CHECK(alm_new(&big_type) != NULL);
  }
  big *kept = (big *)alm_new(&big_type);
  CHECK(kept != NULL);
  CHECK(kept->data[511] == 0);
  slots[0] = &kept->header;
  kept->ref = new_ref(7);
  CHECK(kept->ref != NULL);
  kept->data[0] = 42;

  alm_gc();
  alm_gc_stats s = stats();
  CHECK(s.live_objects == 2);
  CHECK(kept->data[0] == 42);
  CHECK(((node *)kept->ref)->value == 7);

  alm_array *huge = alm_new_array(&long_array_type, 100000);
  CHECK(huge != NULL && huge->length == 100000);
  CHECK(((int64_t *)alm_array_data(huge))[99999] == 0);

  alm_pop_frame(&frame);
  alm_gc();
  CHECK(stats().heap_bytes == 0);
  alm_shutdown();
}

static void test_deep_list(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[1] = {NULL};
  alm_frame frame = {.slots = slots, .count = 1};
  alm_push_frame(&frame);

  /* Deep enough that a recursive marker would overflow the C stack. */
  enum { DEPTH = 500000 };
  for (int i = 0; i < DEPTH; i++) {
    node *n = new_node(i);
    CHECK(n != NULL);
    n->next = (node *)slots[0];
    slots[0] = &n->header;
  }
  alm_gc();
  CHECK(stats().live_objects == DEPTH);

  alm_pop_frame(&frame);
  alm_shutdown();
}

static alm_object *global_slot;

static void test_global_roots(void) {
  CHECK(alm_init(NULL) == 0);
  CHECK(alm_add_root(&global_slot) == 0);
  global_slot = new_ref(99);
  CHECK(global_slot != NULL);

  alm_gc();
  CHECK(stats().live_objects == 1);
  CHECK(((node *)global_slot)->value == 99);

  global_slot = NULL;
  alm_gc();
  CHECK(stats().live_objects == 0);
  alm_shutdown();
}

static void test_nested_frames(void) {
  CHECK(alm_init(NULL) == 0);
  alm_object *outer_slots[1] = {NULL};
  alm_frame outer = {.slots = outer_slots, .count = 1};
  alm_push_frame(&outer);
  outer_slots[0] = alm_new(&object_type);

  alm_object *inner_slots[1] = {NULL};
  alm_frame inner = {.slots = inner_slots, .count = 1};
  alm_push_frame(&inner);
  inner_slots[0] = alm_new(&object_type);
  alm_object *deepest_slots[1] = {NULL};
  alm_frame deepest = {.slots = deepest_slots, .count = 1};
  alm_push_frame(&deepest);
  deepest_slots[0] = alm_new(&object_type);

  alm_gc();
  CHECK(stats().live_objects == 3);

  /* Popping inner also pops deepest, as an exception unwinding both would. */
  alm_pop_frame(&inner);
  alm_gc();
  CHECK(stats().live_objects == 1);

  alm_pop_frame(&outer);
  alm_shutdown();
}

static void test_out_of_memory(void) {
  alm_config config = {.max_heap = (size_t)1024 * 1024};
  CHECK(alm_init(&config) == 0);
  alm_object *slots[1] = {NULL};
  alm_frame frame = {.slots = slots, .count = 1};
  alm_push_frame(&frame);

  /* Keep everything alive until the heap is full. */
  int made = 0;
  for (;;) {
    node *n = new_node(made);
    if (n == NULL) {
      break;
    }
    n->next = (node *)slots[0];
    slots[0] = &n->header;
    made++;
  }
  CHECK(made > 1000);
  CHECK(stats().heap_bytes <= config.max_heap);
  CHECK(alm_new(&big_type) == NULL);
  CHECK(alm_new_array(&long_array_type, INT32_MAX) == NULL);

  /* Dropping the list makes room again. */
  slots[0] = NULL;
  CHECK(new_node(1) != NULL);
  CHECK(alm_new(&big_type) != NULL);

  alm_pop_frame(&frame);
  alm_shutdown();
}

static void test_freed_memory_is_poisoned(void) {
#ifdef TEST_ASAN
  CHECK(alm_init(NULL) == 0);
  alm_object *slots[1] = {NULL};
  alm_frame frame = {.slots = slots, .count = 1};
  alm_push_frame(&frame);

  /* Keep one node so its block stays, and drop its neighbor. */
  slots[0] = new_ref(1);
  node *dead = new_node(2);
  CHECK(slots[0] != NULL && dead != NULL);
  CHECK(!__asan_address_is_poisoned(dead));
  alm_gc();
  CHECK(__asan_address_is_poisoned(dead));
  CHECK(!__asan_address_is_poisoned(slots[0]));

  alm_pop_frame(&frame);
  alm_shutdown();
#endif
}

/* A small, fixed random number generator, so failures can be replayed. */
static uint64_t next_random(uint64_t *state) {
  *state ^= *state << 13U;
  *state ^= *state >> 7U;
  *state ^= *state << 17U;
  return *state;
}

/* Follows a chain of next pointers and checks every node is intact. */
static void check_chain(const node *n) {
  for (int steps = 0; n != NULL && steps < 64; steps++) {
    CHECK(n->header.type == &node_type);
    CHECK(n->check == ~(uint64_t)n->value);
    n = n->next;
  }
}

static void test_stress(void) {
  alm_config config = {.max_heap = (size_t)8 * 1024 * 1024};
  CHECK(alm_init(&config) == 0);
  enum { SLOTS = 64, STEPS = 1000000, GC_EVERY = 250000 };
  alm_object *slots[SLOTS] = {NULL};
  alm_frame frame = {.slots = slots, .count = SLOTS};
  alm_push_frame(&frame);

  uint64_t rng = 0x9E3779B97F4A7C15U;
  for (int step = 0; step < STEPS; step++) {
    uint64_t r = next_random(&rng);
    size_t i = r % SLOTS;
    size_t j = (r >> 8U) % SLOTS;
    switch ((r >> 16U) % 4) {
    case 0:
    case 1: {
      node *n = new_node((int64_t)(r >> 20U));
      CHECK(n != NULL);
      n->next = (node *)slots[j];
      slots[i] = &n->header;
      break;
    }
    case 2:
      if (slots[i] != NULL) {
        ((node *)slots[i])->next = (node *)slots[j];
      }
      break;
    default:
      slots[i] = NULL;
      break;
    }
    /* Check the graph often, including after collections that allocation
     * started on its own, and collect by hand now and then too. */
    if (step % GC_EVERY == 0) {
      alm_gc();
    }
    if (step % 1000 == 0) {
      for (size_t k = 0; k < SLOTS; k++) {
        check_chain((const node *)slots[k]);
      }
    }
  }
  /* Allocation must also have started collections on its own. */
  CHECK(stats().collections > STEPS / GC_EVERY);

  alm_pop_frame(&frame);
  alm_shutdown();
}

int main(void) {
  static const struct {
    const char *name;
    void (*run)(void);
  } tests[] = {
      {"lifecycle", test_lifecycle},
      {"bad_types", test_bad_types},
      {"new_is_zeroed", test_new_is_zeroed},
      {"garbage_is_freed", test_garbage_is_freed},
      {"rooted_list_survives", test_rooted_list_survives},
      {"cycles", test_cycles},
      {"arrays", test_arrays},
      {"large_objects", test_large_objects},
      {"deep_list", test_deep_list},
      {"global_roots", test_global_roots},
      {"nested_frames", test_nested_frames},
      {"out_of_memory", test_out_of_memory},
      {"freed_memory_is_poisoned", test_freed_memory_is_poisoned},
      {"stress", test_stress},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    tests[i].run();
    (void)printf("PASS %s\n", tests[i].name);
  }
  return 0;
}
