# SimpleShell code review

I did not modify any C source code. This review uses the latest autograder output plus local tests against the current workspace.

## Score estimate

The current functional score is exactly:

```text
20.00 / 20.00  basic operation
 2.86 / 20.00  error cases
 0.00 / 20.00  logging
--------------------------------
22.86 / 60.00  functional score
```

If the course adds 10 compile points and 10 style points, the maximum estimate from this submission is:

```text
22.86 + 10 + 10 = 42.86 / 100
```

The exact total depends on the instructor’s separate style score. The server confirms compilation, and the local rebuild also passes with `-Wall -Wextra -Wpedantic`.

## What is already working

- Basic commands and arguments: BATCH TESTS 0–1 passed.
- Pipelines, file redirection, and network redirection: BATCH TESTS 2–3 passed.
- `wait4()` now stores its return value and retries on `EINTR`.
- Parent-side `pipe()` and `fork()` failure paths now return `EXIT_FAILURE` instead of exiting immediately.
- Normal and crash logging have the expected tab-separated shape.
- The current crash branch sets `pipeline_failed = 1` and checks `log != NULL`.

## Confirmed local test results

I rebuilt with `make clean && make`; compilation passed without warnings.

### Basic command

Passed with status 0:

```text
hello
```

### Crash logging

Passed with logging enabled:

```text
EXIT    0:0    ...    CRASH        11
```

The shell returned status 1, which is appropriate for a failed batch pipeline.

### Missing command, missing file, and parse error

The shell continued to later commands and produced messages such as:

```text
execvp: No such file or directory
open: No such file or directory
Parse error at position 0: expected command before '|'
```

### Long pipeline / long input

Still failed locally:

```text
input exceeds max len of 1024 chars
input exceeds max len of 1024 chars
execvp: No such file or directory
```

The last message shows that an oversized logical line was split and a fragment was executed. This matches the server’s poor error-test results.

### Logging

A simple logging run produced valid records such as:

```text
START   0:0     printf hello
ENVREAD 0:0     LANG=C.UTF-8
EXIT    0:0     ...     NORMAL          0
```

This suggests the logging format itself is mostly correct. The logging tests are failing because the tested error pipelines do not complete with the expected error behavior before log validation.

## Fixes still needed

- **Fix oversized logical-line handling — interactive and batch input loops, around `clemsh.c:357-377` and `428-448`.**

  Current problems:

  - Interactive validation examines `input` before calling `fgets()`.
  - The batch condition only catches `len == MAX_INPUT_LENGTH`; a chunk with length greater than 1024 and no newline can bypass the discard path.
  - The rest of an oversized line is not always consumed, so later chunks become separate commands.

  Proposed order:

  1. Print the prompt if interactive.
  2. Call `fgets()`.
  3. Compute the length.
  4. If no newline was read, consume the rest of the same stream through the newline.
  5. Remove the newline before measuring the command length.
  6. Reject the complete line once if its command length exceeds 1024.
  7. Call `pipeline_parse()` only after the line passes validation.

  Use `stdin` in interactive mode and `batch` in batch mode. The buffer should be large enough for 1024 characters, a newline, and `\\0`:

  ```c
  char input[MAX_INPUT_LENGTH + 2];
  ```

  The expected result for an 11,000-character input is one length error and no `execvp` message.

- **Investigate the five-second timeouts — TCP error paths around `clemsh.c:59-75`, `225-233`, and `256-265`.**

  Each `getaddrinfo()` result can receive a three-second `poll()` timeout. A host with multiple address results can therefore take longer than the grader’s five-second limit. The comments also say five seconds while the code uses 3000 milliseconds.

  Proposed solution: enforce one total deadline for the whole connection attempt, or use a shorter bounded timeout per address. Always close failed sockets and return a child failure after the deadline.

- **Report failed or signaled child processes to stderr — wait loop around `clemsh.c:307-329`.**

  The code marks `pipeline_failed`, but it does not print a shell-generated message when a child exits nonzero or is killed by a signal. A test program can fail without writing anything to stderr, producing no error line. This likely explains why BATCH TEST 6 saw only 3 errors instead of 25 and why LOG TEST 9 saw only 1 instead of 8.

  Proposed solution:

  - For `WIFEXITED` with a nonzero status, print one concise message containing the pipeline/stage and status.
  - For `WIFSIGNALED`, print one concise message containing the pipeline/stage and signal.
  - Keep the `CRASH` log record separate from the stderr message.
  - Avoid printing duplicate shell messages for errors already reported by a failed `open()` or `execvp()` if the grader expects only one line.

- **Ensure error pipelines finish within the grader timeout.**

  BATCH TESTS 4 and 5 do not complete within five seconds. After fixing line consumption and TCP deadlines, retest pipelines containing:

  - missing executables;
  - missing input files;
  - invalid TCP targets;
  - commands that exit nonzero;
  - commands terminated by multiple signals;
  - long pipelines.

  The local 300-stage test completed, so the most likely remaining timeout source is an error-path TCP connection or a long-line fragment interacting with later commands. The exact hidden batch files are unavailable, so this conclusion is an inference from the visible results and local behavior.

- **Recheck logging after error handling is fixed.**

  LOG TESTS 7–9 say the pipeline must execute correctly before logs are checked. The normal `START`, `ENVREAD`, `ENVWRITE`, `EXIT NORMAL`, and `EXIT CRASH` formats appear structurally correct, but missing/incorrect stderr error handling is preventing those tests from reaching the logging assertions.

- **Verify Ctrl-C separately.**

  Interactive mode should return 130 after Ctrl-C and 0 after normal EOF. This is not identified as the main cause of the displayed batch failures, but it remains a required behavior.

## Correct test commands

Use a single backslash before `n`; double backslashes write a literal `\\n` into the batch file.

```sh
make clean && make

printf 'printf hello\n' > /tmp/clemsh-basic.txt
./clemsh /tmp/clemsh-basic.txt

printf '%s\n' 'sh -c "kill -SEGV $$"' > /tmp/clemsh-crash.txt
CLEMSHLOG=1 ./clemsh /tmp/clemsh-crash.txt

python3 -c 'print("printf " + "x" * 11000)' > /tmp/clemsh-long.txt
timeout 7 ./clemsh /tmp/clemsh-long.txt
```

## Priority order

1. Fix long-line consumption so no fragment executes.
2. Bound TCP error connection time.
3. Add missing shell-side error messages for nonzero and signaled children.
4. Rerun error tests.
5. Rerun logging tests after the error tests complete successfully.
