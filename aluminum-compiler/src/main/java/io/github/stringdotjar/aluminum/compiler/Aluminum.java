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
package io.github.stringdotjar.aluminum.compiler;

import java.io.PrintStream;

/**
 * Command-line entry point for the Aluminum compiler.
 *
 * <p>The real work happens in {@link #run(String[], PrintStream)}, which returns an exit code instead of calling
 * {@link System#exit(int)}. That keeps the compiler easy to test and to call from other tools, such as a Gradle
 * plugin.
 */
public final class Aluminum {

  /** Exit code for a run that finished without errors. */
  public static final int EXIT_OK = 0;

  /** Exit code for a run that was given arguments it does not understand. */
  public static final int EXIT_USAGE = 2;

  private Aluminum() {}

  public static void main(String[] args) {
    int code = run(args, System.out);
    if (code != EXIT_OK) {
      System.exit(code);
    }
  }

  /**
   * Runs the compiler with the given command-line arguments.
   *
   * @param args The command-line arguments.
   * @param out Where to print messages for the user.
   * @return {@link #EXIT_OK} on success, or another exit code describing what went wrong.
   */
  public static int run(String[] args, PrintStream out) {
    if (args.length == 0 || "--help".equals(args[0])) {
      printUsage(out);
      return args.length == 0 ? EXIT_USAGE : EXIT_OK;
    }
    out.println("Unknown argument: " + args[0]);
    printUsage(out);
    return EXIT_USAGE;
  }

  private static void printUsage(PrintStream out) {
    out.println("Usage: aluminum [--help]");
  }
}
