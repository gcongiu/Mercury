/*
 * Copyright (c) 2013-2015 Seagate Technology.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

extern "C"
{
        // for LD_PRELOAD
        #define dlsym __
        #include <dlfcn.h>
        #undef dlsym
        #include <sys/syscall.h>
        #include <unistd.h>
        #include <sys/types.h>
};

#include <jsoncpp/json/json.h>
#include <pcrecpp.h>
#include <algorithm>
#include <cstring>
#include "Common.hpp"
#include "Register.hpp"
#include "AdvisorThread.hpp"

// Local Functions Prototypes (Use extern "C" to avoid g++ name mangling)
extern "C" void *(*dlsym( void *handle, const char *symbol ))( );
static int     (*_open)         ( const char *, int, ... )                = NULL;
static int     (*_open_2)       ( const char *, int )                     = NULL;
static int     (*_open64)       ( const char *, int, ... )                = NULL;
static int     (*_close)        ( int )                                   = NULL;
static ssize_t (*_read)         ( int, void*, size_t )                    = NULL;
static ssize_t (*_pread)        ( int, void*, size_t, off_t )             = NULL;
static FILE *  (*_fopen)        ( const char *, const char * )            = NULL;
static FILE *  (*_fopen64)      ( const char *, const char * )            = NULL;
static int     (*_fclose)       ( FILE* )                                 = NULL;
static size_t  (*_fread)        ( void*, size_t, size_t, FILE* )          = NULL;
static int     (*___open_2)     ( const char *, int )                     = NULL;
static int     (*__open)        ( const char *, int, ... )                = NULL;
static int     (*__open64)      ( const char *, int, ... )                = NULL;
static int     (*__close)       ( int )                                   = NULL;
static FILE *  (*__fopen)       ( const char *, const char * )            = NULL;
static FILE *  (*__fopen64)     ( const char *, const char * )            = NULL;
static int     (*__fclose)      ( FILE* )                                 = NULL;
static ssize_t (*__read)        ( int, void*, size_t )                    = NULL;
static ssize_t (*__pread)       ( int, void*, size_t, off_t )             = NULL;
static size_t  (*__fread)       ( void*, size_t, size_t, FILE* )          = NULL;
#if 0
static ssize_t (*__pread64)     ( int, void*, size_t, off64_t )           = NULL;
static ssize_t (*_write)        ( int, const void*, size_t )              = NULL;
static off_t   (*_lseek)        ( int, off_t, int )                       = NULL;
static off64_t (*_lseek64)      ( int, off64_t, int )                     = NULL;
static ssize_t (*_pread64)      ( int, void*, size_t, off64_t )           = NULL;
static ssize_t (*_pwrite)       ( int, const void*, size_t, off_t )       = NULL;
static ssize_t (*_pwrite64)     ( int, const void*, size_t, off64_t )     = NULL;
static ssize_t (*_readv)        ( int const struct iovec*, int )          = NULL;
static ssize_t (*_writev)       ( int, const struct iovec*, int )         = NULL;
static int     (*_fxstat)       ( int, int struct stat* )                 = NULL;
static int     (*_fxstat64)     ( int, int, struct stat64* )              = NULL;
static int     (*_lxstat)       ( int, const char*, struct stat* )        = NULL;
static int     (*_lxstat64)     ( int, const char*, struct stat64* )      = NULL;
static void *  (*_map)          ( void*, size_t, int, int, int, off_t )   = NULL;
static void *  (*_map64)        ( void*, size_t, int, int, int, off64_t ) = NULL;
static FILE *  (*_fopen64)      ( const char *, const char * )            = NULL;
static size_t  (*_fwrite)       ( void*, size_t, size_t, FILE* )          = NULL;
static int     (*_fseek)        ( FILE*, long, int )                      = NULL;
static int     (*_fsync)        ( int )                                   = NULL;
static int     (*_fdatasync)    ( int )                                   = NULL;
static int     (*_aio_read)     ( struct aiocb* )                         = NULL;
static int     (*_aio_read64)   ( struct aiocb64* )                       = NULL;
static int     (*_aio_write)    ( struct aiocb* )                         = NULL;
static int     (*_aio_write64)  ( struct aiocb64* )                       = NULL;
static int     (*_lio_listio)   ( int, struct aiocb*, int, struct sigevent* )   = NULL;
static int     (*_lio_listio64) ( int, struct aiocb64*, int, struct sigevent* ) = NULL;
static ssize_t (*_aio_return)   ( struct aiocb* )                         = NULL;
static ssize_t (*_aio_return64) ( struct aiocb64* )                       = NULL;
#endif

extern "C"
{
        int __open_2( const char*, int );
};

static int     myopen   ( const char *, int, ... );
static int     myopen_2 ( const char *, int );
static int     myopen64 ( const char *, int, ... );
static int     myclose  ( int );
static FILE *  myfopen  ( const char *, const char * );
static FILE *  myfopen64( const char *, const char * );
static int     myfclose ( FILE* );
static ssize_t myread   ( int, void*, size_t );
static ssize_t mypread  ( int, void*, size_t, off_t );
static size_t  myfread  ( void*, size_t, size_t, FILE* );
//static ssize_t mypread64(int, void*, size_t, off64_t );

// Global data structures
// RegisterLog data structure
mercury::RegisterLog * register_;

// paths to be advised
static std::list<std::string> * paths_ = NULL; //valid paths

// timers
static struct timeval start_, end_;

// config file
static char *config_file = NULL;

// Stand Alone I/O initialization (should be last after log4cpp and Config.o)
static void __attribute__(( constructor( 65535 ) )) SAIO_init( void )
{
        /* start measuring the time */
        gettimeofday( &start_, NULL );

        std::cerr << "# SAIO_init()" << std::endl;

        /* initialize register_ */
        register_ = new mercury::RegisterLog( );

        /* initialize paths_ map */
        paths_ = new std::list<std::string>( );

        // read the json configuration file for the rules
        config_file = getenv( "MERCURY_CONFIG" );

        if( config_file == NULL )
        {
                std::cerr << "## ERROR: No configuration file available" << std::endl;
                std::cerr << "## type: export MERCURY_CONFIG=\"config.json\"" << std::endl;
                exit( EXIT_FAILURE );
        }

        // redirect calls from within the interposed lib to original functions in libc
        ___open_2 = ( int     (*)( const char*, int ) )            dlsym( RTLD_NEXT, "__open_2" );
        __open    = ( int     (*)( const char*, int, ... ) )       dlsym( RTLD_NEXT, "open" );
        __close   = ( int     (*)( int ) )                         dlsym( RTLD_NEXT, "close" );
        __read    = ( ssize_t (*)( int, void*, size_t ) )          dlsym( RTLD_NEXT, "read" );
        __fopen   = ( FILE *  (*)( const char*, const char* ) )    dlsym( RTLD_NEXT, "fopen" );
        __fopen64 = ( FILE *  (*)( const char*, const char* ) )    dlsym( RTLD_NEXT, "fopen64" );
        __open64  = ( int     (*)( const char*, int, ... ) )       dlsym( RTLD_NEXT, "open64" );
        __fclose  = ( int     (*)( FILE * ) )                      dlsym( RTLD_NEXT, "fclose" );
        __pread   = ( ssize_t (*)( int, void*, size_t, off_t ) )   dlsym( RTLD_NEXT, "pread" );
        __fread   = ( size_t  (*)( void*, size_t, size_t, FILE* ) )dlsym( RTLD_NEXT, "fread" );
        //__pread64= ( ssize_t (*)( int, void*, size_t, off64_t ) )  dlsym( RTLD_NEXT, "pread64" );

        _open_2   = &myopen_2;
        _open     = &myopen;
        _close    = &myclose;
        _read     = &myread;
        _fopen    = &myfopen;
        _fopen64  = &myfopen64;
        _open64   = &myopen64;
        _fclose   = &myfclose;
        _pread    = &mypread;
        _fread    = &myfread;
        //_pread64 = &mypread64;

        // Configure Config data object
        if( !mercury::Config::init( std::string( config_file ) ) )
        {
                exit( EXIT_FAILURE );
        }
        if( !mercury::Config::getAllPaths( paths_ ) )
        {
                exit( EXIT_FAILURE );
        }

        std::cerr << "# monitored paths: ";
        std::list<std::string>::iterator it = paths_->begin( );
        for( ; it != paths_->end( ); it++ )
                std::cerr << *it;
        std::cerr << std::endl;

        /* initialize log4cpp */
        std::string initFileName = "log4cpp.properties";
        log4cpp::PropertyConfigurator::configure( initFileName );
        log4cpp::Category::getInstance( "mercury" );
}

