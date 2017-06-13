#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <cprops/trie.h>
#include <cprops/linked_list.h>
#include <cprops/avl.h>
#include <yaml.h>

#define actus_log_debug(...)      \
  do                              \
  {                               \
    fprintf(stdout, __VA_ARGS__); \
  } while (0)

#define actus_log_error(...)      \
  do                              \
  {                               \
    fprintf(stderr, __VA_ARGS__); \
  } while (0)

// #include "actus_log.h"
#include "actus_cmddb.h"

#define MAX_STR_LEN 384
#define MAX_ARG_LEN 32
#define MAX_VAL_LEN 128

#define CMD_DELIMITERS " \n=[]/.'"
#define VAL_DELIMITERS " \n=[]'/"
#define ARG_START_DELIMOTER '<'
#define ARG_END_DELIMOTER '>'
#define XPATH_OP_LIST "CRMD"

//gcc actus_cmddb.c -o actus_cmddb -L/home/neoul/projects/c_study/cprops/.libs -lyaml -lcprops -I ./ -g3 -Wall

struct yamldb
{
  int level;
  char key[MAX_STR_LEN];
  char value[MAX_STR_LEN];
};

struct cmddb
{
  int accesskey;
  int cmd_substr_len; // current cmd substring length
  char cmd_arg[MAX_ARG_LEN];
  char *cmd_base;
};

static int new_accesskey = 0;

static int alloc_cnt = 0;
void *_malloc(size_t s)
{
  alloc_cnt++;
  return malloc(s);
}

char *_str_dup(char *src)
{
  alloc_cnt++;
  return strdup(src);
}

void _free(void *p)
{
  alloc_cnt--;
  free(p);
}

#define free _free
#define malloc _malloc
#define strdup _str_dup

char *_str_dump(const char *src)
{
  static int dbgidx;
  static char dbgstr[4][512];
  char *str;
  int i = 0, j = 0;
  dbgidx = (dbgidx + 1) % 4;
  str = dbgstr[dbgidx];
  for (; src[i] > 0; i++)
  {
    if (src[i] == '\n')
    {
      str[j] = '\\';
      str[j + 1] = 'n';
      j = j + 2;
    }
    else if (src[i] == '\t')
    {
      str[j] = '\\';
      str[j + 1] = 't';
      j = j + 2;
    }
    else
    {
      str[j] = src[i];
      j = j + 1;
    }
  }
  str[j] = 0;
  return str;
}

char *_str_lstrip(char *src, char *delimiters)
{
  int i, j;
  int is_delimiter = 0;
  for (i = 0; src[i] > 0; i++)
  {
    char c = src[i];
    for (j = 0; delimiters[j] > 0; j++)
    {
      if (c == delimiters[j])
      {
        is_delimiter = 1;
        break;
      }
    }
    if (!is_delimiter)
      return &(src[i]);
    else
      is_delimiter = 0;
  }
  return src;
}

// Remove CMD_DELIMITERS characters from the string tail.
void _str_rstrip(char *src, char *delimiters)
{
  int i, j;
  int is_delimiter = 0;
  for (i = strlen(src) - 1; i >= 0; i--)
  {
    char c = src[i];
    for (j = 0; delimiters[j] > 0; j++)
    {
      if (c == delimiters[j])
      {
        is_delimiter = 1;
        break;
      }
    }
    if (is_delimiter)
    {
      src[i] = 0;
      is_delimiter = 0;
    }
    else
    {
      break;
    }
  }
  return;
}

void _str_copy(const char *src, char *dest, int len)
{
  if (len >= 0)
  {
    strncpy(dest, src, len);
    dest[len] = 0;
  }
  else
  {
    strcpy(dest, src);
  }
}

void _str_clear(char *src)
{
  src[0] = 0;
}

char *_str_alloc(char *str1, int size)
{
  char *str = malloc(size);
  if (str)
  {
    strncpy(str, str1, size);
  }
  return str;
}

char *_str_alloc2(char *str1, char *str2, int str2_len, int size)
{
  char *str = malloc(size);
  if (str)
  {
    snprintf(str, size, "%s%.*s", str1, str2_len, str2);
  }
  return str;
}

char *_str_alloc_with_pos(char *str1, int str1_len, int size)
{
  char *str;
  if (str1_len > size)
    return NULL;
  str = malloc(size);
  if (str)
  {
    memcpy(str, str1, str1_len);
    str[str1_len] = 0;
  }
  return str;
}

