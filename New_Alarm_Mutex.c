#include "New_Alarm_Mutex.h"

/*
 * New_Alarm_Mutex.c
 *
 *This C application introduces an advanced alarm management and viewing system,
 *building upon the foundation of the original `alarm_thread.c` program. The
 *program is designed to efficiently handle alarms using the main thread for
 *alarm creation, replacement, and deletion and dynamically created and removed
 *display threads for viewing alarm data. Each display thread is responsible for
 *processing and showcasing alarms within its assigned group number, leading to
 *a more streamlined and organized alarm handling process. Key features include
 *mutex-protected access to the alarm list, an optimized sleeping mechanism for
 *each display thread, and a set of commands for creating, replacing, and
 *canceling alarms.
 */

// Cleanup handler function
void cleanup_handler(void *arg) {
  pthread_mutex_lock(&display_alarm_mutex);
  num_display_reading--;
  // Last display thread
  if (num_display_reading == 0) {
    pthread_mutex_unlock(&alarm_mutex);
  }
  pthread_mutex_unlock(&display_alarm_mutex);
}

void *display_alarm(void *arg) {
  // Extract thread arguments
  ThreadArguments *thread_args = (ThreadArguments *)arg;
  int alarm_group = thread_args->alarm_group;
  pthread_cond_t condition = thread_args->condition;
  free(arg);

  // Initialize condition variable
  pthread_cond_init(&condition, NULL);
  sleep(1); // Allow time for initialization, since alarm time >=1

  while (1) {
    // Register a cleanup handler to unlock the mutex
    pthread_cleanup_push(cleanup_handler, NULL);

    // Increment the count of display threads
    pthread_mutex_lock(&display_alarm_mutex);
    num_display_reading++;

    // If this is the first display thread, lock the alarm (writer) mutex
    if (num_display_reading == 1) {
      pthread_mutex_lock(&alarm_mutex);
    }
    pthread_mutex_unlock(&display_alarm_mutex);

    // Variables to find the closest alarm for display
    alarm_t *curr_alarm = alarm_list;
    alarm_t *closest_alarm = NULL;
    time_t closest_expiration_time = 0;

    // Iterate through the alarm list to find the closest alarm
    while (curr_alarm != NULL) {
      if ((curr_alarm->seconds + 4) / 5 == alarm_group) {
        time_t expiration_time = curr_alarm->time + curr_alarm->seconds;
        if (closest_alarm == NULL ||
            expiration_time < closest_expiration_time) {
          closest_alarm = curr_alarm;
          closest_expiration_time = expiration_time;
        }
      }
      curr_alarm = curr_alarm->link;
    }

    // Calculate the time to sleep until the closest alarm
    int sleep_seconds = closest_expiration_time - time(NULL);

    // Continue the loop to recheck the list if the condition was not met
    while (sleep_seconds > 0) {
      // Use the passed condition variable for timed_wait
      struct timespec sleep_time = {sleep_seconds, 0};

      // Perform timed wait, or unless signaled
      int wait_result =
          pthread_cond_timedwait(&condition, &alarm_mutex, &sleep_time);

      // Recheck the list after waking up from the wait
      curr_alarm = alarm_list;

      if (wait_result == ETIMEDOUT) {
        // Break out of the loop when the condition is met and time is expired
        break;
      }

      // Recalculate sleep time
      sleep_seconds = closest_expiration_time - time(NULL);
    }

    // If the closest alarm's time has expired, display it
    if (closest_expiration_time <= time(NULL) && closest_alarm != NULL) {
      // Update the added time to the current time
      closest_alarm->time = time(NULL);

      // Display the alarm information
      printf("Alarm(%d) Displayed by Display Thread %lu for "
             "Alarm_Time_Group_Number %d at %ld: %s\n",
             closest_alarm->alarm_id, pthread_self(), alarm_group, time(NULL),
             closest_alarm->message);
    }

    // Reading is done, pop the cleanup handler, unlocking the mutex
    pthread_cleanup_pop(1);
  }

  // Cleanup, destroy the condition variable
  pthread_cond_destroy(&condition);

  return NULL;
}

void insert_alarm(alarm_t *alarm) {
  int status;
  alarm_t **last, *next;

  // lock the alarm list mutex
  status = pthread_mutex_lock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Lock mutex");
  }

  // last is the header of list, next is the first element in list
  last = &alarm_list;
  next = *last;

  while (next != NULL) {
    if (alarm->alarm_id == next->alarm_id) {
      // An alarm with the same ID already exists, don't insert the new alarm
      printf("An alarm with ID %d already exists.\n", alarm->alarm_id);
      free(alarm); // Free the new alarm
      status = pthread_mutex_unlock(&alarm_mutex);
      if (status != 0) {
        err_abort(status, "Unlock mutex");
      }
      return; // Return without inserting the new alarm
    } else if (alarm->alarm_id < next->alarm_id) {
      // Insert before next, update header
      alarm->link = next;
      *last = alarm;
      break;
    } else {
      // Move to the next item
      last = &next->link;
      next = *last;
    }
  }

  /*
   * If we reach the end of the list, insert the alarm there, next == NULL,
   * and last points to the last item or the list header.
   */
  if (next == NULL) {
    *last = alarm;
    alarm->link = NULL;
  }
  alarm->time = time(NULL);
  printf("Alarm(%d) Inserted by Main Thread %ld Into Alarm List at %ld: "
         "%d %s\n",
         alarm->alarm_id, pthread_self(), time(NULL), alarm->seconds,
         alarm->message);

  // Unlock the alarm list mutex
  status = pthread_mutex_unlock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Unlock mutex");
  }

  int alarm_time_group =
      (alarm->seconds + 4) / 5; // Calculate Alarm_Time_Group_Number

  // Check for an existing or create a display thread
  create_or_check_display_alarm_thread(alarm_time_group, alarm);
}