/* this destructor has to be invoked first (before libc otherwise free() won't be found) */
static void __attribute__(( destructor( 65535 ) )) SAIO_fini( void )
{
        /* stop the timer */
        gettimeofday( &end_, NULL );

        std::cerr << "# SAIO_fini()" << std::endl;
        std::cerr << "# [pid:" << getpid( ) << "] elapsed time: " <<
                (double)(end_.tv_sec - start_.tv_sec) + (end_.tv_usec - start_.tv_usec) / 1000000.0
                << " sec" << std::endl;

        /* reset the to original interface */
        _open_2  = ( int     (*)( const char*, int ) )            dlsym( RTLD_NEXT, "__open_2" );
        _open    = ( int     (*)( const char*, int, ... ) )       dlsym( RTLD_NEXT, "open" );
        _close   = ( int     (*)( int ) )                         dlsym( RTLD_NEXT, "close" );
        _read    = ( ssize_t (*)( int, void*, size_t ) )          dlsym( RTLD_NEXT, "read" );
        _fopen   = ( FILE *  (*)( const char*, const char* ) )    dlsym( RTLD_NEXT, "fopen" );
        _fopen64 = ( FILE *  (*)( const char*, const char* ) )    dlsym( RTLD_NEXT, "fopen64" );
        _open64  = ( int     (*)( const char*, int, ... ) )       dlsym( RTLD_NEXT, "open64" );
        _fclose  = ( int     (*)( FILE * ) )                      dlsym( RTLD_NEXT, "fclose" );
        _pread   = ( ssize_t (*)( int, void*, size_t, off_t ) )   dlsym( RTLD_NEXT, "pread" );
        _fread   = ( size_t  (*)( void*, size_t, size_t, FILE* ) )dlsym( RTLD_NEXT, "fread" );
        //__pread64= (ssize_t (*)( int, void*, size_t, off64_t ) )  dlsym( RTLD_NEXT, "pread64");

        /* free memory */
        delete register_;
        delete paths_;

        /* fini log4cpp and config */
        mercury::Config::shutdown( );
        log4cpp::Category::shutdown( );
}

