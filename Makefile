FINAL_LIBS=-lpthread -lcurl -lsqlite3

# Linux ARM needs -latomic at linking time
ifneq (,$(filter aarch64 armv,$(uname_M)))
        FINAL_LIBS+=-latomic
else
ifneq (,$(findstring armv,$(uname_M)))
        FINAL_LIBS+=-latomic
endif
endif

all: jptodo

jptodo: botlib.c cJSON.c sds.c sqlite_wrap.c json_wrap.c sds.h botlib.h sqlite_wrap.h jptodo.c
	$(CC) -g -ggdb -O2 -Wall -W -std=c11 \
		cJSON.c sds.c json_wrap.c sqlite_wrap.c botlib.c \
		jptodo.c -o jptodo $(FINAL_LIBS)

clean:
	rm -f jptodo