char *_str_pbrk_backward(char *src, int src_len, char *charset)
{
  int i, j;
  if (!src || !charset)
    return NULL;
  for (i = src_len - 1; i >= 0; i--)
  {
    for (j = 0; charset[j] > 0; j++)
      if (src[i] == charset[j])
        return &src[i];
  }
  return NULL;
}

void _cmddb_free(void *item)
{
  struct cmddb *cdb = item;
  if (cdb->cmd_base)
    free(cdb->cmd_base);
  free(cdb);
}

void _cmddb_dump_each(char *prefix, struct cmddb *v)
{
  if (v)
    actus_log_debug(" - [%s] => [%05x] l='%d' a='%s' c='%s'\n", _str_dump(prefix),
                    v->accesskey, v->cmd_substr_len, v->cmd_arg[0] ? v->cmd_arg : "",
                    v->cmd_base ? _str_dump(v->cmd_base) : "");
  else
    actus_log_debug(" - [%s] (branch) =>\n", _str_dump(prefix));
}

static int node_count;
static int depth_total;
static int max_level;

void _cmddb_dump_node(cp_trie_node *node, int level, char *prefix)
{
  int i;
  mtab_node *map_node;

  node_count++;
  depth_total += level;
  if (level > max_level)
    max_level = level;

  for (i = 0; i < node->others->size; i++)
  {
    map_node = node->others->table[i];
    while (map_node)
    {
      _cmddb_dump_node(map_node->value, level + 1, map_node->attr);
      map_node = map_node->next;
    }
  }

  for (i = 0; i < level; i++)
    actus_log_debug("\t");

  struct cmddb *v = node->leaf;
  _cmddb_dump_each(prefix, v);
}

void _cmddb_dump_trie(cp_trie *grp)
{
  node_count = 0;
  depth_total = 0;
  max_level = 0;

  _cmddb_dump_node(grp->root, 0, "");

  actus_log_debug("\n %d nodes, %d deepest, avg. depth is %.2f\n\n",
                  node_count, max_level, (float)depth_total / node_count);
}

char *_cmddb_key(int accesskey, char *cmd, char *buf)
{
  if (accesskey > 0)
  {
    sprintf(buf, "%05x %s", accesskey, (cmd) ? cmd : "");
    return buf;
  }
  return (cmd) ? cmd : "";
}

char *_cmddb_key_oper(int accesskey, char cmd_op, char *buf)
{
  if (accesskey > 0)
  {
    sprintf(buf, "%05x %c", accesskey, cmd_op);
    return buf;
  }
  return "";
}

char *_cmddb_key2(int accesskey, char *buf)
{
  if (!buf)
    return buf;
  if (accesskey > 0)
  {
    char code[12];
    sprintf(code, "%05x ", accesskey);
    memcpy((buf - 6), code, 6);
    return buf - 6;
  }
  return buf;
}

int _arg_get_head(const char *src)
{
  int i;
  for (i = 0; src[i] > 0; i++)
  {
    char c = src[i];
    if (c == ARG_START_DELIMOTER)
      break;
    else
      continue;
  }
  return i;
}

int _arg_get_tail(const char *src)
{
  int i;
  for (i = 0; src[i] > 0; i++)
  {
    char c = src[i];
    if (c == ARG_END_DELIMOTER)
      return i + 1;
    else
      continue;
  }
  return i;
}

char *_cmd_oper_extract(char *src, char *op)
{
  int i;
  char c = src[0];
  char *operations = XPATH_OP_LIST;
  for (i = 0; operations[i] != 0; i++)
  {
    if (c == operations[i])
    {
      *op = operations[i];
      return src + 1;
    }
  }
  return src;
}

int _cmd_parser(const char *src, char *cmd_substr, char *cmd_arg)
{
  int i, j;
  char *delimiters = CMD_DELIMITERS;
  for (i = 0; src[i] > 0; i++)
  {
    char c = src[i];
    if (c == ARG_START_DELIMOTER)
    {
      _str_copy(src, cmd_substr, i);
      j = _arg_get_tail(&(src[i]));
      _str_copy(&(src[i]), cmd_arg, j);
      return j + i;
    }
    for (j = 0; delimiters[j] > 0; j++)
    {
      if (c == delimiters[j])
      {
        _str_copy(src, cmd_substr, i);
        cmd_arg[0] = 0;
        return i + 1;
      }
    }
  }
  cmd_substr[0] = 0;
  cmd_arg[0] = 0;
  if (i > 0)
  {
    _str_copy(src, cmd_substr, i);
  }
  return i;
}

