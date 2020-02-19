
name = alarm_mutex

output: $(name).o
	cc $(name).o -D_POSIX_PTHREAD_SEMANTICS -lpthread -o a.out

$(name).o: $(name).c errors.o
	cc -c $(name).c

errors.o:
	cc -c errors.h