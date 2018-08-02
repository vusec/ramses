libname := ramses
abi := 0
EXTRA_CFLAGS ?= -DNDEBUG

srcs := $(wildcard *.c)
srcs += $(wildcard translate/*.c)
srcs += $(wildcard map/*.c)
srcs += $(wildcard map/x86/*.c)

libfname := lib$(libname)
arsufx := .a
sosufx := .so
SYSLDFLAGS = -Wl,-soname,$(soname).$(abi)

arname := $(libfname)$(arsufx)
soname := $(libfname)$(sosufx)

OFLAGS := -O2
CPPFLAGS := -Iinclude -iquote .
CFLAGS := -std=c99 -Wall -Wpedantic -pedantic -fPIC $(OFLAGS) $(CPPFLAGS) $(EXTRA_CFLAGS)
LDFLAGS := -shared -Wall -Wpedantic -pedantic $(SYSLDFLAGS)

deps := $(patsubst %.c,%.d,$(srcs))
objs := $(patsubst %.c,%.o,$(srcs))

all: $(arname) $(soname)

# Static lib
$(arname): $(objs)
	ar -rcs $@ $?

# Dynamic lib
$(soname): $(soname).$(abi)
	ln -sf $< $@

$(soname).$(abi): $(objs)
	$(CC) $(LDFLAGS) -o $@ $^

# Override built-in compile rule
%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<

# Dependency generation
%.d: %.c
	@set -e; \
	DIR=`dirname $<`; \
	case "$$DIR" in \
	"" | ".") $(CC) -MM -MG $(CPPFLAGS) $< | sed 's|\(.*\)\.o[ :]*|\1.o \1.d : |g' > $@;; \
	*) $(CC) -MM -MG $(CPPFLAGS) $< | sed "s|\(.*\)\.o[ :]*|$$DIR/\1.o $$DIR/\1.d : |g" > $@;; \
	esac

.PHONY: all clean cleanall

clean:
	rm -f $(arname) $(soname) $(soname).$(abi) $(implib) $(objs)
	rm -rf tools/__pycache__
	rm -rf pyramses/__pycache__

cleanall: clean
	rm -f $(deps)

include $(deps)