char *_val_parser(char *src, char *arg, int *val_len)
{
  int i, j;
  char *val = NULL;
  char *delimiters = VAL_DELIMITERS;

  if (strstr(arg, "LINE"))
  {
    int size = strlen(src);
    val = malloc(size + 1);
    if (val)
    {
      _str_copy(src, val, size);
    }
    *val_len = size;
    return val;
  }
check_val:
  for (i = 0; src[i] != 0; i++)
  {
    char c = src[i];
    for (j = 0; delimiters[j] > 0; j++)
    {
      if (c == delimiters[j])
      {
        if (i == 0)
        {
          src = _str_lstrip(src, CMD_DELIMITERS);
          goto check_val;
        }
        val = malloc(i + 1);
        if (val)
        {
          _str_copy(src, val, i);
        }
        *val_len = i + 1;
        return val;
      }
    }
  }
  if (i > 0)
  {
    val = malloc(i + 1);
    if (val)
    {
      _str_copy(src, val, i);
    }
    *val_len = i + 1;
  }
  return val;
}

int _val_update(cp_trie *valtrie, char *src, char *dest)
{
  int failed = 0;
  int i = 0, j = 0;
  char cmd_arg[MAX_ARG_LEN];
  while (src[i])
  {
    char c = src[i];
    if (c == '<')
    {
      int arglen = _arg_get_tail(&(src[i]));
      _str_copy(&(src[i]), cmd_arg, arglen);
      char *value = cp_trie_exact_match(valtrie, cmd_arg);
      if (value)
      {
        char *d = &(dest[j]);
        strcpy(d, value);
        i = i + arglen;
        j = j + strlen(value);
      }
      else
      {
        actus_log_error("\n\n '%s' CHECK !!!\n - no value for %s\n\n", src, cmd_arg);
        i = i + 1;
        failed = 1;
        break;
      }
    }
    else
    {
      dest[j] = src[i];
      i = i + 1;
      j = j + 1;
    }
  }
  if (failed)
  {
    dest[0] = 0;
    return -1;
  }
  dest[j] = 0;
  return 0;
}

struct cmddb *_yamldb_of_each(
    struct yamldb *ydb, cp_trie *cmddb_trie, struct cmddb *prev)
{
  if (!cmddb_trie)
    return NULL;
  if (!ydb)
    return NULL;
  int len = 0;
  int max_len = strlen(ydb->key);
  char cmd_substr[128];
  char cmd_arg[MAX_ARG_LEN];
  char strbuf[MAX_STR_LEN];
  char *key = ydb->key;
  char *cmddb_key = NULL;
  int accesskey = 0;
  int multi_selection = 0;
  if (prev)
    accesskey = prev->accesskey;
  // actus_log_debug("%s ydb->key %s\n", __func__, key);

  do
  {
    len = _cmd_parser(key, cmd_substr, cmd_arg);
    if (len <= 0)
      break;
    if (!cmd_substr[0] && !cmd_arg[0])
    {
      key = &(key[len]);
      continue;
    }

    // actus_log_debug("%s cmd_substr='%s' cmd_arg='%s'\n", __func__,
    //     _str_dump(cmd_substr), cmd_arg);

    if (cmd_substr[0] == '+')
    {
      multi_selection = accesskey;
      strcpy(cmd_substr, &(cmd_substr[1]));
    }

    cmddb_key = _cmddb_key(accesskey, cmd_substr, strbuf);
    struct cmddb *cdb = cp_trie_exact_match(cmddb_trie, cmddb_key);
    if (cdb)
    {
      accesskey = cdb->accesskey;
    }
    else
    {
      cdb = malloc(sizeof(struct cmddb));
      if (!cdb)
      {
        fputs("Failed to initialize cdb!\n", stderr);
        return NULL;
      }
      memset(cdb, 0, sizeof(struct cmddb));
      sprintf(cdb->cmd_arg, "%s", cmd_arg);
      new_accesskey = new_accesskey + 1;
      cdb->accesskey = new_accesskey;
      accesskey = new_accesskey;

      cdb->cmd_substr_len = strlen(cmd_substr);
      cp_trie_add(cmddb_trie, cmddb_key, cdb);
    }
    prev = cdb;
    key = &(key[len]);
  } while (len < max_len);

  if (multi_selection)
  {
    if (prev)
    {
      prev->accesskey = multi_selection;
    }
  }

  if (prev && ydb->value[0])
  {
    if (prev->cmd_base)
    {
      free(prev->cmd_base);
    }
    prev->cmd_base = strdup(ydb->value);
    // actus_log_debug("%s cmd_base %s\n", __func__, prev->cmd_base);
  }
  return prev;
}

