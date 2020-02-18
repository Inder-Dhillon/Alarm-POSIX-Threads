/*
 * THIS IS THE FILE WE'RE SUPPOSED TO WORK ON. NO CHANGES AS OF 2/17/2020
 * - David
 * cc alarm_mutex.c -D_POSIX_PTHREAD_SEMANTICS -lpthread
 */

/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <stdio.h>
#include <string.h>

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[128];
    char                keyword[16];
    int                 alarm_id;
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
        alarm = alarm_list;

        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0.
         */
        if (alarm == NULL)
            sleep_time = 1;
        else {
            alarm_list = alarm->link;
            now = time (NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                sleep_time, alarm->message);
#endif
            }

        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
        if (sleep_time > 0)
            sleep (sleep_time);
        else
            sched_yield ();

        /*
         * If a timer expired, print the message and free the
         * structure.
         */
        if (alarm != NULL) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[256];
    alarm_t *alarm, **last, *next;
    pthread_t thread;
    char keyword_and_id[32];
    char * tokens[3];
    int token_counter;
    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    else {
        printf("Alarm Thread Created New Display Alarm Thread <thread-id> For Alarm(<alarm_id>) at <create_time>: <time message>\n");
    }
    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf (line, "%s %d %128[^\n]", keyword_and_id,
            &alarm->seconds, alarm->message) != 3) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
            //parses input line into tokens
            token_counter = 0;
            char * token = strtok(keyword_and_id, "(");

            while (token != NULL && token_counter < 3){
              tokens[token_counter] = token;
              token_counter++;

              token = strtok(NULL, ")");
            }

            //checks for proper inputs
            if (token_counter != 2 || sscanf(tokens[1], "%d", &alarm->alarm_id) != 1
                || sscanf(tokens[0], "%s", alarm->keyword) != 1){
              fprintf (stderr, "Bad command\n");
              free(alarm);
            }
            else if (strcmp(tokens[0], "Start_Alarm") != 0 &&
                    strcmp(tokens[0], "Change_Alarm") != 0){
              fprintf (stderr, "Bad command\n");
              free(alarm);
            }
            else{
              status = pthread_mutex_lock (&alarm_mutex);
              if (status != 0)
                  err_abort (status, "Lock mutex");
              alarm->time = time (NULL) + alarm->seconds;

              /*
               * Insert the new alarm into the list of alarms,
               * sorted by expiration time.
               */
              last = &alarm_list;
              next = *last;
              while (next != NULL) {
                  if (next->time >= alarm->time) {
                      alarm->link = next;
                      *last = alarm;
                      break;
                  }
                  last = &next->link;
                  next = next->link;
              }
              /*
               * If we reached the end of the list, insert the new
               * alarm there. ("next" is NULL, and "last" points
               * to the link field of the last item, or to the
               * list header).
               */
              if (next == NULL) {
                  *last = alarm;
                  alarm->link = NULL;
              }
              #ifdef DEBUG
              printf ("[list: ");
              for (next = alarm_list; next != NULL; next = next->link)
                  printf ("%d(%d)[\"%s\"] ", next->time,
                      next->time - time (NULL), next->message);
              printf ("]\n");
              #endif
              status = pthread_mutex_unlock (&alarm_mutex);
              if (status != 0)
                  err_abort (status, "Unlock mutex");
              }
          }
      }
}