/** ==============================================================
 *  ========================= open ===============================
 */
int __open_2( const char *pathname, int flags )
{
        /* before the constructor is called _open points to open in libc
         * after the constructor is called _open points to myopen
         * after the destructor is called _open points back to open in libc
         */
        if( unlikely( _open_2 == NULL ) )
                _open_2 = (int (*)( const char *, int ))dlsym( RTLD_NEXT, "__open_2" );

        return _open_2( pathname, flags );
}

int myopen_2( const char *pathname, int flags )
{
        mercury::AdvisorThread * at;
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        pthread_attr_t attr;

        int fd = ___open_2( pathname, flags );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                pthread_attr_init( &attr );
                pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
                at = new mercury::AdvisorThread( dup( fd ), pathname, attr );
                register_->registerProcess( fd, at );
                pthread_attr_destroy( &attr );
        }
        return fd;
}

int open( const char *pathname, int flags, ... )
{
        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        /* before the constructor is called _open points to open in libc
         * after the constructor is called _open points to myopen
         * after the destructor is called _open points back to open in libc
         */
        if( unlikely( _open == NULL ) )
                _open = (int (*)( const char *, int, ... ))dlsym( RTLD_NEXT, "open" );

        return _open( pathname, flags, ap );
}

int myopen( const char *pathname, int flags, ... )
{
        mercury::AdvisorThread * at;
        char * filename = strdup( pathname );
        char * basedir = dirname( (char *)filename );
        std::string path( pathname );
        std::string base( basedir );
        pthread_attr_t attr;

        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        int fd = __open( pathname, flags, ap );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " <<  strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                pthread_attr_init( &attr );
                pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
                at = new mercury::AdvisorThread( dup( fd ), std::string( pathname ), attr );
                register_->registerProcess( fd, at );
                pthread_attr_destroy( &attr );
        }
        return fd;
}

