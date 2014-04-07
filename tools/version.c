#define DT_MSG_DELIM "---------------------------------------------------"

#include "dt_utils.h"

static char *version_tags = "";

void version_info (void)
{
    dt_info (version_tags, DT_MSG_DELIM "\n");
    dt_info (version_tags, "  dtmedia version 1.0\n");
    dt_info (version_tags, "  based on FFMPEG, built on " __DATE__ " " __TIME__ "\n");
    dt_info (version_tags, "  GCC: " __VERSION__ "\n");
    dt_info (version_tags, DT_MSG_DELIM "\n");
}

void show_usage ()
{
    dt_info (version_tags, DT_MSG_DELIM "\n");
    dt_info (version_tags, " Usage: dtplayer [options] [url|path/]filename \n\n");
    dt_info (version_tags, " Basic options: \n");
    dt_info (version_tags, " -w <width>  set video width \n");
    dt_info (version_tags, " -h <height> set video height \n");
    dt_info (version_tags, " -noaudio    disable audio \n");
    dt_info (version_tags, " -novideo    disable video \n");
    dt_info (version_tags, DT_MSG_DELIM "\n");
}