int _yamldb_of_each_print(void *item, void *dummy)
{
  struct yamldb *a = item;
  if (a->value[0])
    actus_log_debug("%d/%s(%s)", a->level, _str_dump((char *)a->key),
                    _str_dump((char *)a->value));
  else
    actus_log_debug("%d/%s ", a->level, _str_dump((char *)a->key));
  return 0;
}

void _yamldb_print(cp_list *list)
{
  cp_list_callback(list, _yamldb_of_each_print, NULL);
  actus_log_debug("\n");
}

int _cmddb_build(cp_trie *cmddb_trie, cp_list *cmdlist)
{
  struct cmddb *prev = NULL;
  cp_list_iterator itr;
  struct yamldb *ydb;
  if (!cmddb_trie || !cmdlist)
    return 0;
  cp_list_iterator_init(&itr, cmdlist, COLLECTION_LOCK_NONE);
  while ((ydb = cp_list_iterator_next(&itr)))
    prev = _yamldb_of_each(ydb, cmddb_trie, prev);
  return 0;
}

static cp_trie *g_cdb = NULL;

cp_trie *cmddb_load(const char *filename)
{
  FILE *fh = fopen(filename, "r");
  yaml_parser_t parser;
  yaml_event_t event; /* New variable */

  /* Initialize parser */
  if (!yaml_parser_initialize(&parser))
  {
    fputs("Failed to initialize parser!\n", stderr);
    return NULL;
  }
  if (fh == NULL)
  {
    fputs("Failed to open file!\n", stderr);
    yaml_parser_delete(&parser);
    return NULL;
  }

  /* Set input file */
  yaml_parser_set_input_file(&parser, fh);

  cp_list *list = cp_list_create_nosync();
  if (list == NULL)
  {
    fputs("can\'t create cmd list\n", stderr);
    yaml_parser_delete(&parser);
    fclose(fh);
    return NULL;
  }

  cp_trie *cmddb_trie = cp_trie_create_trie(
      COLLECTION_MODE_NOSYNC | COLLECTION_MODE_DEEP, NULL, _cmddb_free);
  if (cmddb_trie == NULL)
  {
    fputs("can\'t create cmd trie\n", stderr);
    cp_list_destroy(list);
    yaml_parser_delete(&parser);
    fclose(fh);
    return NULL;
  }

  /* START new code */
  int level = 0;
  int iskey = 0;
  char *yml_val = NULL;

  struct yamldb *cur_ydb;

  do
  {
    if (!yaml_parser_parse(&parser, &event))
    {
      actus_log_error("Parser error %d\n", parser.error);
      exit(EXIT_FAILURE);
    }

    switch (event.type)
    {
    case YAML_NO_EVENT:
      break;
    /* Stream start/end */
    case YAML_STREAM_START_EVENT:
      break;
    case YAML_STREAM_END_EVENT:
      break;
    /* Block delimeters */
    case YAML_DOCUMENT_START_EVENT:
    case YAML_DOCUMENT_END_EVENT:
    case YAML_SEQUENCE_START_EVENT:
    case YAML_SEQUENCE_END_EVENT:
      break;
    case YAML_MAPPING_START_EVENT:
    {
      level += 1;
      iskey = 1;
      cur_ydb = malloc(sizeof(struct yamldb));
      cur_ydb->level = level;
      _str_clear(cur_ydb->key);
      _str_clear(cur_ydb->value);
      sprintf(cur_ydb->key, "level %d", level);
      cp_list_append(list, cur_ydb);
    }
    break;
    case YAML_MAPPING_END_EVENT:
    {
      cur_ydb = cp_list_remove_tail(list);
      free(cur_ydb);
      level -= 1;
    }
    break;
    /* Data */
    case YAML_ALIAS_EVENT:
      //   indent(level);
      //   rintf("Got alias (anchor %s)\n", event.data.alias.anchor);
      break;
    case YAML_SCALAR_EVENT:
    {
      yml_val = (char *)event.data.scalar.value;
      if (iskey)
      {
        if (strcmp(yml_val, "=") == 0)
        {
          yml_val = "";
        }
        cur_ydb = cp_list_get_tail(list);
        if (cur_ydb)
        {
          _str_copy(yml_val, cur_ydb->key, strlen(yml_val));
          _str_clear(cur_ydb->value);
        }
      }
      else
      {
        cur_ydb = cp_list_get_tail(list);
        if (cur_ydb)
        {
          _str_copy(yml_val, cur_ydb->value, strlen(yml_val));
          // _yamldb_print(list);
          _cmddb_build(cmddb_trie, list);
        }
      }
      iskey = (iskey + 1) % 2;
    }
    break;
    }
    if (event.type != YAML_STREAM_END_EVENT)
      yaml_event_delete(&event);
  } while (event.type != YAML_STREAM_END_EVENT);
  _cmddb_dump_trie(cmddb_trie);
  yaml_event_delete(&event);
  /* END new code */
  // _yamldb_print(list);
  cp_list_destroy(list);
  /* Cleanup */
  yaml_parser_delete(&parser);
  fclose(fh);
  g_cdb = cmddb_trie;
  return cmddb_trie;
}

