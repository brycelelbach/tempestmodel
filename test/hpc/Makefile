# TempestBase directory
TEMPESTBASEDIR= ../..


# Compile with BLAS libraries
USEBLAS= true

# Load system-specific defaults
include $(TEMPESTBASEDIR)/mk/Make.defs

##
## Build instructions
##
all: atm DataContainerTest TaskTest
atm:
	cd $(TEMPESTBASEDIR)/src/base; make
	cd $(TEMPESTBASEDIR)/src/atm; make

##
## Individual test case build instructions
##
DataContainerTest : $(BUILDDIR)/DataContainerTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/DataContainerTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

TaskTest : $(BUILDDIR)/TaskTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/TaskTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

##
## Clean
##
clean:
	rm -f DataContainerTest TaskTest
	rm -rf $(DEPDIR)
	rm -rf $(BUILDDIR)

##
## Include dependencies
##
include $(FILES:%.cpp=$(DEPDIR)/%.d)

# DO NOT DELETE

