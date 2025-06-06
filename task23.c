#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>

#define DEBOUNCE_INTERVAL_MS 300

volatile sig_atomic_t debounce_flag = 0;
timer_t debounce_timer;

// Restore original terminal settings
struct termios orig_termios;

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// Set terminal to raw mode for non-blocking key reading
void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal_mode);
    new_termios = orig_termios;

    new_termios.c_lflag &= ~(ICANON | ECHO); // Raw input mode
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

// Set stdin to non-blocking mode
void set_nonblocking_mode() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Timer signal handler
void timer_handler(int sig) {
    debounce_flag = 0;
}

// Start or restart the debounce timer
void start_debounce_timer() {
    struct itimerspec ts;
    ts.it_value.tv_sec = DEBOUNCE_INTERVAL_MS / 1000;
    ts.it_value.tv_nsec = (DEBOUNCE_INTERVAL_MS % 1000) * 1000000;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    timer_settime(debounce_timer, 0, &ts, NULL);
    debounce_flag = 1;
}

int main() {
    struct sigaction sa;
    struct sigevent sev;

    printf("Debounce key handler with POSIX timer (FreeBSD)\n");
    printf("Press keys to test. Press 'q' to quit.\n");

    // Set terminal to raw, non-blocking input mode
    set_conio_terminal_mode();
    set_nonblocking_mode();

    // Setup signal handler for timer
    sa.sa_flags = 0;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    // Create POSIX timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &debounce_timer;

    if (timer_create(CLOCK_REALTIME, &sev, &debounce_timer) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    while (1) {
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);

        if (n > 0) {
            if (!debounce_flag) {
                printf("Key pressed: %c\n", ch);
                start_debounce_timer();

                if (ch == 'q') {
                    break;
                }
            } else {
                // Debounced key - ignored
            }
        }

    }

    timer_delete(debounce_timer);
    return 0;
}
