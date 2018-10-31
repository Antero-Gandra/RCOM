
#pragma once

#define TRUE 1
#define FALSE 0

extern int alarmFired;

void alarmHandler(int signal);

void setAlarm();

void stopAlarm();