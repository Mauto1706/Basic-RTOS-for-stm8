#ifndef MINIOS_H
#define MINIOS_H

#include <stdint.h>
#include <stdlib.h>
#include "stm32f4xx.h"

typedef struct MiniOS_Task
{
    uint32_t   *stackPointer;
    uint32_t   *stackBase;
    const char *name;
    uint32_t   stackSize;
    uint32_t   sleepTime;
    uint32_t   startTime;
    struct MiniOS_Task *frontStack;
    struct MiniOS_Task *rearStack;
} MiniOS_Task;



void MiniOS_Init(void);

void MiniOS_TaskCreate(void (*taskFunction)(void), const char *taskName, uint32_t stackSize);
void MiniOS_Start(void);






#endif /* MINIOS_H */
