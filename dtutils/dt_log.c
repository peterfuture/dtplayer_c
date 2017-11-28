#include "dt_log.h"
#include "dt_ini.h"
#include "dt_setting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define LOG_INI_FILE "./sys_set.ini"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"


static int dt_log_level = DT_LOG_INFO;
//static FILE * dt_fp = NULL;

static int check_level(int level)
{
    return level >= dtp_setting.log_level;
}

static int tag_enable(char *filter, char *tag)
{
    // tag filter
    if (strlen(filter) > 0 &&
        strlen(tag) > 0 &&
        strstr(filter, tag) == NULL) {
        return 0;
    }
    return 1;
}

static int display_time()
{
    /*
    time_t timer;
    char buffer[25];
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 25, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s[%s]", KYEL, buffer);
    */
    char buffer[30];
    struct timeval tv;
    time_t curtime;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(buffer, 30, "%Y-%m-%d %T.", localtime(&curtime));
    printf("[%s%ld]", buffer, tv.tv_usec);
    return 0;
}

void dt_log(void *tag, int level, const char *fmt, ...)
{
    if (!check_level(level)) {
        return;
    }

    if (!tag_enable(dtp_setting.log_filter, (char *)tag)) {
        return;
    }

    dt_get_log_level(DT_LOG_ERROR);
    printf("[%s] ", (char *) tag);
    va_list vl;
    va_start(vl, fmt);
#if ENABLE_LINUX
    vprintf(fmt, vl);
#endif
#if ENABLE_ANDROID
    __android_log_vprint(ANDROID_LOG_INFO, tag, fmt, vl);
#endif
    va_end(vl);
}

void dt_error(void *tag, const char *fmt, ...)
{
    if (!check_level(DT_LOG_ERROR)) {
        return;
    }
    if (!tag_enable(dtp_setting.log_filter, (char *)tag)) {
        return;
    }
    display_time();
    dt_get_log_level(DT_LOG_ERROR);
    printf("[%s] ", (char *) tag);
    va_list vl;
    va_start(vl, fmt);
#if ENABLE_LINUX
    vprintf(fmt, vl);
#endif
#if ENABLE_ANDROID
    __android_log_vprint(ANDROID_LOG_ERROR, tag, fmt, vl);
#endif
    va_end(vl);

}

void dt_debug(void *tag, const char *fmt, ...)
{
    if (!check_level(DT_LOG_DEBUG)) {
        return;
    }
    if (!tag_enable(dtp_setting.log_filter, (char *)tag)) {
        return;
    }
    display_time();
    dt_get_log_level(DT_LOG_WARNING);
    printf("[%s] ", (char *) tag);
    va_list vl;
    va_start(vl, fmt);
#if ENABLE_LINUX
    vprintf(fmt, vl);
#endif
#if ENABLE_ANDROID
    __android_log_vprint(ANDROID_LOG_DEBUG, tag, fmt, vl);
#endif
    va_end(vl);
}

void dt_warning(void *tag, const char *fmt, ...)
{
    if (!check_level(DT_LOG_WARNING)) {
        return;
    }
    if (!tag_enable(dtp_setting.log_filter, (char *)tag)) {
        return;
    }
    display_time();
    dt_get_log_level(DT_LOG_INFO);
    printf("[%s] ", (char *) tag);
    va_list vl;
    va_start(vl, fmt);
#if ENABLE_LINUX
    vprintf(fmt, vl);
#endif
#if ENABLE_ANDROID
    __android_log_vprint(ANDROID_LOG_WARN, tag, fmt, vl);
#endif
    va_end(vl);
}

void dt_info(void *tag, const char *fmt, ...)
{
    if (!check_level(DT_LOG_INFO)) {
        return;
    }
    if (!tag_enable(dtp_setting.log_filter, (char *)tag)) {
        return;
    }
    display_time();
    dt_get_log_level(DT_LOG_INFO);
    printf("[%s] ", (char *) tag);
    va_list vl;
    va_start(vl, fmt);
#if ENABLE_LINUX
    vprintf(fmt, vl);
#endif
#if ENABLE_ANDROID
    __android_log_vprint(ANDROID_LOG_INFO, tag, fmt, vl);
#endif
    va_end(vl);
}

void dt_set_log_level(int level)
{
    dt_log_level = level;
    printf("after set log level :%d ", dt_log_level);
    dt_get_log_level(dt_log_level);
    printf("\n");
}

/*get log level string desc*/
void dt_get_log_level(int level)
{
    switch (level) {
    case DT_LOG_INVALID:
        printf("[Invalid]");
        break;
    case DT_LOG_ERROR:
        printf("%s[ERROR]", KRED);
        break;
    case DT_LOG_DEBUG:
        printf("%s[DEBUG]", KBLU);
        break;
    case DT_LOG_WARNING:
        printf("%s[WARNING]", KGRN);
        break;
    case DT_LOG_INFO:
        printf("[INFO]");
        break;
    case DT_LOG_MAX:
        printf("%s[Invalid]", KRED);
        break;
    default:
        printf("%s[Invalid]", KRED);
        break;
    }
    printf("%s: ", KNRM);
}

#if 0
int main()
{
    dt_get_log_level();
    dt_info("TEST", "this is info level test \n");
    dt_error("TEST", "this is error level test \n");
    dt_debug("TEST", "this is debug level test \n");
    dt_warning("TEST", "this is warnning level test \n");
    dt_set_log_level(1);
    dt_info("TEST", "this is info level test \n");
    dt_error("TEST", "this is error level test \n");
    dt_debug("TEST", "this is debug level test \n");
    dt_warning("TEST", "this is warning level test \n");
    return;
}
#endif
