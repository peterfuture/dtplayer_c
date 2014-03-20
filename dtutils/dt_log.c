#include "dt_log.h"
#include "dt_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define LOG_INI_FILE "./sys_set.ini"

static int dt_log_level = DT_LOG_INFO;
//static FILE * dt_fp = NULL;

static int check_level (int level)
{
	char valBuf[512];
	if (GetEnv ("LOG", "log.level", valBuf) > 0)
		dt_log_level = atoi (valBuf);
	return level >= dt_log_level;
}

void dt_log (void *tag, int level, const char *fmt, ...)
{
	if (!check_level (level))
		return;
	printf ("[%s] ", (char *) tag);
	dt_get_log_level (level);
	va_list vl;
	va_start (vl, fmt);
	vprintf (fmt, vl);
	va_end (vl);
}

void dt_error (void *tag, const char *fmt, ...)
{
	if (!check_level (DT_LOG_ERROR))
		return;
	printf ("[%s] ", (char *) tag);
	dt_get_log_level (DT_LOG_ERROR);
	va_list vl;
	va_start (vl, fmt);
	vprintf (fmt, vl);
	va_end (vl);

}

void dt_debug (void *tag, const char *fmt, ...)
{
	if (!check_level (DT_LOG_DEBUG))
		return;
	printf ("[%s] ", (char *) tag);
	dt_get_log_level (DT_LOG_DEBUG);
	va_list vl;
	va_start (vl, fmt);
	vprintf (fmt, vl);
	va_end (vl);
}

void dt_warning (void *tag, const char *fmt, ...)
{
	if (!check_level (DT_LOG_WARNING))
		return;
	printf ("[%s] ", (char *) tag);
	dt_get_log_level (DT_LOG_WARNING);
	va_list vl;
	va_start (vl, fmt);
	vprintf (fmt, vl);
	va_end (vl);
}

void dt_info (void *tag, const char *fmt, ...)
{
	if (!check_level (DT_LOG_INFO))
		return;
	printf ("[%s] ", (char *) tag);
	dt_get_log_level (DT_LOG_INFO);
	va_list vl;
	va_start (vl, fmt);
	vprintf (fmt, vl);
	va_end (vl);
}

void dt_set_log_level (int level)
{
	dt_log_level = level;
	printf ("after set log level :%d ", dt_log_level);
	dt_get_log_level (dt_log_level);
	printf ("\n");
}

/*get log level string desc*/
void dt_get_log_level (int level)
{
	switch (level)
	{
	case DT_LOG_INVALID:
		printf ("[Invalid]");
		break;
	case DT_LOG_ERROR:
		printf ("[ERROR]");
		break;
	case DT_LOG_DEBUG:
		printf ("[DEBUG]");
		break;
	case DT_LOG_WARNING:
		printf ("[WARNING]");
		break;
	case DT_LOG_INFO:
		printf ("[INFO]");
		break;
	case DT_LOG_MAX:
		printf ("[Invalid]");
		break;
	default:
		printf ("[Invalid]");
		break;
	}
}

#if 0
int main ()
{
	dt_get_log_level ();
	dt_info ("TEST", "this is info level test \n");
	dt_error ("TEST", "this is error level test \n");
	dt_debug ("TEST", "this is debug level test \n");
	dt_warning ("TEST", "this is warnning level test \n");
	dt_set_log_level (1);
	dt_info ("TEST", "this is info level test \n");
	dt_error ("TEST", "this is error level test \n");
	dt_debug ("TEST", "this is debug level test \n");
	dt_warning ("TEST", "this is warning level test \n");
	return;
}
#endif
