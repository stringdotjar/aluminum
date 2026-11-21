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
 * Public interface of the Aluminum runtime.
 *
 * Code produced by the compiler calls into these functions. Every exported
 * symbol uses the alm_ prefix so it cannot clash with symbols from other native
 * libraries linked into the same program.
 */
#ifndef ALUMINUM_RUNTIME_H
#define ALUMINUM_RUNTIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Settings for alm_init(). Zero in any field means "use the default". */
typedef struct alm_config {
  /*
   * Largest number of bytes the heap may take from the system. Once it is
   * reached and a collection frees nothing, allocation returns NULL. Zero
   * means no limit other than what the system can give.
   */
  size_t max_heap;
} alm_config;

/*
 * Sets up the runtime. Must be called once, before any other alm_ function.
 *
 * config may be NULL to use the defaults. Calling this while the runtime is
 * already running does nothing and ignores config.
 *
 * Returns 0 on success, or a nonzero value if the runtime could not start.
 */
int alm_init(const alm_config *config);

/*
 * Tears the runtime down and frees every object on the heap. Calling it when
 * the runtime was never started does nothing.
 */
void alm_shutdown(void);

/*
 * Returns 1 if alm_init() has succeeded and alm_shutdown() has not been called
 * since, or 0 otherwise.
 */
int alm_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
