#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.63])
AC_INIT([MERCURY], [0.2], [giuseppe.congiu@seagate.com])
AM_SILENT_RULES([yes])
#AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(confdb)
AC_CONFIG_MACRO_DIR([confdb])
AM_INIT_AUTOMAKE
LT_PREREQ(2.2)
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_LANG_CPLUSPLUS 
AC_PROG_LIBTOOL

# Configure mercury.sh
AC_CONFIG_FILES([script/mercury.sh], [chmod +x script/mercury.sh])
AC_CONFIG_FILES([script/blkstat.py], [chmod +x script/blkstat.py])

# Checks for GPFS header file.
AC_CHECK_HEADERS([gpfs_fcntl.h], 
		 AC_DEFINE(HAVE_GPFS_FCNTL_H, 1, 
			   [Define to 1 if you have <gpfs_fcntl.h>.]),
		 AC_MSG_WARN([GPFS not available.]))

# Checks for Json header file. 
AC_CHECK_HEADERS([jsoncpp/json/json.h], 
		 [],
		 AC_MSG_ERROR([libjson is not installed.]))

# Checks for Python Compliant Regular Expressions header file.
AC_CHECK_HEADERS([pcrecpp.h], 
		 [],
		 AC_MSG_ERROR([libpcrecpp is not installed.]))

# Checks for log4cpp header files.
AC_CHECK_HEADERS([log4cpp/Category.hh \
		  log4cpp/Appender.hh \
		  log4cpp/OstreamAppender.hh \
		  log4cpp/FileAppender.hh \
		  log4cpp/Layout.hh \
		  log4cpp/BasicLayout.hh \
		  log4cpp/Priority.hh \
		  log4cpp/NDC.hh \
		  log4cpp/PropertyConfigurator.hh],
		 [],
		 AC_MSG_ERROR([liblog4cpp is not installed.]))

AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for libraries.
AC_CHECK_LIB([gpfs], [gpfs_fcntl], [LDFLAGS=-lgpfs])
#AC_CHECK_LIB([dl], [-ldl])
#AC_CHECK_LIB([jsoncpp], [-ljsoncpp])
#AC_CHECK_LIB([m], [-lm])
#AC_CHECK_LIB([pcrecpp], [-lpcrecpp])
#AC_CHECK_LIB([rt], [-lrt])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday memset socket strdup strerror])

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT
