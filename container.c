#include <stdio.h>
#include <stddef.h>

struct person {
	int name;
	int age;
	float height;
};

struct employee {
	struct person info;
	int emp_id;
	float salary;
};

#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

int main() {
	struct employee emp;
	struct person *p;

	p->name = 33;
	p->age = 37;
	p->height = 178.0;

	p = container_of(&emp.emp_id, struct person, age);
	printf("Name: %s\n", p->age);

	return 0;
}
