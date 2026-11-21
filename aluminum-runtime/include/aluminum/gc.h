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
 * Object allocation and garbage collection.
 *
 * The collector is a precise, non-moving, stop-the-world mark-sweep collector.
 * "Precise" means it never guesses: it only follows pointers it was told about,
 * either through root frames (alm_push_frame()), global roots (alm_add_root()),
 * or the reference offsets in each object's alm_type. Objects never move, so a
 * pointer to an object stays valid for as long as the object is reachable.
 *
 * For now only one thread may use the heap. Every function here must be called
 * from the thread that called alm_init().
 */
#ifndef ALUMINUM_GC_H
#define ALUMINUM_GC_H

#include <stddef.h>
#include <stdint.h>

#include "aluminum/object.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A group of root slots belonging to one function call.
 *
 * A function that holds object pointers in local variables keeps them in an
 * array of slots, describes that array with a frame, and pushes the frame
 * while it runs. The collector treats every non-NULL slot of every pushed
 * frame as alive. Frames form a stack through prev, newest first, which is
 * why this is called a shadow stack: it shadows the real call stack.
 */
typedef struct alm_frame {
  /* The frame pushed before this one. Set by alm_push_frame(). */
  struct alm_frame *prev;

  /* The root slots. Each one is either NULL or points at a live object. */
  alm_object **slots;

  /* Number of slots. */
  uint32_t count;
} alm_frame;

/* Numbers that describe the heap, filled in by alm_get_gc_stats(). */
typedef struct alm_gc_stats {
  /* Number of collections since alm_init(). */
  uint64_t collections;

  /* Bytes the heap currently holds from the system, used or not. */
  size_t heap_bytes;

  /* Bytes of objects that survived the most recent collection. */
  size_t live_bytes;

  /* Number of objects that survived the most recent collection. */
  size_t live_objects;
} alm_gc_stats;

/*
 * Allocates a new instance of a class.
 *
 * The object is zeroed, and its header points at type. Allocating may run a
 * collection first, so any object pointer not held in a root may be freed by
 * this call.
 *
 * Returns the new object, or NULL if type is not an instance type, the
 * runtime is not running, or there is no memory left even after collecting.
 */
alm_object *alm_new(const alm_type *type);

/*
 * Allocates a new array with length elements.
 *
 * The elements are zeroed. Like alm_new(), this may run a collection first.
 *
 * Returns the new array, or NULL if type is not an array type, length is
 * negative, the runtime is not running, or there is no memory left even after
 * collecting.
 */
alm_array *alm_new_array(const alm_type *type, int32_t length);

/*
 * Runs a full collection now.
 *
 * Every object that cannot be reached from a root frame or a global root is
 * freed. Does nothing if the runtime is not running.
 */
void alm_gc(void);

/*
 * Pushes a frame of root slots onto the shadow stack.
 *
 * The caller owns the frame and its slots, which usually live in local
 * variables, and must set slots and count before pushing. The slots must stay
 * valid until the frame is popped.
 */
void alm_push_frame(alm_frame *frame);

/*
 * Pops a frame and every frame pushed after it.
 *
 * Popping several frames at once is what exception unwinding needs, when a
 * throw leaves more than one function at a time.
 */
void alm_pop_frame(alm_frame *frame);

/*
 * Registers a global slot, such as a static field, as a root.
 *
 * The slot must stay valid until alm_shutdown(). Global roots cannot be
 * removed, because static fields live for the whole program.
 *
 * Returns 0 on success, or a nonzero value if there was no memory to record
 * the slot.
 */
int alm_add_root(alm_object **slot);

/* Copies the current heap numbers into stats. */
void alm_get_gc_stats(alm_gc_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