cp_trie *cmddb_get(void)
{
  return g_cdb;
}

void cmddb_unload(cp_trie *cmddb_trie)
{
  cp_trie_destroy(cmddb_trie);
}

struct _cmddb_parms
{
  cp_trie *cmddb_trie;
  cp_trie *valtrie;
  cp_list *outlist;
};

static char *_cmddb_x2c_alloc(char *xpath)
{
  // if(data) {
  //   void *vdata = malloc(strlen(xpath) + strlen(data) + 1 + 16);
  //   sprintf((char *)vdata, "%s %s=%s", "xpath", xpath, data);
  //   return vdata;
  // }
  // else {
  void *vdata = malloc(strlen(xpath) + 1 + 16);
  sprintf((char *)vdata, "%s %s", "xpath", xpath);
  return vdata;
  // }
}

static void _cmddb_x2c_free(void *data)
{
  // printf("%s d=%p\n", __func__, data);
  free(data);
}

static int _cmddb_x2c_cmp(char *str1, char *str2)
{
  // skip "xpath O" prefix and netconf op of xpath.
  // return strcmp(&(str1[7]), &(str2[7]));
  return strcmp(str1, str2);
}

static int _cmddb_x2c_print(void *n, void *dummy)
{
  cp_avlnode *node = n;
  actus_log_debug(" -- xpath='%s'\n", (char *)_str_dump(node->value));
  return 0;
}

static int _cmddb_x2c_get_cli(void *n, void *dummy)
{
  int matched;
  int accesskey = 0;
  cp_avlnode *node = n;
  struct _cmddb_parms *params = dummy;
  char strbuf[MAX_STR_LEN];
  char *incmd = strbuf;
  char *outcmd = NULL;
  char cmd_op = 0;
  int search_done = 0;

  strncpy(incmd, node->value, sizeof(strbuf));
  do
  {
    char operbuf[16];
    char *cmddb_key = NULL;
    struct cmddb *cdb = NULL;
    if (incmd[0] == 0)
    {
      // need to consider...
      search_done = 1;
      break;
    }
    incmd = _str_lstrip(incmd, CMD_DELIMITERS);
    incmd = _cmd_oper_extract(incmd, &cmd_op);
    cmddb_key = _cmddb_key_oper(accesskey, cmd_op, operbuf);
    matched = cp_trie_prefix_match(params->cmddb_trie, cmddb_key, (void *)&cdb);

    if (cdb)
      goto check_cdb;

    incmd = _str_lstrip(incmd, CMD_DELIMITERS);
    cmddb_key = _cmddb_key2(accesskey, incmd);
    matched = cp_trie_prefix_match(params->cmddb_trie, cmddb_key, (void *)&cdb);

  check_cdb:
    if (cdb)
    {
      _cmddb_dump_each(cmddb_key, cdb);
      if (cdb->cmd_arg[0])
      {
        char *cmd_arg_val;
        int cmd_arg_val_len = 0;
        incmd = &(incmd[cdb->cmd_substr_len]);
        cmd_arg_val = _val_parser(incmd, cdb->cmd_arg, &cmd_arg_val_len);
        // actus_log_debug("@@ cmd_arg %s=%s\n",cdb->cmd_arg, cmd_arg_val);
        incmd = &(incmd[cmd_arg_val_len]);
        cp_trie_add(params->valtrie, cdb->cmd_arg, cmd_arg_val);
      }
      else
      {
        incmd = &(incmd[cdb->cmd_substr_len]);
      }
      accesskey = cdb->accesskey;
      if (cdb->cmd_base)
      {
        if (!outcmd)
        {
          outcmd = malloc(MAX_STR_LEN);
          if (!outcmd)
          {
            fputs("outlist alloc failed.\n", stderr);
            return -1;
          }
          outcmd[0] = 0;
          sprintf(outcmd, "%s", cdb->cmd_base);
        }
        else
        {
          sprintf(outcmd + strlen(outcmd), " %s", cdb->cmd_base);
        }
        // actus_log_debug("@@ cmd_base %s\n",cdb->cmd_base);
        // actus_log_debug("@@ outcmd %s\n",outcmd);
      }
    }
    else
    {
      break;
    }
  } while (matched);

  if (outcmd)
  {
    _str_clear(strbuf);
    _val_update(params->valtrie, outcmd, strbuf);
    char *lastcmd = cp_list_get_tail(params->outlist);
    if (lastcmd)
    {
      int lastlen = strlen(lastcmd);
      int curlen = strlen(strbuf);
      int cmp_res = strncmp(lastcmd, strbuf, (curlen < lastlen) ? curlen : lastlen);
      // actus_log_debug("strbuf  '%s'\n", strbuf);
      // actus_log_debug("lastcmd '%s' lastlen %d curlen %d cmp_res %d\n", lastcmd, lastlen, curlen, cmp_res);
      if (cmp_res == 0)
      {
        if (curlen > lastlen)
        {
          lastcmd = cp_list_remove_tail(params->outlist);
          free(lastcmd);
          strncpy(outcmd, strbuf, MAX_STR_LEN);
          cp_list_append(params->outlist, outcmd);
          return 0;
        }
        else
        {
          free(outcmd);
          return 0;
        }
      }
    }
    strncpy(outcmd, strbuf, MAX_STR_LEN);
    cp_list_append(params->outlist, outcmd);
  }
  else
  {
    // fputs(">>> Failed convertion from XPATH to CLI. <<<\n", stderr);
    // fputs(node->value, stderr);
    // fputs("\n", stderr);
  }
  return 0;
}

