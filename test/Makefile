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

test_datapoints: test_datapoints.c ../src/datapoints.c

test_expocurve: test_expocurve.c ../src/expo.c

test_signextension: test_signextension.c