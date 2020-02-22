/*
*Fixed input warnings
* Inder
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
    int 						id;
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;
const char* START = "Start_Alarm";
const char* CHANGE = "Change_Alarm";
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

void *display_alarm_thread (void *arg)
{
    alarm_t *display_alarm_list = (alarm_t*) arg;
    int sleep_time = 5;
    int status;
    time_t now;
    while (1) {
        //If thread's list is ever empty, make the thread exit.
        if (display_alarm_list == NULL)
            pthread_exit();

        //lock mutex before accessing
        status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");

        //Get current time
        now = time (NULL);

        //Across list of alarms on this thread
        alarm_t *alarm = *display_alarm_list;
        while (alarm!= NULL){

        //print out display message
        if (alarm->time - now > 0)
            printf("Alarm %d Printed by Alarm Display Thread %lu at %ld: %s\n", alarm->id,pthread_self(), now,alarm->message);
        alarm = alarm->link;
        }

        //Unlock mutex before sleep
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");

        //sleep for 5 seconds
        sleep (sleep_time);
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[256];
    alarm_t *alarm, **last, *next;
    pthread_t thread;
    char action[16];

    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into action command(%[^(]) up to the left 
         * parenthesis, id (%d) separated by parenthesis,
         * seconds (%d) and a message (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf (line, "%[^(] (%d) %d %128[^\n]", 
            action, &alarm->id, &alarm->seconds, alarm->message) < 4) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } 
        // Checks for valid request
        else if (strcmp(action, START) != 0 && 
		  				strcmp(action, CHANGE) != 0) {
		  		fprintf (stderr, "Bad command\n");
            free (alarm);		
		  } 
		  else {
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            alarm->time = time (NULL) + alarm->seconds;

				if (strcmp(action, START) == 0) {
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */
            last = &alarm_list;
            next = *last;
            while (next != NULL) {
                if (next->id >= alarm->id) {
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
            printf("Alarm %d Inserted by Main Thread %lu Into Alarm List at %ld: %d %s\n", alarm->id,pthread_self(), alarm->time,alarm->seconds, alarm->message);
         	}
         	else if (strcmp(action, CHANGE) == 0) {
         		last = &alarm_list;
         		next = *last;
         		while (next != NULL) {
						if (next->id == alarm->id) {
							next->seconds = alarm->seconds;
							strncpy(next->message, alarm->message, sizeof(alarm->message));	
							printf("Alarm %d Changed at %ld: %d %s\n", alarm->id, alarm->time, alarm->seconds, alarm->message);				         		
							break;
						}
						next = next->link;
         		}
					if (next == NULL)
						printf("No changes made\n");
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