static int _cmddb_x2c_update_cli(void *vdata, void *dummy)
{
  struct _cmddb_parms *params = dummy;
  char *incmd = vdata;
  char outcmd[MAX_STR_LEN];
  _val_update(params->valtrie, incmd, outcmd);
  strncpy(incmd, outcmd, MAX_STR_LEN);
  return 0;
}

int _print_item(void *item, void *dummy)
{
  if (dummy)
  {
    actus_log_debug(" -- %s=%s\n", (char *)dummy, (char *)item);
  }
  else
  {
    actus_log_debug(" -- %s\n", (char *)item);
  }
  return 0;
}

void _free_data(void *ptr)
{
  char *txt = (char *)ptr;
  // actus_log_debug("### free list node -> %s\n", txt);
  free(txt);
}

int cmddb_x2c_convert_push(cp_avltree **xpath_cmd_list, char *xpath)
{
  if (!*xpath_cmd_list)
  {
    *xpath_cmd_list = cp_avltree_create((cp_compare_fn)_cmddb_x2c_cmp);
    if (!*xpath_cmd_list)
    {
      fputs("*xpath_cmd_list creation failed.\n", stderr);
      return -1;
    }
  }
  char *xpath_cmd = _cmddb_x2c_alloc(xpath);
  cp_avltree_insert(*xpath_cmd_list, xpath_cmd, xpath_cmd);
  return 0;
}

int cmddb_x2c_convert_flush(cp_trie *cmddb_trie, cp_avltree **xpath_cmd_list, cmddb_cmd_send func)
{
  struct _cmddb_parms cmdparams;
  if (!cmddb_trie)
  {
    fputs("cmddb_trie is NULL.\n", stderr);
    return -1;
  }
  if (!*xpath_cmd_list)
  {
    fputs("xpath_cmd_list is NOT created.\n", stderr);
    return -1;
  }
  cp_avltree_callback(*xpath_cmd_list, _cmddb_x2c_print, NULL);

  cmdparams.cmddb_trie = cmddb_trie;
  // cmdparams.valtrie = cp_trie_create_trie(
  //  COLLECTION_MODE_NOSYNC | COLLECTION_MODE_COPY |
  //  COLLECTION_MODE_DEEP, (cp_copy_fn) strdup, free);
  cmdparams.valtrie = cp_trie_create_trie(
      COLLECTION_MODE_NOSYNC | COLLECTION_MODE_DEEP, NULL, _free_data);
  if (cmdparams.valtrie == NULL)
  {
    fputs("valtrie is NOT created.\n", stderr);
    return -1;
  }

  cmdparams.outlist = cp_list_create_nosync();
  if (cmdparams.outlist == NULL)
  {
    fputs("can\'t create cmd list\n", stderr);
    return -1;
  }

  // get commands
  cp_avltree_callback(*xpath_cmd_list, _cmddb_x2c_get_cli, &cmdparams);

  // update commands with argument values
  cp_list_callback(cmdparams.outlist, _cmddb_x2c_update_cli, &cmdparams);
  cp_list_callback(cmdparams.outlist, _print_item, "cli");
  cp_list_callback(cmdparams.outlist, func, &cmdparams);
  // debug
  // cp_list_callback(cmdparams.outlist, _print_item, NULL);
  // cp_trie_dump(cmdparams.valtrie);

  // destroy all
  cp_list_destroy_custom(cmdparams.outlist, _free_data);
  cp_trie_destroy(cmdparams.valtrie);
  cp_avltree_destroy_custom(*xpath_cmd_list, NULL, _cmddb_x2c_free);
  *xpath_cmd_list = NULL;
  return 0;
}

