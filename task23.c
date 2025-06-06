#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define DEBOUNCE_MS 500 // Debounce delay in milliseconds

// Global variables
volatile int latest_key = 0;
volatile int pending_event = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
timer_t timerid;
pthread_t input_thread;

// Timer handler
void timer_handler(int sig, siginfo_t *si, void *uc) {
    pthread_mutex_lock(&mutex);
    if (pending_event) {
        time_t now = time(NULL); // Store the current time in a variable
        printf("Processed key: %c at %s", latest_key, ctime(&now));
        pending_event = 0;
    }
    pthread_mutex_unlock(&mutex);
}

// Worker thread for keyboard input
void *input_worker(void *arg) {
    char key;
    printf("Enter keys (press Ctrl+C to exit). Debounced after %dms...\n", DEBOUNCE_MS);
    while (1) {
        key = getchar();
        if (key == EOF) break; // Exit on Ctrl+C or EOF
        pthread_mutex_lock(&mutex);
        latest_key = key;
        pending_event = 1;
        // Reset the timer
        struct itimerspec its;
        its.it_value.tv_sec = DEBOUNCE_MS / 1000;
        its.it_value.tv_nsec = (DEBOUNCE_MS % 1000) * 1000000;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0; // One-shot timer
        if (timer_settime(timerid, 0, &its, NULL) == -1) {
            fprintf(stderr, "Failed to reset timer\n");
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    struct sigevent sev;
    struct sigaction sa;

    // Set up signal handler for timer
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Block SIGRTMIN in main thread (inherited by input thread)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("pthread_sigmask");
        exit(1);
    }

    // Create the timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(1);
    }

    // Start input worker thread
    if (pthread_create(&input_thread, NULL, input_worker, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Wait for input thread to finish
    pthread_join(input_thread, NULL);

    // Cleanup
    timer_delete(timerid);
    printf("Program terminated.\n");
    return 0;
}
