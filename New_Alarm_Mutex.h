/*
 * New_Alarm_Mutex.h
 *
 * Header file containing function and data structure declarations
 * for the New_Alarm_Mutex.c program.
 */
#ifndef ALARM_MUTEX_H
#define ALARM_MUTEX_H

#include "errors.h"
#include <pthread.h>
#include <time.h>

// Define a structure to pass the condition variable and alarm group to each
// display thread
typedef struct {
  int alarm_group;
  pthread_cond_t condition;
} ThreadArguments;

// Define a structure to store information about display alarm threads
typedef struct display_alarm_info {
  pthread_t thread;
  int alarm_time_group;
  int alarms_in_group;
  pthread_cond_t condition;        // Condition variable for signaling
  struct display_alarm_info *next; // Pointer to the next thread in the list
} display_alarm_info_t;

// Define a data structure to store information about each alarm
typedef struct alarm_tag {
  struct alarm_tag *link;
  int alarm_id;
  int seconds;
  time_t time; /* seconds from EPOCH */
  char message[128];
} alarm_t;

// Mutex for managing the alarm list
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Alarm list and current_alarm
alarm_t *alarm_list;

// List of display threads and how many threads are currently reading
display_alarm_info_t *display_alarm_threads;
int num_display_reading = 0;

// Mutex for num_display_reading, to keep track of reading threads
pthread_mutex_t display_alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function declarations

/**
 * @brief Cleanup handler function for display alarm threads.
 *
 * This function is responsible for unlocking the alarm mutex in case the
 * display alarm thread exits while its locked. It decrements the count of
 * display threads, and if it is the last display thread, it unlocks the alarm
 * mutex to allow other threads to proceed. The cleanup handler is registered
 * with the pthread_cleanup_push function in the display_alarm function.
 *
 * @param arg Unused parameter (required by pthread_cleanup_push).
 */
void cleanup_handler(void *arg);

/**
 * @brief The display alarm thread function that displays alarms in its group.
 *
 * This function is responsible for displaying alarms associated with a specific
 * Alarm_Time_Group_Number. It continuously checks for alarms in the specified
 * group and displays them when it's their time.
 *
 * @param arg A pointer to a structure containing the group number and condition
 * variable for the display thread.
 */
void *display_alarm(void *arg);

/**
 * @brief Creates or checks a display alarm thread for a specific
 * Alarm_Time_Group_Number.
 *
 * This function checks if a display alarm thread for a particular
 * Alarm_Time_Group_Number already exists. If it exists, it increments the
 * count of alarms in that group and signals that thread for an update. If it
 * doesn't exist, it creates a new display alarm thread and updates the display
 * alarm threads list.
 *
 * @param alarm_group The group number to create or find a thread
 * for.
 * @param alarm The alarm to be associated with the display thread.
 */
void create_or_check_display_alarm_thread(int alarm_group, alarm_t *alarm);

/**
 * @brief Check and potentially remove a display alarm thread if it's no longer
 * responsible for any alarms.
 *
 * This function checks whether a display alarm thread associated with a
 * specific group number is responsible for any remaining alarms in
 * the group. If there are no alarms left in the group, it terminates the thread
 * and removes it from the display alarm threads data structure.
 *
 * @param alarm_group The group number the display thread is responsible for.
 */
void check_or_remove_display_thread(int alarm_group);

/**
 * @brief Insert an alarm into the list, maintaining order by alarm ID.
 *
 * This function inserts a new alarm into the list while maintaining the order
 * of alarms based on their alarm IDs. If an alarm with the same ID already
 * exists, the new alarm is not inserted, and a message is printed.
 *
 * @param alarm A pointer to the new alarm to insert.
 */
void insert_alarm(alarm_t *alarm);

/**
 * @brief Replace an existing alarm with a new one based on the same alarm ID.
 *
 * This function replaces an existing alarm with a new alarm based on the same
 * alarm ID. If an alarm with the same ID exists in the alarm list, it updates
 * the existing alarm's information. If no such alarm is found, it prints a
 * message indicating that the alarm with the specified ID was not found.
 *
 * @param alarm A pointer to the new alarm that will replace the existing one.
 */
void replace_alarm(alarm_t *alarm);

/**
 * @brief Cancel an alarm with the specified ID.
 *
 * This function cancels an alarm with the specified alarm ID. It searches the
 * alarm list for the alarm with the given ID, removes it from the list, and
 * terminates any associated display alarm thread if it's no longer responsible
 * for any alarms in the same group.
 *
 * @param alarm_id The ID of the alarm to be canceled.
 */
void cancel_alarm(int alarm_id);

/**
 * @brief Main function for the New_Alarm_Mutex program.
 *
 * This function continuously reads user input and performs actions based on the
 * input commands. It implements a command-line interface for interacting with
 * the alarm management system to create, replace or cancel alarms.
 *
 */
int main(int argc, char *argv[]);

#endif // NEW_ALARM_MUTEX_H