static int _cmddb_c2x_get_xpath(char *incmd, struct _cmddb_parms *params)
{
  ;
  cp_trie *cmddb_trie = params->cmddb_trie;
  cp_trie *valtrie = params->valtrie;
  cp_list *outlist = params->outlist;
  int accesskey = 0;
  char *substr = NULL;
  char strbuf[MAX_STR_LEN] = {0};
  struct cmddb *cdb = NULL;
  char splitter[] = "\n\t ";
  int search_done = 0;

  snprintf(strbuf, sizeof(strbuf), "  %s %s", "cli", incmd);
  incmd = strbuf;

  cp_list *templist = cp_list_create_nosync();
  if (templist == NULL)
  {
    fputs("can\'t create cmd templist\n", stderr);
    return -1;
  }

  do
  {
    char *cmddb_key = NULL;
    incmd = _str_lstrip(incmd, CMD_DELIMITERS);
    if (incmd[0] == 0)
    {
      search_done = 1;
      break;
    }
    cmddb_key = _cmddb_key2(accesskey, incmd);
    cp_trie_prefix_match(cmddb_trie, cmddb_key, (void *)&cdb);
    if (!cdb)
    {
      break;
    }
    _cmddb_dump_each(cmddb_key, cdb);
    if (cdb->cmd_arg[0])
    {
      char *cmd_arg_val;
      int cmd_arg_val_len = 0;
      incmd = &(incmd[cdb->cmd_substr_len]);
      cmd_arg_val = _val_parser(incmd, cdb->cmd_arg, &cmd_arg_val_len);
      actus_log_debug(" - @@ cmd_arg %s=%s\n", cdb->cmd_arg, cmd_arg_val);
      incmd = &(incmd[cmd_arg_val_len]);
      cp_trie_add(valtrie, cdb->cmd_arg, cmd_arg_val);
    }
    else
    {
      incmd = &(incmd[cdb->cmd_substr_len]);
    }
    accesskey = cdb->accesskey;
    if (cdb->cmd_base)
    {
      // add cmd_base to templist.
      cp_list_append(templist, cdb->cmd_base);
    }
  } while (cdb);

  // cp_list_callback(templist, _print_item, NULL);
  if (cp_list_is_empty(templist))
  {
    return -1;
  }
  if (!search_done)
  {
    cp_list_destroy(templist);
    return -1;
  }

  _str_clear(strbuf);
  cp_list_iterator iter;
  cp_list_iterator_init(&iter, templist, COLLECTION_LOCK_NONE);
  while ((substr = cp_list_iterator_next(&iter)))
  {
    int pos;
    int substr_len;
    char cmd_op;
    char *valstr;

  collect_outlist:
    cmd_op = 0;
    substr_len = strlen(substr);
    _cmd_oper_extract(substr, &cmd_op);
    if (cmd_op)
    {
      _str_clear(strbuf);
    }
    pos = strcspn(substr, splitter);
    valstr = _str_pbrk_backward(substr, pos, "[=]");
    if (valstr && *valstr == '=')
    {
      cp_list_append(outlist, _str_alloc2(strbuf, substr, pos, MAX_STR_LEN));
    }
    else
    {
      snprintf(strbuf, MAX_STR_LEN, "%s%.*s", strbuf, pos, substr);
    }
    if (pos + 1 < substr_len)
    {
      substr = &(substr[pos]);
      substr = _str_lstrip(substr, splitter);
      goto collect_outlist;
    }
  }
  // cp_list_callback(outlist, _print_item, NULL);
  if (cp_list_is_empty(outlist) && strbuf[0])
  {
    cp_list_append(outlist, _str_alloc(strbuf, MAX_STR_LEN));
  }
  cp_list_destroy(templist);
  return 0;
}

