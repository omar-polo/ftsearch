.PATH:${.CURDIR}/../lib

PROG =	mkftsidx
SRCS =	mkftsidx.c ports.c wiki.c db.c dictionary.c tokenize.c

WARNINGS = yes

CPPFLAGS += -I/usr/local/include -I${.CURDIR}/../include
LDADD = -lexpat -lsqlite3 -L/usr/local/lib

.if defined(PROFILE)
CPPFLAGS += -DPROFILE
LDADD += -static -lm -lpthread
DEBUG = -pg
.endif

DEBUG += -O0 -g

show-prof:
	gprof mkftsidx ../gmon.out | gprof2dot | dot -Tpng > profile.png
	nsxiv profile.png &

.include <bsd.prog.mk>
