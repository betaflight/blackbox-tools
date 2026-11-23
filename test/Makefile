DEBUG_FLAGS	 = -g3 -ggdb

CFLAGS = -O0 \
		$(DEBUG_FLAGS) \
		-std=gnu99 \
		-Wall -pedantic -Wextra -Wshadow

LDLIBS = -lm

all: pframe_intervals test_datapoints test_expocurve test_signextension

# pframe_intervals is a design tool with no assertions, so it is not run here
check: test_datapoints test_expocurve test_signextension
	./test_datapoints
	./test_expocurve
	./test_signextension

clean:
	rm -f pframe_intervals test_datapoints test_expocurve test_signextension

pframe_intervals: pframe_intervals.c

# Each test lists the header it includes, so a header change rebuilds it.
# A header must not reach the compiler, so the recipe passes only the C files.
LINK_C = $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@

test_datapoints: test_datapoints.c ../src/datapoints.c ../src/datapoints.h
	$(LINK_C)

test_expocurve: test_expocurve.c ../src/expo.c ../src/expo.h
	$(LINK_C)

test_signextension: test_signextension.c ../src/tools.c ../src/tools.h
	$(LINK_C)
