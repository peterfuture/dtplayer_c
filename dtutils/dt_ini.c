#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dt_ini.h"

#define tpNULL       0
#define tpSECTION    1
#define tpKEYVALUE   2
#define tpCOMMENT    3

#define CONF_MAX_PATH 512

#define ArePtrValid(Sec,Key, Val) ((Sec!=NULL)&&(Key!=NULL)&&(Val!=NULL))

struct entry {
    char type;
    char *text;
    struct entry *prev;
    struct entry *next;
};

struct ini_context {
    struct entry *list;
    char *path;
};

static char *DEFAULT_INI_PATH = "etc/sys_set.ini";
static struct ini_context * I = NULL;

static void entry_free(struct entry *list)
{
    struct entry *cur = list;
    struct entry *next = NULL;

    while (1) {
        if (!cur) {
            break;
        }
        next = cur->next;
        if (cur->text) {
            free(cur->text);    /* Frees the pointer if not NULL */
        }
        free(cur);
        cur = next;
    }
}

struct entry *new_entry(void)
{
    struct entry *e = (struct entry *) malloc(sizeof(*e));
    if (e == NULL) {
        return NULL;
    }
    memset(e, 0, sizeof(*e));
    return e;
}

static int load_ini(struct ini_context *ctx)
{
    char str[255];
    char *pstr = NULL;
    struct entry *e = NULL;
    struct entry *tail = NULL;
    char *file = ctx->path;
    FILE *fp = NULL;

    entry_free(ctx->list);

    if (!file) {
        return -1;
    }

    if ((fp = fopen(file, "r")) == NULL) {
        return -1;
    }

    printf("Load %s.\n", file);

    while (fgets(str, 255, fp)) {
        pstr = strchr(str, '\n');
        if (pstr != NULL) {
            *pstr = 0;
        }
        e = new_entry();
        if (e == NULL) {
            goto fail;
        }
        // add to list-tail
        if (!ctx->list) {
            ctx->list = e;
            tail = e;
        } else {
            tail->next = e;
            e->next = NULL;
            e->prev = tail;
            tail = e;
        }

        e->text = (char *) malloc(strlen(str) + 1);
        if (e->text == NULL) {
            goto fail;
        }
        strcpy(e->text, str);
        pstr = strchr(str, ';');
        if (pstr != NULL) {
            *pstr = 0;
        }
        /* Cut all comments */
        if ((strstr(str, "[") > 0) && (strstr(str, "]") > 0)) { /* Is Section */
            e->type = tpSECTION;
        } else {
            if (strstr(str, "=") > 0) {
                e->type = tpKEYVALUE;
            } else {
                e->type = tpCOMMENT;
            }
        }
    }
    fclose(fp);
    return 0;
fail:
    entry_free(ctx->list);
    return -1;
}

int dt_ini_open(char *file)
{
    int ret = 0;
    struct ini_context *ctx = I;
    if (ctx) {
        return -1;
    }
    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -1;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->path = (file ? file : DEFAULT_INI_PATH);
    ret = load_ini(ctx);
    if (ret < 0) {
        free(ctx);
        return ret;
    }
    I = ctx;
    return ret;
}

static int get_keyval(const char * section, const char *key, char *out,
                      struct entry *list)
{
    struct entry *e = NULL;
    char Text[255];
    char SecText[130];
    char KeyText[255];
    char ValText[255];
    char *pText;

    if (ArePtrValid(section, key, out) == 0) {
        return -1;
    }

    e = list;
    // find section
    sprintf(SecText, "[%s]", section);
    while (e != NULL) {
        if (e->type == tpSECTION) {
            if (strcasecmp(SecText, e->text) == 0) {
                break;
            }
        }
        e = e->next;
    }
    if (!e) {
        return -1;
    }

    // find key
    e = e->next;
    while (e != NULL) {
        if (e->type == tpSECTION) {
            return -1;
        }
        if (e->type != tpKEYVALUE) {
            e = e->next;
            continue;
        }

        strcpy(Text, e->text);
        pText = strchr(Text, ';');
        if (pText != NULL) {
            *pText = 0;
        }
        pText = strchr(Text, '=');
        if (pText != NULL) {
            *pText = 0;
            strcpy(KeyText, Text);
            *pText = '=';
            if (strcasecmp(KeyText, key) == 0) {
                strcpy(ValText, pText + 1);
                printf("%s,%s\n", KeyText, ValText);
                break;
            }
        }
        e = e->next;
    }

    if (!e) {
        return -1;
    }

    strcpy(out, ValText);
    return 0;
}

int dt_ini_get_entry(char *section, char *key, char *val)
{
    struct ini_context *ctx = I;
    if (!ctx) {
        return -1;
    }
    return get_keyval(section, key, val, ctx->list);
}

int dt_ini_release()
{
    if (!I) {
        return 0;
    }
    entry_free(I->list);
    free(I);
    I = NULL;
    return 0;
}

static void debug_list_entry(struct entry *list)
{
    struct entry *e = list;
    while (e) {
        printf("e->type:%d e->text:%s \n", e->type, e->text);
        e = e->next;
    }
}

//#define INI_TEST 1
#ifdef INI_TEST
int main(int argc, char **argv)
{
    char val[CONF_MAX_PATH];
    printf("INI TEST Enter\n");
    dt_ini_open(argv[1]);
    if (I) {
        debug_list_entry(I->list);
    }
    dt_ini_get_entry("LOG", "log.level", val);
    printf("INI TEST Exit\n");
#if 0
    char valBuf[CONF_MAX_PATH];
    GetPrivateProfileString("SystemSet", "Volume", valBuf, "./systemset.ini");
    printf("valBuf=%s\n", valBuf);
    WritePrivateProfileString("SystemSet", "Volume", "ssg", "./systemset.ini");
    GetPrivateProfileString("SystemSet", "Volume", valBuf, "./systemset.ini");
    printf("valBuf=%s\n", valBuf);
#endif
    return 0;
}
#endif
