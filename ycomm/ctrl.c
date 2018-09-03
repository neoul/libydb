#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

// getpid()
// #include <sys/types.h>
// #include <unistd.h>
#include <yaml.h>
// #include "ycomm.h"
// #include "inc.h"
char *y_space_str = "                                                         ";
void yipc_node_print(char *str, int level)
{
	if (level <= 50) // size of y_space_str
		fprintf(stdout, "%.*s%s\n", (level)*2, y_space_str, str);
}

// yipc_node_t *yipc_node_create(char *key, char *value)
// {
// 	yipc_node_t *ynode;
// 	if(!key && !value)
// 		return NULL;
// 	ynode = malloc(sizeof(yipc_node_t));
// 	if(!ynode)
// 		return NULL;
// 	memset(ynode, 0x0, sizeof(yipc_node_t));

// 	if (key)
// 	{
// 		ynode->key = strdup(key);
// 		if(!ynode->key)
// 			goto _failed;
// 	}
// 	else
// 	{
		
// 	}
	
// 	return ynode;
// _failed:
// 	if(ynode->key)
// 		free(ynode->key);
// 	free(ynode);
// }

// void yipc_node_delete(yipc_node_t *ynode)
// {
// 	if(ynode)
// 		free(ynode);
// }

// yipc_res_t yipc_node_attach(yipc_node_t *this, yipc_node_t *parent, yipc_node_t *child)
// {
// 	if (!this)
// 		return YIPC_ERR_NO_ARG;
// 	if (parent)
// 	{
// 		this->parent = parent;
// 	}
// 	if (child)
// 	{
// 		this
// 	}
	
// 	return YIPC_OK;
// }


int yipc_yaml_parser_error(yaml_parser_t *parser)
{
	/* Display a parser error message. */
	switch (parser->error)
	{
	case YAML_MEMORY_ERROR:
		fprintf(stderr, "not enough memory for parsing\n");
		break;

	case YAML_READER_ERROR:
		if (parser->problem_value != -1)
		{
			fprintf(stderr, "reader error: %s: #%X at %zd\n", parser->problem,
					parser->problem_value, parser->problem_offset);
		}
		else
		{
			fprintf(stderr, "reader error: %s at %zu\n", parser->problem,
					parser->problem_offset);
		}
		break;

	case YAML_SCANNER_ERROR:
		if (parser->context)
		{
			fprintf(stderr, "[yaml parser]\n");
			fprintf(stderr, "\t"
							"scanner error: %s at line %zu, column %zu\n",
					parser->context,
					parser->context_mark.line + 1, parser->context_mark.column + 1);
			fprintf(stderr, "\t"
							"%s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
			fprintf(stderr, "\n");
		}
		else
		{
			fprintf(stderr, "scanner error: %s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
		}
		break;

	case YAML_PARSER_ERROR:
		if (parser->context)
		{
			fprintf(stderr, "[yaml parser]\n");
			fprintf(stderr, "\t"
							"parser error: %s at line %zu, column %zu\n",
					parser->context,
					parser->context_mark.line + 1, parser->context_mark.column + 1);
			fprintf(stderr, "\t"
							"%s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
			fprintf(stderr, "\n");
		}
		else
		{
			fprintf(stderr, "parser error: %s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
		}
		break;

	case YAML_COMPOSER_ERROR:
		if (parser->context)
		{
			fprintf(stderr, "[yaml parser]\n");
			fprintf(stderr, "\t"
							"composer error: %s at line %zu, column %zu\n",
					parser->context,
					parser->context_mark.line + 1, parser->context_mark.column + 1);
			fprintf(stderr, "\t"
							"%s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
			fprintf(stderr, "\n");
		}
		else
		{
			fprintf(stderr, "composer error: %s at line %zu, column %zu\n",
					parser->problem, parser->problem_mark.line + 1,
					parser->problem_mark.column + 1);
		}
		break;

	default:
		/* Couldn't happen. */
		fprintf(stderr, "internal error\n");
		break;
	}
	return 0;
}

// yipc read data from a yaml file
void *yipc_yaml_read_file(char *file)
{
	int level = 0;
	FILE *fh = NULL;
	yaml_parser_t parser;
	yaml_token_t token; /* new variable */
	if (!file)
	{
		return NULL;
	}
	fh = fopen(file, "r");
	if (!fh)
	{
		return NULL;
	}

	/* Initialize parser */
	if (!yaml_parser_initialize(&parser))
	{
		yipc_yaml_parser_error(&parser);
		yaml_parser_delete(&parser);
		fclose(fh);
		return NULL;
	}

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

	/* BEGIN new code */
	do
	{
		yaml_parser_scan(&parser, &token);
		if (!token.type)
		{
			yipc_yaml_parser_error(&parser);
			break;
		}
		switch (token.type)
		{
		/* Stream start/end */
		case YAML_STREAM_START_TOKEN:
			yipc_node_print("[stream start]", level);
			// init top node
			level = 0;
			break;
		case YAML_STREAM_END_TOKEN:
			yipc_node_print("[stream end]", level);
			level = 0;
			break;
		case YAML_KEY_TOKEN:
			yipc_node_print("[key token]", level);
			break;
		case YAML_VALUE_TOKEN:
			yipc_node_print("[value token]", level);
			break;
			/* Block delimeters */
		case YAML_BLOCK_SEQUENCE_START_TOKEN:
			yipc_node_print("[block sequence token]", level);
			level++;
			break;
		case YAML_BLOCK_MAPPING_START_TOKEN:
			yipc_node_print("[block map token]", level);
			level++;
			break;
		case YAML_BLOCK_ENTRY_TOKEN:
			// level++;
			// yipc_node_print("[block entry token]", level);
			break;
		case YAML_BLOCK_END_TOKEN:
			level--;
			yipc_node_print("[block end]", level);
			break;
		case YAML_SCALAR_TOKEN:
		{
			char *scalar = (char *)token.data.scalar.value;
			yipc_node_print(scalar, level + 1);
			printf("scalar len=%d\n", strlen(scalar));
		}
		break;
		/* Others */
		case YAML_FLOW_SEQUENCE_START_TOKEN:
		case YAML_FLOW_SEQUENCE_END_TOKEN:
		case YAML_FLOW_MAPPING_START_TOKEN:
		case YAML_FLOW_MAPPING_END_TOKEN:
		case YAML_FLOW_ENTRY_TOKEN:
		case YAML_DOCUMENT_START_TOKEN:
		case YAML_DOCUMENT_END_TOKEN:
		case YAML_VERSION_DIRECTIVE_TOKEN:
		case YAML_TAG_DIRECTIVE_TOKEN:
		case YAML_TAG_TOKEN:
		case YAML_ANCHOR_TOKEN:
		case YAML_ALIAS_TOKEN:
		default:
			yipc_node_print("ignored token", 0);
			break;
		}
		if (token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);
	} while (token.type != YAML_STREAM_END_TOKEN);
	yaml_token_delete(&token);
	/* END new code */

	/* Cleanup */
	yaml_parser_delete(&parser);
	fclose(fh);
	return NULL;
}

int main(int argc, char *argv[])
{
	if(argc > 1)
	{
		yipc_yaml_read_file(argv[1]);
	}
}