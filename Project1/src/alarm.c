
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "alarm.h"
#include "link.h"

int alarmFired = FALSE;

void alarmHandler(int signal)
{
    if (signal != SIGALRM)
        return;

    alarmFired = TRUE;

    stats->timeouts++;

    printf("Connection time out!\n");

    //Set alarm again
    alarm(settings->timeout);
}

void setAlarm()
{

    //Setup
    struct sigaction action;
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGALRM, &action, NULL);

    //Set alarm
    alarmFired = FALSE;
    alarm(settings->timeout);
}

void stopAlarm()
{

    //Setup
    struct sigaction action;
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGALRM, &action, NULL);

    //Block alarm
    alarm(0);
}