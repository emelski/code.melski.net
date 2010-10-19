// main.cpp --
//
// This file implements a simple character level markov text generator.  It
// reads sample text from standard input, then generates any number of
// K-order markov texts to a series of files.
//
//
// Usage:
//
//      markov [options] < sample_text
//
//          --order=K           Specify the number of preceeding tokens to 
//                              consider in determining the next token.
//                              Default is 3.
//          --outputsize=N      Number of bytes to output.  The generator will
//                              not necessarily generate exactly this many 
//                              bytes.  For example, it may wander too close
//                              to the end of the original input when
//                              generating output, and lose the ability to
//                              generate more text.  Also, the generator 
//                              always tries to end the file with a newline,
//                              so it will continue to generate output after
//                              hitting N bytes, until a newline is output
//                              or 2*N bytes have been output.  Default is
//                              10000 bytes.
//          --inputsize=N       Maximum number of bytes of input to read.
//                              Default is 5,000,000 bytes.
//          --setsize=N         Number of output samples to produce.  Files
//                              named "output.0" through "output.N-1" will
//                              be created.  Default is 1.
//
// Copyright (c) 2004-2009 Electric Cloud, Inc.
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Electric Cloud nor the names of its employees may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <getopt.h>
#include <unistd.h>

using namespace std;

static int K = 3;

//----------------------------------------------------------------------------
// strcmpIndirect
//
//      Compare two strings given by pointers to pointers to the strings.
//      This is just a simple wrapper around strcmp which accomodates an
//      extra level of indirection.
//----------------------------------------------------------------------------

int strcmpIndirect(const void *a, const void *b) {
    const char *stringA, *stringB;
    stringA = *((char **)a);
    stringB = *((char **)b);

    return strcmp(stringA, stringB);
}

// The following enum supplies integer values for our command-line options.

enum {
    MARKOV_OPTIONS_INPUT_SIZE = 500,
    MARKOV_OPTIONS_ORDER,
    MARKOV_OPTIONS_OUTPUT_SIZE,
    MARKOV_OPTIONS_NUMBER_OF_SAMPLES,
    MARKOV_OPTIONS_HELP
};

// Our list of commmand-line options.

static struct option MARKOV_OPTIONS[] = {
    { "inputsize",      1,      0,      MARKOV_OPTIONS_INPUT_SIZE },
    { "order",          1,      0,      MARKOV_OPTIONS_ORDER },
    { "outputsize",     1,      0,      MARKOV_OPTIONS_OUTPUT_SIZE },
    { "setsize",        1,      0,      MARKOV_OPTIONS_NUMBER_OF_SAMPLES },
    { "help",           0,      0,      MARKOV_OPTIONS_HELP },
    { 0,                0,      0,      0}
};

static void usage(char *argv[])
{
    char *tail = strrchr(argv[0], '/');
    if (tail == NULL) {
        tail = argv[0];
    } else {
        tail = tail + 1;
    }
    fprintf(stderr, 
"markov\n"
"Generate letter-level Markov text based on sample text read from standard\n"
"input, writing to output files named output.*.\n\n"
"Usage:\n"
"%s [options ...]\n\n"
"Valid options are:\n\n"
"    --order=K            Number of preceeding characters to consider when\n"
"                         generating the next character (default 3).\n"
"    --outputsize=N       Number of characters to generate in the output\n"
"                         file (default 10000).\n"
"    --setsize=N          Number of output files to generate (default 1).\n"
            "\n"
            , tail);
    return;
}

