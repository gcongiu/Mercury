AUTOMAKE_OPTIONS = foreign

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
bin_PROGRAMS = AdviceManager

# additional libs
AdviceManager_LDADD = $(LDFLAGS) -ljsoncpp -lpcrecpp -lm -lrt -ldl -lpthread -llog4cpp

# c++ flags
AdviceManager_CPPFLAGS = -g -O2 -I$(top_srcdir)/include -Wall

# source files
AdviceManager_SOURCES = AdviceManager.cpp \
			AdvisorThread.cpp  \
			Config.cpp         \
			Register.cpp       \
			BlockCache.cpp

# Assisted I/O library
lib_LTLIBRARIES = libAIO.la libSAIO.la

# additional libs
libAIO_la_LIBADD = -ljsoncpp -lm -lrt -ldl -llog4cpp
libAIO_la_LDFLAGS = -version-info 0:0:0

# c++ flags
libAIO_la_CPPFLAGS = -g -O2 -Wall -I$(top_srcdir)/include

# I/O library source
libAIO_la_SOURCES = AIO.cpp \
		    Config.cpp # additional libs

# Stand Alone I/O library
libSAIO_la_LIBADD = -ljsoncpp -lpcrecpp -lm -lrt -ldl -lpthread -llog4cpp
libSAIO_la_LDFLAGS = -version-info 0:0:0

# c++ flags
libSAIO_la_CPPFLAGS = -g -O2 -Wall -I$(top_srcdir)/include

# I/O library source
libSAIO_la_SOURCES = SAIO.cpp \
		     AdvisorThread.cpp \
		     Config.cpp \
		     Register.cpp \
		     BlockCache.cpp
