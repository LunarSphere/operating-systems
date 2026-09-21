# How to improve the SimpleShell autograder result

This guide is based on the latest autograder output and local tests. It describes changes for you to make. No C source code was modified while creating this file.

## Current score

The current functional score is:

- Basic operation: 20.00/20.00
- Error handling: 2.86/20.00
- Logging: 0.00/20.00
- Functional total: 22.86/60.00

If the course adds 10 compile points and 10 style points, the best-case total from this submission is about 42.86/100.

Normal commands, arguments, pipelines, file redirection, and successful network redirection are working. The remaining failures are concentrated in error recovery and oversized input.

## Step 1: Verify the submitted source

Run:

    make clean
    make

The build must finish without warnings or errors. Recreate the archive after the final changes. A local change does not affect an archive already submitted to the once-per-day grader.

## Step 2: Fix oversized logical lines

Your local 11,000-character test produced multiple length errors and eventually:

    execvp: File name too long

That means one oversized input line is being split into several commands.

### 2a. Make the buffer large enough

Near the input declaration, use:

    char input[MAX_INPUT_LENGTH + 2];

This allows 1024 command characters, a newline, and the terminating null byte.

### 2b. Read before checking the length

In interactive mode, print the prompt and call fgets() before calling strlen() or strchr():

    printf("> ");
    fflush(stdout);
    if (fgets(input, sizeof input, stdin) == NULL) {
        /* handle Ctrl-C, EOF, or a real read error */
    }

Do not inspect input before fgets() fills it.

### 2c. Consume the rest of an oversized line

After fgets(), use this pattern in the interactive loop:

    size_t len = strlen(input);
    int ch;

    if (strchr(input, '\n') == NULL) {
        ch = fgetc(stdin);
        if (ch != '\n' && ch != EOF) {
            while (ch != '\n' && ch != EOF) {
                ch = fgetc(stdin);
            }
            fprintf(stderr, "input exceeds max len of 1024 chars\n");
            continue;
        }
    }

    if (len > 0 && input[len - 1] == '\n') {
        input[--len] = '\0';
    }

    if (len > MAX_INPUT_LENGTH) {
        fprintf(stderr, "input exceeds max len of 1024 chars\n");
        continue;
    }

Use the same logic in the batch loop, replacing stdin with batch. Do not call pipeline_parse() after rejecting the line.

## Step 3: Report failed and signaled children

In the wait4() result handling, around the WIFEXITED and WIFSIGNALED branches:

    if (WIFEXITED(state)) {
        int status = WEXITSTATUS(state);
        if (status != 0) {
            pipeline_failed = 1;
            fprintf(stderr,
                    "pipeline %d stage %zu exited with status %d\n",
                    pipeline_number, i, status);
        }
        /* write the NORMAL log record when logging is enabled */
    } else if (WIFSIGNALED(state)) {
        int signal_number = WTERMSIG(state);
        pipeline_failed = 1;
        fprintf(stderr,
                "pipeline %d stage %zu terminated by signal %d\n",
                pipeline_number, i, signal_number);
        /* write the CRASH log record when logging is enabled */
    }

Keep if (log != NULL) around log writes. A child can fail without printing anything itself, but the shell still needs to report the failure. This likely explains why the grader expected 25 errors but saw only 3.

## Step 4: Keep error pipelines moving

For missing commands, bad files, parser errors, bad TCP endpoints, nonzero exits, and signal crashes:

- Print one concise stderr message.
- Mark the pipeline failed.
- Close file descriptors.
- Wait for already-created children.
- Continue with the next batch line.

Parent-side pipe() and fork() failures should return from execute_pipeline():

    return EXIT_FAILURE;

Keep child-side setup and execvp() failures as _exit(EXIT_FAILURE).

## Step 5: Bound TCP error time

The connection helper gives each address a three-second polling timeout. A host with multiple addresses can exceed the grader's five-second limit.

Use a total deadline for all addresses, or a shorter bounded timeout per address. Close failed sockets and let the child terminate so the parent can finish waiting. The comments currently say five seconds while the code uses 3000 milliseconds; make the intended limit consistent.

## Step 6: Verify Ctrl-C and EOF

Normal EOF should return 0:

    printf 'printf ok\n' | ./clemsh
    echo $?

Ctrl-C at the interactive prompt should return 130. Treat an interrupted fgets() as Ctrl-C rather than a generic read failure.

## Step 7: Run focused regression tests

Use a single backslash before n; double backslashes write literal backslash characters into the batch file.

    make clean
    make

    printf 'printf hello\n' > /tmp/basic.txt
    ./clemsh /tmp/basic.txt

    printf '%s\n' 'sh -c "kill -SEGV $$"' > /tmp/crash.txt
    CLEMSHLOG=1 timeout 5 ./clemsh /tmp/crash.txt
    grep CRASH clemshlog.txt

    python3 -c 'print("printf " + "x" * 11000)' > /tmp/long.txt
    timeout 5 ./clemsh /tmp/long.txt

Expected for the long-line test: one length error and no execvp error.

Test child failure recovery:

    printf 'sh -c "exit 7"\nprintf after\n' > /tmp/child-error.txt
    timeout 5 ./clemsh /tmp/child-error.txt

Expected: one descriptive error, after output, and completion within five seconds.

## Step 8: Rebuild and resubmit

After the focused tests pass:

    make clean
    make

Create a fresh archive from the current source files and Makefile. The current 22.86/60 result will not change until a new archive is submitted and graded.

## Priority

1. Consume oversized lines completely.
2. Print missing nonzero-exit and signal errors.
3. Bound TCP error time and verify error pipelines cannot hang.
4. Rerun the error tests.
5. Rerun the logging tests after error pipelines complete successfully.
