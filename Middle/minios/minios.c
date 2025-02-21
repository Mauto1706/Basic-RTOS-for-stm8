#include "minios.h"

#define MINIOS_TASK_SIZE_DEFAULT        128u
#define MINIOS_CORE_REGISTER_SIZE        64u // 16 regisiter
#define MINIOS_FPU_REGISTER_SIZE         72u

#define ARM_CORTEX_M4_NUMBER_REGISTER   16u

#define MINIOS_SYSTICK_CYCLE            1000

#define ROUND_UP_TO_4(x)  (((x) + 3) & ~0x03)

uint32_t volatile timeSystem = 0;

MiniOS_Task * volatile currentTask = NULL;
MiniOS_Task * volatile nextTask = NULL;
MiniOS_Task * volatile activeTasks = NULL;
MiniOS_Task * volatile suspendedTasks = NULL;

void MiniOS_IdleTask(void)
{
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	while(1)
	{
		__WFI();
	}
}


void MiniOS_EnterCritical(void)
{
    __disable_irq();
}

void MiniOS_ExitCritical(void)
{
    __enable_irq();
}

void MiniOS_SaveRegistersR4_R11(uint32_t *regValues)
{
    __asm volatile
	(
        "STMIA %[output]!, {R4-R11}"
        : [output] "=r" (regValues)
        : "0" (regValues)
        : "memory"
    );
}



void MiniOS_Init(void)
{
	MiniOS_EnterCritical();

	activeTasks = (MiniOS_Task *)malloc(sizeof(MiniOS_Task));
	activeTasks->name = "IDLE Task";
	activeTasks->stackBase    = (uint32_t *)calloc(ROUND_UP_TO_4(MINIOS_TASK_SIZE_DEFAULT + MINIOS_CORE_REGISTER_SIZE + MINIOS_FPU_REGISTER_SIZE), sizeof(uint8_t));
	activeTasks->stackPointer = &activeTasks->stackBase[ROUND_UP_TO_4(MINIOS_TASK_SIZE_DEFAULT + MINIOS_CORE_REGISTER_SIZE + MINIOS_FPU_REGISTER_SIZE) / sizeof(uint32_t) - MINIOS_TASK_SIZE_DEFAULT/sizeof(uint32_t)];

    activeTasks->stackPointer[15] = (1U << 24);                 // Set Thumb bit in xPSR
    activeTasks->stackPointer[14] = (uint32_t)MiniOS_IdleTask;  // PC points to Idle task
    activeTasks->stackPointer[13] = (uint32_t)MiniOS_IdleTask;  // LR value for returning to thread mode with PSP

    activeTasks->stackSize = MINIOS_TASK_SIZE_DEFAULT;
    activeTasks->sleepTime = 0;
    activeTasks->frontStack = activeTasks;
    activeTasks->rearStack = activeTasks;

    currentTask = activeTasks;
    nextTask = currentTask;

    SCB->CPACR &= ~(0xF << 20);    // off FDU
    SysTick->LOAD = (SystemCoreClock / MINIOS_SYSTICK_CYCLE) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    NVIC_SetPriority(PendSV_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);

	MiniOS_ExitCritical();
}

void MiniOS_TaskCreate(void (*taskFunction)(void), const char *taskName, uint32_t stackSize)
{
	MiniOS_EnterCritical();

	MiniOS_Task *newTask = (MiniOS_Task *)malloc(sizeof(MiniOS_Task));

	newTask->stackBase = (uint32_t *)calloc(ROUND_UP_TO_4(stackSize + MINIOS_CORE_REGISTER_SIZE + MINIOS_FPU_REGISTER_SIZE), sizeof(uint8_t));
	uint32_t *stack = &newTask->stackBase[ROUND_UP_TO_4(stackSize + MINIOS_CORE_REGISTER_SIZE + MINIOS_FPU_REGISTER_SIZE) / sizeof(uint32_t) - stackSize/sizeof(uint32_t)];

	MiniOS_SaveRegistersR4_R11(&stack[0]);

    stack[13] = (uint32_t)taskFunction;  // LR
    stack[14] = (uint32_t)taskFunction;  // PC (task function)
    stack[15] = (1U << 24);  // xPSR (Thumb bit set)

    newTask->stackPointer = stack;
    newTask->name = taskName;
    newTask->stackSize = stackSize;
    newTask->sleepTime = 0;

    // Add new task to task list
    newTask->rearStack = currentTask->rearStack;
    currentTask->rearStack->frontStack = newTask;
    currentTask->rearStack = newTask;
    newTask->frontStack = currentTask;

	MiniOS_ExitCritical();
}

