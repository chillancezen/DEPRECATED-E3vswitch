#ifndef _INIT_H
#define _INIT_H

#define TASK_PRIORITY_HIGH 0x0
#define TASK_PRIORITY_MEDIUM 0x1
#define TASK_PRIORITY_LOW 0x100
#define TASK_PRIORITY_EXTRA_LOW 0x200
#define TASK_PRIORITY_ULTRA_LOW 0x300

extern struct init_task  task_head;

struct init_task{
	int priority;
	void (*func)(void);
	struct init_task * next;
};

#define E3_init(fun,pri) \
	struct init_task _it##fun={ \
		.priority=(pri), \
		.func=fun, \
		.next=(void*)0, \
	}; \
	__attribute__((constructor)) void _it_init_##fun(void){ \
		(_it##fun).next=task_head.next; \
		task_head.next=&(_it##fun); \
}


#define E3_init_tail(fun) \
	struct init_task _it##fun={ \
		.priority=TASK_PRIORITY_LOW, \
		.func=fun, \
		.next=(void*)0, \
	}; \
	__attribute__((constructor)) void _it_init_##fun(void){ \
		struct init_task * ptr=&task_head; \
		for(;ptr->next;ptr=ptr->next); \
		ptr->next=&(_it##fun); \
}

#define E3_init_head(fun) \
	struct init_task _it##fun={ \
		.priority=TASK_PRIORITY_LOW, \
		.func=fun, \
		.next=(void*)0, \
	}; \
	__attribute__((constructor)) void _it_init_##fun(void){ \
		(_it##fun).next=task_head.next; \
		task_head.next=&(_it##fun); \
}

void init_registered_tasks(void);



	
#endif