/** ==============================================================
 *  ========================= open64 =============================
 */
int open64( const char *pathname, int flags, ... )
{
        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        /* before the constructor is called _open64 points to open in libc
         * after the constructor is called _open64 points to myopen64
         * after the destructor is called _open64 points back to open64 in libc
         */
        if( unlikely( _open64 == NULL ) )
                _open64 = (int (*)( const char *, int, ... ))dlsym( RTLD_NEXT, "open" );

        return _open64( pathname, flags, ap );
}

int myopen64( const char *pathname, int flags, ... )
{
        mercury::AdvisorThread * at;
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        pthread_attr_t attr;

        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        int fd = __open64( pathname, flags, ap );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                pthread_attr_init( &attr );
                pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
                at = new mercury::AdvisorThread( dup( fd ), pathname, attr );
                register_->registerProcess( fd, at );
                pthread_attr_destroy( &attr );
        }
        return fd;
}

/** ==============================================================
 *  ========================= fopen ==============================
 */
FILE* fopen( const char *pathname, const char *mode )
{
        /* before the constructor is called _fopen points to fopen in libc
         * after the constructor is called _fopen points to myfopen
         * after the destructor is called _fopen points back to fopen in libc
         */
        if( unlikely( _fopen == NULL ) )
                _fopen = (FILE* (*)( const char *, const char * ))dlsym( RTLD_NEXT, "fopen" );

        return _fopen( pathname, mode );
}

FILE* myfopen( const char *pathname, const char *mode )
{
        mercury::AdvisorThread * at;
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        pthread_attr_t attr;
        FILE* fp = __fopen( pathname, mode );

        if( fp == NULL )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return NULL;
        }

        int fd = fileno( fp );
        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                pthread_attr_init( &attr );
                pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
                at = new mercury::AdvisorThread( dup( fd ), pathname, attr );
                register_->registerProcess( fd, at );
                pthread_attr_destroy( &attr );
        }
        return fp;
}

/** ==============================================================
 *  ========================= fopen64 ============================
 */
FILE* fopen64( const char *pathname, const char *mode )
{
        /* before the constructor is called _fopen points to fopen in libc
         * after the constructor is called _fopen points to myfopen
         * after the destructor is called _fopen points back to fopen in libc
         */
        if( unlikely( _fopen64 == NULL ) )
                _fopen64 = (FILE* (*)( const char *, const char * ))dlsym( RTLD_NEXT, "fopen64" );

        return _fopen64( pathname, mode );
}

FILE* myfopen64( const char *pathname, const char *mode )
{
        mercury::AdvisorThread * at;
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        pthread_attr_t attr;
        FILE* fp = __fopen64( pathname, mode );

        if( fp == NULL )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return NULL;
        }

        int fd = fileno( fp );
        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                pthread_attr_init( &attr );
                pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
                at = new mercury::AdvisorThread( dup( fd ), pathname, attr );
                register_->registerProcess( fd, at );
                pthread_attr_destroy( &attr );
        }
        return fp;
}

 /** ==============================================================
 *  ========================= close ==============================
 */