void create_or_check_display_alarm_thread(int alarm_group, alarm_t *alarm) {
  int status;

  // Check if a display alarm thread for this group exists
  display_alarm_info_t *current = display_alarm_threads;

  while (current != NULL) {
    if (current->alarm_time_group == alarm_group) {
      current->alarms_in_group = current->alarms_in_group + 1;
      // Signal it to recheck its list for an earlier printing alarm
      pthread_cond_signal(&current->condition);
      return; // Found an existing display alarm thread
    }
    current = current->next;
  }

  // If no existing thread is found, create a new one
  if (current == NULL) {
    // Create a new display alarm thread structure
    display_alarm_info_t *new_thread_info =
        (display_alarm_info_t *)malloc(sizeof(display_alarm_info_t));
    if (new_thread_info == NULL) {
      perror("Failed to allocate memory for new display alarm thread");
    }
    // Set the alarm group number and number of alarms in the group
    new_thread_info->alarm_time_group = alarm_group;
    new_thread_info->alarms_in_group = 1;

    // Add the new display thread at the beginning of list
    new_thread_info->next = display_alarm_threads;
    display_alarm_threads = new_thread_info;

    // Allocate memory for the thread arguments
    ThreadArguments *thread_args = malloc(sizeof(ThreadArguments));
    if (thread_args == NULL) {
      perror("Failed to allocate memory for thread arguments");
      exit(1);
    }
    // Set the values in the structure for display thread
    thread_args->alarm_group = alarm_group; // alarm group for thread
    thread_args->condition =
        new_thread_info->condition; // conditon variable for thread

    status = pthread_create(&new_thread_info->thread, NULL, display_alarm,
                            thread_args);
    if (status != 0) {
      err_abort(status, "Create display alarm thread");
    }

    // Print the creation message
    printf("Created New Display Alarm Thread %lu for Alarm_Time_Group_Number "
           "%d to Display Alarm(%d) at %ld: %d %s\n",
           new_thread_info->thread, alarm_group, alarm->alarm_id, time(NULL),
           alarm->seconds, alarm->message);
  }
}

void check_or_remove_display_thread(int alarm_group) {
  int status;

  display_alarm_info_t *prev = NULL;
  display_alarm_info_t *current = display_alarm_threads;

  // Check if there are any alarms left in the replaced alarm's group
  while (current != NULL) {
    if (current->alarm_time_group == alarm_group) {
      // If checked then remove one alarm from group for replace or cancel
      current->alarms_in_group = current->alarms_in_group - 1;
      if (current->alarms_in_group == 0) {
        // Terminate the display alarm thread and remove from the list
        status = pthread_cancel(current->thread);

        if (status == 0) {
          printf("Display Alarm Thread %lu for Alarm_Time_Group_Number %d "
                 "Terminated at %ld\n",
                 current->thread, alarm_group, time(NULL));

          if (prev == NULL) {
            display_alarm_threads = current->next;
          } else {
            prev->next = current->next;
          }

          free(current);
        }
      } else {
        // if not removed, signal the thread to recheck the list since one of
        // its alarms are removed
        pthread_cond_signal(&current->condition);
      }

      break;
    }

    prev = current;
    current = current->next;
  }
}

void replace_alarm(alarm_t *alarm) {
  int status;
  alarm_t *alarm_to_replace = NULL;
  int replaced_alarm_group = -1;

  // Lock the alarm mutex to access and modify the alarm list
  status = pthread_mutex_lock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Lock mutex");
  }

  alarm_t *curr = alarm_list;
  while (curr != NULL) {
    if (curr->alarm_id == alarm->alarm_id) {
      alarm_to_replace = curr;
      replaced_alarm_group = (curr->seconds + 4) / 5;
      break;
    }
    curr = curr->link;
  }
  // if the alarm to replace is found replace its data with the recieved data
  if (alarm_to_replace != NULL) {
    alarm_to_replace->seconds = alarm->seconds;
    alarm_to_replace->time = time(NULL);
    strcpy(alarm_to_replace->message, alarm->message);

    // Print the replacement message
    printf("Alarm(%d) Replaced at %ld: %d %s\n", alarm_to_replace->alarm_id,
           alarm_to_replace->time, alarm_to_replace->seconds,
           alarm_to_replace->message);

    int new_alarm_group = (alarm_to_replace->seconds + 4) / 5;

    // Check display threads only if the new group is different than original
    if (replaced_alarm_group != new_alarm_group) {

      // Check whether to remove display thread
      check_or_remove_display_thread(replaced_alarm_group);

      // Create or get a display alarm thread for the new group
      create_or_check_display_alarm_thread(new_alarm_group, alarm_to_replace);
    }

  } else {
    printf("Alarm with ID %d not found and cannot be replaced.\n",
           alarm->alarm_id);
  }

  // Unlock the alarm mutex
  status = pthread_mutex_unlock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Unlock mutex");
  }
}