void MiniOS_Start(void)
{
    // Set PSP to the stack of the first task and switch to PSP
    __set_CONTROL(__get_CONTROL() | 0x2);  // Use PSP
    __ISB();  // Synchronize pipeline
    __set_PSP((uint32_t)activeTasks->stackPointer + ARM_CORTEX_M4_NUMBER_REGISTER * 4);
    asm volatile (
        "BX %0\n"
        :
        : "r" (&MiniOS_IdleTask)
	);
   SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
   __WFI();
}

uint32_t MiniOS_GetTickMs(void)
{
    return timeSystem / 1000;
}

void MiniOS_TaskSleep(uint32_t ms)
{
    MiniOS_EnterCritical();

    // Set current task's sleep time
    currentTask->startTime = MiniOS_GetTickMs();
    currentTask->sleepTime = ms;

    nextTask = currentTask->rearStack;
    currentTask->frontStack->rearStack = nextTask;
    nextTask->frontStack = currentTask->frontStack;

    // Add current task to suspended list
    if (suspendedTasks == NULL)
    {
        suspendedTasks = currentTask;
        suspendedTasks->rearStack = suspendedTasks;
        suspendedTasks->frontStack = suspendedTasks;
    }
    else  // Nếu danh sách bị suspend có task
    {
        currentTask->rearStack = suspendedTasks->rearStack;
        suspendedTasks->rearStack->frontStack = currentTask;
        suspendedTasks->rearStack = currentTask;
        currentTask->frontStack = suspendedTasks;
        suspendedTasks = currentTask;
    }

    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    MiniOS_ExitCritical();
    __WFI();
}

void MiniOS_CheckSuspendedTasks(void)
{
    uint32_t tickMs = MiniOS_GetTickMs();
    MiniOS_Task *task = suspendedTasks;

    do
    {
        if (suspendedTasks == NULL)
        {
            break;
        }

        // Check if the task has completed its sleep time
        if ((tickMs - task->startTime) >= task->sleepTime)
        {
            task->sleepTime = 0;

            // Remove task from suspended list
            if (task == task->rearStack)  // Nếu là task duy nhất trong danh sách
            {
                suspendedTasks = NULL;
            }
            else  // Nếu không phải task duy nhất
            {
                task->frontStack->rearStack = task->rearStack;
                task->rearStack->frontStack = task->frontStack;
                suspendedTasks = task->rearStack;
            }

            task->rearStack = currentTask->rearStack;
            currentTask->rearStack->frontStack = task;
            currentTask->rearStack = task;
            task->frontStack = currentTask;

            break;
        }
        task = task->rearStack;
    } while (task != suspendedTasks);
}


__attribute__((interrupt()))
__attribute__((optimize("Ofast")))
void SysTick_Handler(void)
{
	if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
	{
		timeSystem += MINIOS_SYSTICK_CYCLE;
	}
	//MiniOS_CheckSuspendedTasks();

    if (currentTask == suspendedTasks)
    {
        nextTask = activeTasks;
    }
    else
    {
        nextTask = currentTask->rearStack;
    }
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}


void PendSV_Handler(void)
{
    __asm volatile
	(
        "MRS R0, PSP\n"            // Load PSP to R0
        "STMDB R0!, {R4-R11}\n"    // Save R4-R11 on stack
        "MSR PSP, R0\n"            // Update PSP
    );

    currentTask->stackPointer = (uint32_t *)__get_PSP();  // Save current task stack pointer
    currentTask = nextTask;                               // Switch to the next task
    __set_PSP((uint32_t)currentTask->stackPointer);       // Set PSP to new task's stack pointer

    __asm volatile
	(
		"MRS R0, PSP\n"
		"LDMIA R0!, {R4-R11}\n"    // Restore R4-R11 from stack
		"MSR PSP, R0\n"
		//"LDR LR, [%0, #52]\n"
		"BX LR\n"                  // Return from exception
		//:
		//:"r"(currentTask->stackPointer)                 // Return from exception
    );

}