int close( int fd )
{
        /* before the constructor is called _close points to close in libc
         * after the constructor is called _close points to myclose
         * after the destructor is called _close points back to close in libc
         */
        if( unlikely( _close == NULL ) )
                _close = (int (*)( int ))dlsym( RTLD_NEXT, "close" );

        return _close(fd);
}

int myclose( int fd )
{
        int ret = __close( fd );
        if( register_->lookup( fd ) )
                register_->unregisterProcess( fd ) ;
        return ret;
}

/** ==============================================================
 *  ========================= fclose =============================
 */
int fclose( FILE *fp )
{
        /* before the constructor is called _fclose points to fclose in libc
         * after the constructor is called _fclose points to myfclose
         * after the destructor is called _fclose points back to fclose in libc
         */
        if( unlikely( _fclose == NULL ) )
                _fclose = (int (*)( FILE * ))dlsym( RTLD_NEXT, "fclose" );

        return _fclose( fp );
}

int myfclose( FILE *fp )
{
        int fd = fileno( fp );
        int ret = __fclose( fp );
        if( register_->lookup( fd ) )
                register_->unregisterProcess( fd );
        return ret;
}

/** ==============================================================
 *  ========================= read ===============================
 */
ssize_t read( int fd, void *buf, size_t count )
{
        /* before the constructor is called _read points to read in libc
         * after the constructor is called _read points to myread
         * after the destructor is called _read points back to read in libc
         */
        if( unlikely( _read == NULL ) )
                _read = (ssize_t (*)( int, void*, size_t ))dlsym( RTLD_NEXT, "read" );

        return _read( fd, buf, count );
}

ssize_t myread( int fd, void *buf, size_t count )
{
        std::string message;
        off_t offset = lseek( fd, 0, SEEK_CUR );
        if( register_->lookup( fd ) )
        {
                sprintf( (char *)message.c_str( ), "Read %lli %lli", (offset_t)offset, (offset_t)count );
                register_->getAdvisorThread( fd )->enqueue( message );
        }
        return __read( fd, buf, count );
}

/** ==============================================================
 *  ========================= pread ==============================
 */
ssize_t pread( int fd, void *buf, size_t count, off_t offset )
{
        /* before the constructor is called _pread points to pread in libc
         * after the constructor is called _pread points to mypread
         * after the destructor is called _pread points back to pread in libc
         */
        if( unlikely( _pread == NULL ) )
                _pread = (ssize_t (*)( int, void*, size_t, off_t ))dlsym( RTLD_NEXT, "pread" );

        return _pread( fd, buf, count, offset );
}

ssize_t mypread( int fd, void* buf, size_t count, off_t offset )
{
        std::string message;
        if( register_->lookup( fd ) )
        {
                sprintf( (char *)message.c_str( ), "Read %lli %lli", (offset_t)offset, (offset_t)count );
                register_->getAdvisorThread( fd )->enqueue( message );
        }
        return __pread( fd, buf, count, offset );
}

/** ==============================================================
 *  ========================= fread ==============================
 */
size_t fread( void *ptr, size_t size, size_t nmemb, FILE *stream )
{
        /* before the constructor is called _fread points to fread in libc
         * after the constructor is called _fread points to myfread
         * after the destructor is called _fread points back to fread in libc
         */
        if( unlikely( _fread == NULL ) )
                _fread = (size_t (*)( void*, size_t, size_t, FILE* ))dlsym( RTLD_NEXT, "fread" );

        return _fread( ptr, size, nmemb, stream );
}

size_t myfread( void *ptr, size_t size, size_t nmemb, FILE *stream )
{
        int fd = fileno( stream );
        off_t offset = lseek( fd, 0, SEEK_CUR );
        std::string message;
        if( register_->lookup( fd ) )
        {
                sprintf( (char *)message.c_str( ), "Read %lli %lli", (offset_t)offset, (offset_t)size );
                register_->getAdvisorThread( fd )->enqueue( message );
        }
        return __fread( ptr, size, nmemb, stream );
}
