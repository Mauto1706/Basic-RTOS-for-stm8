#include "mainApp.h"
#include "minios.h"




void Task1(void)
{

    while (1)
    {
        // Công việc của Task 1: chạy vòng lặp delay
    	//for (uint32_t i = 0; i < 0xFFFFFF; i++) {}
        //MiniOS_TaskSleep(2000);
    }
}

void Task2(void)
{
    //MiniOS_TaskCreate(Task3, "Task 3", 512);

    while (1)
    {
        // Công việc của Task 2: chạy vòng lặp delay
    	for (uint32_t i = 0; i < 0xFFFFFF; i++) {}
        //MiniOS_TaskSleep(2000);  // Cho task 2 ngủ 2 giây
    }
}

void Task3(void)
{
    while (1)
    {
        // Công việc của Task 3: chạy vòng lặp delay
        for (uint32_t i = 0; i < 0xFFFFFF; i++) {}
        //MiniOS_TaskSleep(1000);  // Cho task 3 ngủ 1 giây
    }
}

void mainApp(void)
{
    // Khởi tạo MiniOS
    MiniOS_Init();

    // Tạo các task với kích thước ngăn xếp lớn hơn để tránh tràn stack
    MiniOS_TaskCreate(Task1, "Task 1", 2048);  // Tăng kích thước ngăn xếp cho Task1
    MiniOS_TaskCreate(Task2, "Task 2", 1024);  // Tăng kích thước ngăn xếp cho Task2
    MiniOS_TaskCreate(Task3, "Task 3", 1024);  // Tăng kích thước ngăn xếp cho Task3

    // Bắt đầu MiniOS
    MiniOS_Start();



    while (1)
    {

        //MiniOS_TaskSleep(1000);
    }
}
