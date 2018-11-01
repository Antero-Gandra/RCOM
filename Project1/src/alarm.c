
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

    printf("Connection time out!\n");

    alarm(settings->timeout);
}

void setAlarm()
{

    struct sigaction action;
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGALRM, &action, NULL);

    alarmFired = FALSE;
    alarm(settings->timeout);
}

void stopAlarm()
{

    struct sigaction action;
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGALRM, &action, NULL);

    alarm(0);
}