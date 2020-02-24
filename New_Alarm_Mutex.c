/*
*Authors:
*	Aggrim Arora
*	Arnesh Dawar
*	Inderpal Dhillon
*	Rui Huang
*	Xiahan Chen
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

typedef struct node_tag{
	alarm_t				*item;
	struct node_tag	*next;
} node;


pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;
const char* START = "Start_Alarm";
const char* CHANGE = "Change_Alarm";
// Variables for display and core alarm thread
int d_size[3];
int d_min = 0;
int d_active = 0;
int d_curr = 0;
int bufAt[3] = {0,0,0};
alarm_t* (buf[3][10]);
alarm_t *d_alarm = NULL;
node *data[3]; 
/*
* The display alarm thread's start routine
*/
void *display_alarm_thread(void *arg)
{
	int id = d_curr;
	node* dat = data[id];
	
	while(1){
		
		if (dat->item->time <= time(NULL)){
				printf("Alarm Thread Terminated Display Thread %ld at %ld.\n", pthread_self(), time(NULL));
				pthread_exit(NULL);
				
		}
	
		printf("Alarm (%d) Printed by Alarm Display Thread %ld at %ld: %d %s.\n", 
				dat->item->id, pthread_self(), time(NULL), dat->item->seconds, dat->item->message);
		sleep(5);
	}
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time = 0;
    time_t now;
    int status;
    // Variables to control display
    int d_status;
    pthread_t display[3];
	 int i; 
	 node *ptr;
    
    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1) {
    			// Check and/or generate display threads to allocate alarms to
				// pthread_create (&thread, NULL, alarm_thread, NULL);
				// Check for open slot for pthread
    		if (d_alarm != NULL){
 				if (d_active < 3){
					for (i = 0; i < 3; i ++)
						if (d_size[i] == 0){
							d_curr = i;
							break;					
						}
			   	d_status = pthread_create (&display[d_curr], NULL, display_alarm_thread, NULL);
			   	if (d_status != 0)
			   		err_abort(d_status, "Did not create");
			   	// allocate new space for memory
			   	data[d_curr] = (node*)malloc(sizeof(node));
					data[d_curr]->item = d_alarm;		   		
			   	data[d_curr]->next = NULL;	
			   	// integer calculations for faster processing
			   	d_active += 1;
			   	d_size[d_active] += 1;
			   	printf("Alarm Thread Created New Display Alarm Thread %ld For Alarm(%d) at %ld: %d %s.\n",
			   			display[d_curr], d_alarm->id, now, d_alarm->seconds, d_alarm->message );
			   	// Wait for new input
			   	d_alarm = NULL;
        		}
        		else{
        			// Check for Minimum size list
        			d_min = d_size[0];
        			d_curr = 0;
					for (i = 1; i < 3; i ++)
							if (d_size[i] < d_min){
								d_min = d_size[i];
								d_curr = i;					     		
					     		}
					ptr = data[d_curr];
					// Append on to the list with a new node
					while (ptr != NULL)
						ptr = ptr->next;
					ptr = (node*)malloc(sizeof(node));
					ptr->item = d_alarm;		   		
			   	ptr->next = NULL;	
						
        			printf("Alarm Thread Display Alarm Thread %ld Assigned to Display Alarm(%d) at %ld: %d %s.",
			   			display[d_curr], d_alarm->id, now, d_alarm->seconds, d_alarm->message );
			   	// Wait for new input
			   	d_alarm = NULL;
        		}   		
    		}
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
		  if (alarm != NULL)
            	sleep (1);
        else
            sched_yield ();

        /*
         * If a timer expired, print the message and free the
         * structure.
         */
 			// Free head of list if it expires
			if (alarm != NULL)
			if (alarm->time <= time(NULL))
			{
				printf("Alarm Thread Removed Alarm(%d) at %ld: %d %s.", alarm->id, time(NULL), alarm->seconds, alarm->message);
				free(alarm);  
			} 	
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
             * sorted by id.
             */
             // preemptive check for new alarms
            d_alarm = alarm;
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