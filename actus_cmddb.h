#ifndef __ACTUS_CMDDB__
#define __ACTUS_CMDDB__
#include <cprops/trie.h>
#include <cprops/avl.h>

// The command of cmddb can be one of xpath format command or system cli format.
// Translation (Convertion) rule must be defined in cmd.yml file (yaml).

// cmddb callback function to send the stacked commands
typedef int cmddb_cmd_send(void *, void *);

// initialize cmddb.
cp_trie *cmddb_load(const char *filename);

// deinitialize cmddb.
void cmddb_unload(cp_trie *cmddb_trie);

// return the last crated cmddb_trie.
cp_trie *cmddb_get(void);

// the example for cmddb_cmd_send function
int cmddb_cmd_print(void *cmdstr, void *dump);

// stack xpath commands from NETCONF agent.
int cmddb_x2c_convert_push(cp_avltree **xpath_cmd_list, char *xpath);

// convert the xpath commands to system cli and then send these commands to the system using cmddb_cmd_send function.
int cmddb_x2c_convert_flush(cp_trie *cmddb_trie, cp_avltree **xpath_cmd_list, cmddb_cmd_send func);

// convert the system cli received from the system to the xpath commands and then send it to NETCONF agent to update.
int cmddb_c2x_convert(cp_trie *cmddb_trie, char *incmd, cmddb_cmd_send func);

#endif