int main(int argc, char *argv[])
{
    int MAX_CHARS = 5000000;
    int OUTPUT_CHARS = 10000;
    int SET_SIZE = 1;
    extern char *optarg;

    bool done = false;
    while (!done) {
        switch (getopt_long(argc, argv, "", MARKOV_OPTIONS, 0)) {
            case MARKOV_OPTIONS_INPUT_SIZE: {
                MAX_CHARS = atoi(optarg);
                break;
            }

            case MARKOV_OPTIONS_ORDER: {
                K = atoi(optarg);
                break;
            }

            case MARKOV_OPTIONS_OUTPUT_SIZE: {
                OUTPUT_CHARS = atoi(optarg);
                break;
            }

            case MARKOV_OPTIONS_NUMBER_OF_SAMPLES: {
                SET_SIZE = atoi(optarg);
                break;
            }

            case MARKOV_OPTIONS_HELP: {
                usage(argv);
                return 1;
            }

            case -1: {
                done = true;
                break;
            }

            default: {
                fprintf(stderr, "bad option\n");
                usage(argv);
                return 1;
            }
        }
    }
    
    char *input = new char[MAX_CHARS];
    char **suffixes = new char *[MAX_CHARS];
    char *output = new char[OUTPUT_CHARS * 2];

    // Read as much input as we are willing to read.

    int bytesRead = read(0, input, MAX_CHARS - 1);
    if (bytesRead < 0) {
        fprintf(stderr, "markov: error reading input data, exiting.\n");
        return 1;
    }

    if (bytesRead == 0) {
        fprintf(stderr, "markov: no input data, exiting.\n");
        return 1;
    }

    input[bytesRead] = 0;

    // Set up the suffix array.  Each element in this array points to a 
    // distinct character in the input.

    fprintf(stderr, "markov: initializing suffix array.\n");
    for (int i = 0; i < bytesRead; ++i) {
        suffixes[i] = &(input[i]);
    }

    // Now sort the array to bring suffixes with similar prefixes together.
    // We'll use a qsort with a special strcmp that adds a level of 
    // indirection, so we can sort the pointers.

    fprintf(stderr, "markov: sorting suffix array.\n");
    qsort(suffixes, bytesRead, sizeof(char *), strcmpIndirect);

    // Seed the random number generator.

    struct timeval now;
    gettimeofday(&now, 0);
    srand(now.tv_usec);

    for (int current = 0 ; current < SET_SIZE ; ++current) {
        // Seed the output with a random sequence of K characters from the
        // input.

        int seedStart = (int) ((bytesRead / 2) * rand() / (RAND_MAX + 1.0));
        int index = 0;
        memset(output, 0, OUTPUT_CHARS * 2);

        for (index = 0; index < K; ++index) {
            output[index] = input[index + seedStart];
        }

        // Now output approximately OUTPUT_CHARS characters.  We say
        // approximately because we will only stop output if we have
        // just written a newline, to try to guarantee well-formed
        // output.  This means that in general, we will output more
        // than OUTPUT_CHARS characters.

        done = false;
        while (!done && (index < (OUTPUT_CHARS * 2))) {
            if (index % 250 == 0) {
                write(2, ".", 1);
            }

            // First search the suffix array for the first suffix with the
            // prefix output[index - K].

            char *prefix = &(output[index - K]);

            int l = -1;
            int u = bytesRead;
            while ((l + 1) != u) {
                int m = (l + u) / 2;
                if (strncmp(suffixes[m], prefix, K) < 0) {
                    l = m;
                } else {
                    u = m;
                }
            }

            // "u" now holds the index of the first suffix that
            // matches.  Of all the suffixes with this prefix, pick
            // one at random.

            int choice = 0;
            for (int i = 0; suffixes[u + i] 
                         && strncmp(suffixes[u + i], prefix, K) == 0; ++i) {
                if ((rand() % (i + 1)) == 0) {
                    choice = i;
                }
            }

            // Now take the K+1'th character of the chosen suffix.  If this
            // character is the null terminator, then we're done.  If this 
            // character is a newline and we have output at least OUTPUT_CHARS
            // characters, then we're done.  Otherwise, we loop.

            switch (suffixes[u + choice][K]) {
                case '\0':
                    // We happened to find the suffix that starts
                    // exactly K characters from the end of the input.
                    // If this is the only suffix that matches the
                    // prefix, then we're done, regardless of how many
                    // characters of output we've generated.
                    // Otherwise, we'll loop and try again.

                    if ((choice == 0) 
                            && (strncmp(suffixes[u + 1], prefix, K)) != 0) {
                        done = true;
                    }
                    break;

                case '\n':
                    if (index >= OUTPUT_CHARS) {
                        done = true;
                    }

                    // DELIBERATELY FALL THROUGH: WE WANT THE NEWLINE
                    // IN OUTPUT.

                default:
                    output[index] = suffixes[u + choice][K];
                    index++;
                    break;
            }
        }

        char filename[32];
        sprintf(filename, "output.%d", current);
        FILE *f = fopen(filename, "w");
        fprintf(f, "%s", output);
        fclose(f);

        write(2, "done\n", 5);
    }
    return 0;
}

