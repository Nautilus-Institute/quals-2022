#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define DEFAULT_TIMEOUT 5

/*
 * Handles receiving the SIGALRM signal.
 */
void alarm_handler(int sig) {
    puts("Time's up!");
    exit(1);
}

/*
 * Performs the actual challenge logic.
 */
bool run_challenge() {
    // ask the user a math problem
    unsigned int x = rand() % INT_MAX;
    unsigned int y = rand() % INT_MAX;
    printf("%d + %d = ", x, y);
    fflush(stdout);

    // return whether the answer was right or not
    unsigned int z = 0;
    scanf("%d", &z);
    return x + y == z;
}

/*
 * Main function.
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char flag[128];
    unsigned int timeout;
    bool success;

    // set flag
    int fd = open("flag", O_RDONLY);
    unsigned int length = read(fd, flag, 127);
    flag[length] = 0;
    close(fd);

    // set seed
    srand(time(NULL));

    // set timeout from environment
    char *begin = getenv("TIMEOUT");
    if (begin != NULL) {
        char *end = begin + strlen(getenv("TIMEOUT"));
        timeout = strtoul(begin, &end, 10);
        if (begin == end || errno == ERANGE || errno == EINVAL) {
            timeout = DEFAULT_TIMEOUT;
        }
    } else {
        timeout = DEFAULT_TIMEOUT;
    }

    // set alarm
    signal(SIGALRM, alarm_handler);
    alarm(timeout);

    // run challenge
    success = run_challenge();

    // turn off alarm
    signal(SIGALRM, SIG_IGN);

    // check for success
    if (success) {
        puts("Correct!\nHere's your flag:");
        printf("%s\n", flag);
    } else {
        puts("Incorrect.\nBetter luck next time! :(");
    }

    return 0;
}
