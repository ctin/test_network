#include <stdbool.h>

typedef void EventLoopFunction();

bool AddTask(EventLoopFunction func);
bool HasTasks();
EventLoopFunction* PopTask();
