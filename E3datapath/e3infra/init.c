#include <init.h>
#include <stdio.h>

struct init_task  task_head={
	.next=(void*)0
	};

void init_registered_tasks(void)
{

	struct init_task task_tmp;
	struct init_task * task,*ptr;
	/*sort tasks according to priorities*/
	task_tmp.next=task_head.next;
	task_head.next=NULL;
	
	while(task_tmp.next){
		task=task_tmp.next;
		task_tmp.next=task->next;


		ptr=&task_head;
		while(ptr->next){
			if(ptr->next->priority>=task->priority)
				break;
			ptr=ptr->next;
		}
		task->next=ptr->next;
		ptr->next=task;
	}
	for(ptr=task_head.next;ptr;ptr=ptr->next)
		ptr->func();
}
