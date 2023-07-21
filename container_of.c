#include <stdio.h>
#include <stdlib.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({			\
			const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
			(type *)( (char *)__mptr - offsetof(type,member) );})

struct person {
	char *name;
	int age;
	float height;
};

struct employee {
	struct person info;
	int emp_id;
	float salary;
};

int main() {
	struct employee emp;
	struct person *p = malloc(sizeof(struct person));

	p->name = "derek";
	p->age = 37;
	p->height = 178.0;

	emp.info = *p;
	emp.emp_id = 10001;
	emp.salary = 1000000.00;

	struct employee *pemp = malloc(sizeof(struct employee));
	pemp = container_of(&emp.emp_id, struct employee, emp_id);
	printf("emp id: %d, salary: %f, name = %s, age = %d, height = %f\n", pemp->emp_id, pemp->salary, pemp->info.name, pemp->info.age, pemp->info.height);

	return 0;
}