void cancel_alarm(int alarm_id) {
  int status;
  alarm_t *prev = NULL;
  alarm_t *curr = alarm_list;

  // Initialize variables to store information about the canceled alarm
  int canceled_alarm_group = -1;

  // Lock the alarm mutex
  status = pthread_mutex_lock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Lock mutex");
  }

  while (curr != NULL) {
    if (curr->alarm_id == alarm_id) {

      // Determine the Alarm_Time_Group_Number associated with the canceled
      // alarm
      canceled_alarm_group = (curr->seconds + 4) / 5;

      // Remove the canceled alarm from the alarm list
      if (prev == NULL) {
        alarm_list = curr->link;
      } else {
        prev->link = curr->link;
      }

      // Print the cancellation message
      printf("Alarm(%d) Canceled at %ld: %d %s\n", alarm_id, time(NULL),
             curr->seconds, curr->message);

      free(curr); // Free the alarm

      // Unlock the alarm mutex
      status = pthread_mutex_unlock(&alarm_mutex);
      if (status != 0) {
        err_abort(status, "Unlock mutex");
      }

      // Check for empty display thread group
      check_or_remove_display_thread(canceled_alarm_group);

      return;
    }
    prev = curr;
    curr = curr->link;
  }

  printf("Alarm with ID %d not found and cannot be canceled.\n", alarm_id);
  // Unlock the alarm mutex
  status = pthread_mutex_unlock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Unlock mutex");
  }
}

int main(int argc, char *argv[]) {
  int status;
  char line[128];
  alarm_t *alarm;
  pthread_t thread;

  while (1) {
    printf("Alarm>");
    if (fgets(line, sizeof(line), stdin) == NULL)
      exit(0);
    if (strlen(line) <= 1) {
      continue;
    }
    // Allocate new alarm
    alarm = (alarm_t *)malloc(sizeof(alarm_t));
    if (alarm == NULL)
      errno_abort("Allocate alarm");

    /*
     * Parse input line into alarm_id (%d), seconds (%d), and a message
     * (%128[^\n]), consisting of up to 128 characters separated by
     * whitespace.
     */
    // COMMAND 1: Start_Alarm
    if (sscanf(line, "Start_Alarm(%d) %d %128[^\n]", &alarm->alarm_id,
               &alarm->seconds, alarm->message) == 3) {
      if (alarm->alarm_id > 0 && alarm->seconds > 0) {
        // Valid alarm_id and seconds, proceed with adding the alarm

        // Insert the new alarm into the list of alarms, sorted by alarm id
        insert_alarm(alarm);
      } else {
        // Invalid alarm_id or seconds
        if (alarm->alarm_id <= 0) {
          fprintf(stderr, "Alarm ID must be greater than 0\n");
        }
        if (alarm->seconds <= 0) {
          fprintf(stderr, "Alarm time must be greater than 0\n");
        }
        // Free the invalid alarm
        free(alarm);
      }
    }
    // COMMAND 2: Replace_Alarm
    else if (sscanf(line, "Replace_Alarm(%d) %d %128[^\n]", &alarm->alarm_id,
                    &alarm->seconds, alarm->message) == 3) {
      if (alarm->alarm_id > 0 && alarm->seconds > 0) {
        // Valid alarm_id and seconds, proceed with replacing the alarm
        replace_alarm(alarm);
        free(alarm); // Free dataholder alarm
      } else {
        // Invalid alarm_id or seconds
        if (alarm->alarm_id <= 0) {
          fprintf(stderr, "Alarm ID must be greater than 0\n");
        }
        if (alarm->seconds <= 0) {
          fprintf(stderr, "Alarm time must be greater than 0\n");
        }
      }
    }
    // COMMAND 3: Cancel_Alarm
    else if (sscanf(line, "Cancel_Alarm(%d)", &alarm->alarm_id) == 1) {
      if (alarm->alarm_id > 0) {
        cancel_alarm(alarm->alarm_id);
      } else {
        fprintf(stderr, "Invalid alarm ID. Please enter a non-negative ID.\n");
      }
    } else {
      fprintf(stderr, "Bad command\n");
      free(alarm); // Free the invalid alarm
    }
    fflush(stdout); // Manually flush the stdout buffer
  }
}