static int _cmddb_c2x_update_xpath(void *vdata, void *dummy)
{
  struct _cmddb_parms *params = dummy;
  char *incmd = vdata;
  char outcmd[MAX_STR_LEN];
  _val_update(params->valtrie, incmd, outcmd);
  strncpy(incmd, outcmd, MAX_STR_LEN);
  return 0;
}

int cmddb_c2x_convert(cp_trie *cmddb_trie, char *incmd, cmddb_cmd_send func)
{
  int res = 0;
  struct _cmddb_parms cmdparams;
  cmdparams.cmddb_trie = cmddb_trie;
  cmdparams.valtrie = cp_trie_create_trie(
      COLLECTION_MODE_NOSYNC | COLLECTION_MODE_DEEP, NULL, _free_data);
  if (cmdparams.valtrie == NULL)
  {
    fputs("valtrie is NOT created.\n", stderr);
    return -1;
  }
  // outlist = cp_list_create_list(
  //  COLLECTION_MODE_COPY | COLLECTION_MODE_DEEP | COLLECTION_MODE_MULTIPLE_VALUES,
  //  (cp_compare_fn) strcmp, (cp_copy_fn) strdup, _free_data);

  cmdparams.outlist = cp_list_create_nosync();
  if (cmdparams.outlist == NULL)
  {
    fputs("can\'t create cmd list\n", stderr);
    return -1;
  }
  res = _cmddb_c2x_get_xpath(incmd, &cmdparams);
  if (res >= 0)
  {
    // debug
    actus_log_debug("\n");
    actus_log_debug("*** success to convert cli to xpath. ***\n");
    actus_log_debug(" cli='%s'\n", incmd);
    cp_list_callback(cmdparams.outlist, _print_item, "xpath");
    actus_log_debug("\n");
    // cp_trie_dump(cmdparams.valtrie);

    // update commands with argument values
    cp_list_callback(cmdparams.outlist, _cmddb_c2x_update_xpath, &cmdparams);
    cp_list_callback(cmdparams.outlist, func, &cmdparams);
  }
  else
  {
    actus_log_error("\n");
    actus_log_error("*** fail to convert cli to xpath. ***\n");
    actus_log_error(" cli='%s'\n", incmd);
    actus_log_error("\n");
  }
  cp_list_destroy_custom(cmdparams.outlist, _free_data);
  cp_trie_destroy(cmdparams.valtrie);
  return 0;
}

int cmddb_cmd_print(void *cmdstr, void *dump)
{
  actus_log_debug("%s - %s\n", __FUNCTION__, (char *)cmdstr);
  return 0;
}

int main(void)
{
  actus_log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);
  cp_trie *cmddb_trie = cmddb_load("cmddb.yml");
  actus_log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);

  char *CLI_INPUT[] = {
      "hostname myhost\n",
      "ntp server ABC ip 192.168.44.1 port 25",
      "interface ge24/no shutdown",
      "interface Vlan1/ip address 192.168.0.2 255.255.255.0",
      "interface Vlan1/ip address 192.168.0.2/16",
      "interface Vlan1/ip address 192.168.0.2/16 secondary",
      "ethernet soam meg megid mymeg vlan 100 ccm-interval 1sec level 4",
  };

  char *XPATH_INPUT[] = {
      "C/sys:system/sys:ntp/sys:server[sys:name='ABC']/sys:udp/sys:address=192.168.44.1",
      "C/sys:system/sys:ntp/sys:server[sys:name='ABC']/sys:udp/sys:port=25",
      "M/if:interfaces/if:interface[if:name='ge10']/if:description=ge10 description!!",
      "C/if:interfaces/if:interface[if:name='Vlan1']/if:name=Vlan1",
      "D/if:interfaces/if:interface[if:name='Vlan1']/if:name=Vlan1",
      "R/sys:system/sys:hostname=myhost"};

  int i;
  cp_avltree *xpath_cmd_list = NULL;
  for (i = 0; i < (sizeof(XPATH_INPUT) / sizeof(char *)); i++)
  {
    cmddb_x2c_convert_push(&xpath_cmd_list, XPATH_INPUT[i]);
  }
  actus_log_debug("=====================================\n");
  cmddb_x2c_convert_flush(cmddb_trie, &xpath_cmd_list, &cmddb_cmd_print);

  for (i = 0; i < (sizeof(CLI_INPUT) / sizeof(char *)); i++)
  {
    actus_log_debug("=====================================\n");
    cmddb_c2x_convert(cmddb_trie, CLI_INPUT[i], &cmddb_cmd_print);
  }

  cmddb_unload(cmddb_trie);

  actus_log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);
  return 0;
}
