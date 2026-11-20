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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
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
 * Code produced by the compiler calls into these functions. Every exported symbol uses the alm_ prefix so it
 * cannot clash with symbols from other native libraries linked into the same program.
 */
#ifndef ALUMINUM_RUNTIME_H
#define ALUMINUM_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sets up the runtime. Must be called once, before any other alm_ function.
 *
 * Returns 0 on success, or a nonzero value if the runtime could not start.
 */
int alm_init(void);

/*
 * Tears the runtime down. Calling it when the runtime was never started does nothing.
 */
void alm_shutdown(void);

/*
 * Returns 1 if alm_init() has succeeded and alm_shutdown() has not been called since, or 0 otherwise.
 */
int alm_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
