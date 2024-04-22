This C program is an builds upon the original `alarm_thread.c` program, introducing significant improvements to the alarm management system. The key enhancement is the utilization of the main thread for efficiently managing alarms while dynamically creating multiple display threads. Each display thread is responsible for processing and displaying information for its designated set of alarms.

## Usage

1. Ensure that the header file (New_Alarm_Mutex.h) is in the same directory as New_Alarm_Mutex.c, compile the program using:
    `cc New_Alarm_Mutex.c -D_POSIX_PTHREAD_SEMANTICS -lpthread`
2. Run the compiled executable using "a.out".
3. Follow the example commands below to manage alarms.

## Example Commands

- `Start_Alarm(1) 10 Message`: Starts an alarm with ID 1, that will display every 10 seconds with the specified message by the display thread for group 2.
- `Replace_Alarm(1) 15 NewMessage`: Replaces the existing alarm with ID 1 with a new display time and message.
- `Cancel_Alarm(1)`: Cancels the alarm with ID 1.

## Features

1. Main Thread for Alarm Mangement with Dynamic Display Threads:
   - Efficiently manages alarms using the main thread.
   - Dynamically creates multiple display threads for parallel processing.

2. Mutex-Protected Alarm List:
   - Ensures thread safety with mutex protection for access to the alarm list.

3. Efficient Sleeping Mechanism:
   - The display threads strategically sleep allowing the main thread sufficient time to modify the alarm list without contention.

4. Command-Driven Alarm Handling:
   - Supports three commands: `Start_Alarm`, `Replace_Alarm`, and `Cancel_Alarm`.
   - Provides a flexible and interactive interface for managing alarms.

5. Dynamic Alarm Insertion and Replacement:
   - Intelligently handles the insertion of new alarms into the list and the replacement of existing alarms.
   - Organizes alarms within the list based on their ids.

6. Alarm Cancellation Mechanism:
   - Efficiently removes specified alarms from the list.
   - Manages associated display threads and ensures proper synchronization.

7. Dynamic Display Threads Creation:
   - Display alarm threads are dynamically created based on the alarm time group.
   - Enables optimal parallel processing of alarms and adapts to varying alarms wait times.

8. Informative Display Alarm Information:
   - Periodically processes alarms, presenting detailed information such as alarm ID, thread ID, alarm time group, timestamp, and alarm message.

9. Interactive User Interface:
   - The main thread continuously awaits user input, allowing users to initiate, replace, or cancel alarms without having to wait.
   - Gracefully handles invalid commands with corresponding error messages.

## Display Threads

### Dynamic Display Thread Creation

Alarm requests will be sorted into categories based on their time values. Each category is assigned its own display thread where each display thread is responsible for at most five alarms at a time.

Example:
- Alarms with a “Time” value from 1 to 5 belong to group 1.
- Alarms with a “Time” value from 6 to 10 belong to group 2.
- Alarms with a “Time” value from 11 to 15 belong to group 3.
- And so on…

If an alarm belonging to certain group is inserted and there is no display thread responsible for that group then a new display thread is created. After being created by the main thread the display thread is responsible for the following:

- Periodically checking the alarms with time values that it is responsible for.
- For every alarm in the alarm list that the display thread is responsible for it will print every time seconds where time is the time specified before message for that alarm.
- Print: “Alarm (<alarm_id>) Printed by Alarm Thread <thread-id> > for Alarm_Time_Group_Number <Alarm_Time_Group_Number> at <time>: <time message>”

### Dynamic Display Thread Termination

Display threads are terminated under the following circumstances:

- After replacing an alarm, there is no alarms left in its group, the display thread is responsible for it is therefore terminated.

- After removing an alarm, no other alarms exist for that group number.
   
- Print: “Display Alarm Thread <thread-id> for Alarm_Time_Group_Number <Alarm_Time_Group_Number> Terminated at <replace_time>”
