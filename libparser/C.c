/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2008,
 *	2010, 2015
 *	Tama Communications Corporation
 *
 * This file is part of GNU GLOBAL.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "internal.h"
#include "die.h"
#include "strbuf.h"
#include "strlimcpy.h"
#include "token.h"
#include "c_res.h"

static void C_family(const struct parser_param *, int);
static void process_attribute(const struct parser_param *);
static int function_definition(const struct parser_param *, char *);
static void condition_macro(const struct parser_param *, int);
static int enumerator_list(const struct parser_param *);

#define IS_TYPE_QUALIFIER(c)	((c) == C_CONST || (c) == C_RESTRICT || (c) == C_VOLATILE)

#define DECLARATIONS    0
#define RULES           1
#define PROGRAMS        2

#define TYPE_C		0
#define TYPE_LEX	1
#define TYPE_YACC	2

#define MAXPIFSTACK	100

/*
 * #ifdef stack.
 */
static struct {
	short start;		/* level when '#if' block started */
	short end;		/* level when '#if' block end */
	short if0only;		/* '#if 0' or notdef only */
} stack[MAXPIFSTACK], *cur;
static int piflevel;		/* condition macro level */
static int level;		/* brace level */
static int externclevel;	/* 'extern "C"' block level */
char function_scope[MAXTOKEN]="<global>";

/**
 * yacc: read yacc file and pickup tag entries.
 */
	void
yacc(const struct parser_param *param)
{
	C_family(param, TYPE_YACC);
}
/**
 * C: read C file and pickup tag entries.
 */
	void
C(const struct parser_param *param)
{
	C_family(param, TYPE_C);
}

extern char curfile[MAXPATHLEN];
static char *get_next_tag_name(char *tagname)
{
	char file[MAXPATHLEN]="";
	strcpy(file, curfile);
	replace_char(file, '/', '_');
	replace_char(file, '.', '_');
	static int tag_index=0;
	sprintf(tagname, "%s__anon%d", file, tag_index++);
	return tagname;
}

static void display_tokens(char token_str[][MAXTOKEN], int count)
{
	int i=0;
	for( i=0 ; i<count ; i++ )
	{
		printf("[%d:%s]", i, token_str[i]);
	}
	printf("==>%s\n", token);

}
typedef struct symdata
{
	char parent_token[1600][1000];
	char struct_tagname[1600][1000];
	char line[200];
	char tagname[200];
	char symbol[300];
	char savetoken[MAXTOKEN][MAXTOKEN];
}SYMDATA;

static void process_bracket(const struct parser_param *param, struct symdata *sdata, int c, int cc)
{
	int parent_sym = 0;
	int tdef_token_cnt = 0;
	const char *interested = "[]{}=;,*()";
	while( (c = nexttoken(interested, c_reserved_word)) != EOF )
	{
		if(c == '=' || c == ';' || c == ',' )
		{
			tdef_token_cnt = 0;
			break;
		}
		if(c == '[' || c == '(') process_bracket(param, sdata, c, cc);
		if(c == ']' || c == ')') break;
		if(c == SYMBOL)
		{
			PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);

		}
	}
}


static void process_struct(const struct parser_param *param, struct symdata *sdata, int c, int cc)
{
	int parent_sym = 0;
	const char *interested = "{}=;";
	int tdef_token_cnt = 0;
	int sym_type[MAXTOKEN]={0};
	/*printf("===================================c[%d]===cc[%d]====token[%s]===============\n", c, cc, token);*/

	memset(sdata->parent_token, 0, sizeof(sdata->parent_token));
	memset(sdata->struct_tagname, 0, sizeof(sdata->struct_tagname));
	int savelineno = 0;
	int typedef_savelevel = level;
	sym_type[level] = cc;
	if(cc == C_TYPEDEF)
	{
		/* skip type qualifiers */
		do {
			cc = nexttoken("{}(),;", c_reserved_word);
		} while (IS_TYPE_QUALIFIER(cc) || cc == '\n');
	}

	/*
	 * This parser is too complex to maintain.
	 * We should rewrite the whole.
 */


	memset(sdata->savetoken, 0, sizeof(sdata->savetoken));
	if ((param->flags & PARSER_WARNING) && c == EOF) {
		warning("unexpected eof. [+%d %s]", lineno, curfile);
		return;
	} else if (cc == C_ENUM || cc == C_STRUCT || cc == C_UNION) {
		char *interest_enum = "[]{},;";
		int c_ = cc;					
		parent_sym = c = c_;


		if (c_ == C_ENUM) {
			if (c == '{') 
				c = enumerator_list(param);
			else
				pushbacktoken();
		} else {
			for (; c != EOF; c = nexttoken(interest_enum, c_reserved_word)) {

				/*printf("===========>[%c:%x], token[%s], level=%d, tdeflevel=%d, tdef_token_cnt=%d\n", c,c, token, level, typedef_savelevel, tdef_token_cnt);*/
				//TODO
				if(level < typedef_savelevel)break;

				int c_ = c;
				switch (c) {
					case SHARP_IMPORT:
					case SHARP_INCLUDE:
					case SHARP_INCLUDE_NEXT:
					case SHARP_ERROR:
					case SHARP_LINE:
					case SHARP_PRAGMA:
					case SHARP_WARNING:
					case SHARP_IDENT:
					case SHARP_SCCS:
						while ((c = nexttoken(interested, c_reserved_word)) != EOF && c != '\n') ;
						continue;
						break;
					case SHARP_IFDEF:
					case SHARP_IFNDEF:
					case SHARP_IF:
					case SHARP_ELIF:
					case SHARP_ELSE:
					case SHARP_ENDIF:
						condition_macro(param, c);
						continue;
						break;
					case SHARP_SHARP:		/* ## */
						(void)nexttoken(interested, c_reserved_word);
						continue;
						break;



						//KALS
					case C_STRUCT:
					case C_UNION:

						while ((c = nexttoken(interest_enum, c_reserved_word)) == C___ATTRIBUTE__)
							process_attribute(param);

						/* read tag name if exist */
						if (c == SYMBOL) {
							// TAG name process
							if (peekc(0) == '{')  {
								sym_type[level+1] = c_;
								if(strcmp(sdata->parent_token[level],"") == 0){
									sprintf(sdata->parent_token[level+1], "%s", token);
								}else{
									//CHECK
									/*sprintf(sdata->parent_token[level+1], "%s::%s", sdata->parent_token[level], token);*/
									sprintf(sdata->parent_token[level+1], "%s", token);
								}
								sprintf(sdata->struct_tagname[level+1], token);

								sprintf(sdata->line, "s|d|%s", sdata->parent_token[level+1]);
								PUT_XTAG(PARSER_DEF, sdata->parent_token[level+1], lineno, sp, sdata->line);
							}else if ( (peekc(0) == ';' || peekc(0) == '=' || peekc(0) == ',' ) && level == typedef_savelevel) {
								//TODO
								break;
							}else if( peekc(0) == '\n') 
							{
								/*while ((c = nexttoken(interested, c_reserved_word)) != EOF && c == '\n') */
								/*{*/
								/*printf("+++++++++++++++++++TOKEN[%s], C=%x+++++++++++++++++++++++++++\n", token, c);*/
								/*}*/
								continue;

							} else {
								if(level > typedef_savelevel)
								{
									/*sprintf(sdata->line, "m|s|%s|%s", sdata->parent_token[level+1], sdata->parent_token[level]);*/
									sprintf(sdata->line, "m|s|%s|%s", token, sdata->parent_token[level]);
									c = nexttoken(interested, c_reserved_word);
									if(c == SYMBOL){
										sprintf(sdata->symbol, "%s::%s", sdata->parent_token[level], token);
										PUT_XTAG(PARSER_DEF, sdata->symbol, lineno, sp, sdata->line);
										//REMOVE
										/*PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);*/
									}
								}else{

									if( sym_type[level] == C_TYPEDEF){

										sprintf(sdata->line, "t|s|d|%s", token);
										c = nexttoken(interested, c_reserved_word);
										if(c == SYMBOL){
											PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
										}
									}else{
										const char *interested_fun = "{}=;*()[]";
										/*printf("~~~~==----===~~~~~~~~~~~~~~~~~~~%d %s %x~~~~~~~~~~~~~~~~\n", c, token, peekc(0));*/
										//struct a* name() = function name
										//SKIP the new line
										while ( c != EOF && c == '\n') { c = nexttoken(interested, c_reserved_word); }
										/*printf("~~~~===++====~~~~~~~~~~~~~~~~~~~%d %s %x~~~~~~~~~~~~~~~~\n", c, token, peekc(0));*/
										if(strchr(interested_fun,c) || strchr(interested_fun, peekc(0) ) )
										{
											c = nexttoken(interested_fun, c_reserved_word);
											/*printf("~~~~=========~~~~~~~~~~~~~~~~~~~%d %s %x~~~~~~~~~~~~~~~~\n", c, token, peekc(0));*/
											sprintf(sdata->line, "-");
											/*c = nexttoken(interested, c_reserved_word);*/
											//SKIP the new line
											/*while ( c != EOF && c == '\n') { c = nexttoken(interested, c_reserved_word); }*/
											/*if(c == SYMBOL)*/
											{
												PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
											}
											return ;

										}else
										{
											sprintf(sdata->line, "s|v|%s", token);
											c = nexttoken(interested, c_reserved_word);
											if(c == SYMBOL){
												PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
											}
										}
									}

								}
							}
						}else
						{
							//SKIP the new line
							while ( c != EOF && c == '\n') { c = nexttoken(interested, c_reserved_word); }

							// NO Tag name
							if (c == '{')  {
								sym_type[level+1] = c_;
								/*sprintf(sdata->parent_token[level+1], get_next_tag_name(sdata->tagname));*/

								sprintf(sdata->struct_tagname[level+1], get_next_tag_name(sdata->tagname));
								if(strcmp(sdata->parent_token[level],"") == 0){
									sprintf(sdata->parent_token[level+1], "%s", sdata->struct_tagname[level+1]);
								}else{
									//CHECK
									//sprintf(sdata->parent_token[level+1], "%s::%s", sdata->parent_token[level], sdata->struct_tagname[level+1]);
									sprintf(sdata->parent_token[level+1], "%s", sdata->struct_tagname[level+1]);
								}
								sprintf(sdata->struct_tagname[level+1], sdata->parent_token[level+1]);
								sprintf(sdata->line, "s|d|%s", sdata->parent_token[level+1]);
								/*printf("===== %s ======\n", sdata->line);*/
								PUT_XTAG(PARSER_DEF, sdata->parent_token[level+1], lineno, sp, sdata->line);
								level++;
								if(level >= MAXTOKEN) level = MAXTOKEN-1;
							}else{
								//struct variable
								sprintf(sdata->struct_tagname[level+1], get_next_tag_name(sdata->tagname));
								if(strcmp(sdata->parent_token[level],"") == 0){
									sprintf(sdata->parent_token[level+1], "%s", sdata->struct_tagname[level+1]);
								}else{
									sprintf(sdata->parent_token[level+1], "%s::%s", sdata->parent_token[level], sdata->struct_tagname[level+1]);
								}
								/*sprintf(sdata->parent_token[level+1], get_next_tag_name(sdata->tagname));*/
							}
						}
						continue;
						break;


					default:
						break;
				}
				/*display_tokens(sdata->savetoken, tdef_token_cnt);                     */
				if (c == ';' && level == typedef_savelevel) {
					break;
				}else if (c == ';' && level != typedef_savelevel) {

					if(tdef_token_cnt ==1)
					{
						sprintf(sdata->symbol, "%s::%s", sdata->parent_token[level], token);
						/*printf("LEVEL=%d, ", level);*/
						sprintf(sdata->line, "m|d|d|%s",  sdata->parent_token[level]);
						PUT_XTAG(PARSER_DEF, sdata->symbol, lineno, sp, sdata->line);
					}
					else if(tdef_token_cnt ==2)
					{
						sprintf(sdata->symbol, "%s::%s", sdata->parent_token[level], token);
						sprintf(sdata->line, "m|t|%s|%s", sdata->savetoken[0],  sdata->parent_token[level]);
						PUT_XTAG(PARSER_DEF, sdata->symbol, lineno, sp, sdata->line);
						//REMOVE
						/*PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);*/
					}
					tdef_token_cnt =0;
					strcpy(sdata->savetoken[tdef_token_cnt], "d");
				}else if(c == '[' || c == '(' )
				{
					char temp[MAXTOKEN];
					strcpy(temp, token);
					process_bracket(param, sdata, c, cc);
					strcpy(token, temp);

				} else if (c == '{'){
					level++;
					tdef_token_cnt =0;
					strcpy(sdata->savetoken[tdef_token_cnt], "d");
					if(level >= MAXTOKEN) level = MAXTOKEN-1;
					//KALS
				}else if (c == '}') {
					level--;
					if(level<0) level=0;

					if(level == 0) strcpy(function_scope, "<global>");

					tdef_token_cnt =0;
					const char *interested = "[]{}=;,*()";
					while( (c = nexttoken(interested, c_reserved_word)) != EOF )
					{
						if(c == '=' || c == ';' || c == ',' )
						{
							tdef_token_cnt = 0;
							break;
						}
						if(c == '[' || c == '(' ){
							process_bracket(param, sdata, c, cc);
						}

						if (level == typedef_savelevel)
						{
							if(c == SYMBOL && (sym_type[level] == C_TYPEDEF))
							{
								sprintf(sdata->line, "t|s|d|%s", sdata->parent_token[level+1]);
								PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
							}
							else if(c == SYMBOL && (sym_type[level] == C_STRUCT || sym_type[level] == C_UNION || sym_type[level] == C_ENUM))
							{
								sprintf(sdata->line, "s|v|%s", sdata->parent_token[level+1]);
								PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
							}
						}else{
							if(c == SYMBOL && (sym_type[level] == C_TYPEDEF))
							{
								//KALS
								sprintf(sdata->line, "t|s|d|%s", sdata->parent_token[level+1]);
								PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);
							}
							else if(c == SYMBOL && (sym_type[level] == C_STRUCT || sym_type[level] == C_UNION || sym_type[level] == C_ENUM))
							{

								sprintf(sdata->symbol, "%s::%s", sdata->parent_token[level], token);
								/*sprintf(sdata->line, "m|s|%s|%s", token, sdata->parent_token[level]);*/
								sprintf(sdata->line, "m|s|%s|%s", sdata->struct_tagname[level+1], sdata->parent_token[level]);
								PUT_XTAG(PARSER_DEF, sdata->symbol, lineno, sp, sdata->line);
								sprintf(sdata->line, "s|v|%s", sdata->parent_token[level+1]);
								//REMOVE
								/*PUT_XTAG(PARSER_DEF, token, lineno, sp, sdata->line);*/
							}
						}
					}
					strcpy(sdata->savetoken[tdef_token_cnt], sdata->parent_token[level] );

					if (level == typedef_savelevel)
						break;
				} else if (c == SYMBOL) {
					strlimcpy(sdata->savetoken[tdef_token_cnt++], token, MAXTOKEN);
					savelineno = lineno;
					if(tdef_token_cnt >= MAXTOKEN)tdef_token_cnt = MAXTOKEN-1;
				}
			}
			if (level <= typedef_savelevel)
				return;
			if (c == ';')
				return;
		}
		if ((param->flags & PARSER_WARNING) && c == EOF) {
			warning("unexpected eof. [+%d %s]", lineno, curfile);
			return;
		}
	} else if (c == SYMBOL) {
		PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
	}
	memset(sdata->savetoken, 0, sizeof(sdata->savetoken));
	while ((c = nexttoken("(),;", c_reserved_word)) != EOF) {
		switch (c) {
			case SHARP_IFDEF:
			case SHARP_IFNDEF:
			case SHARP_IF:
			case SHARP_ELIF:
			case SHARP_ELSE:
			case SHARP_ENDIF:
				condition_macro(param, c);
				continue;
			default:
				break;
		}
		if (c == '(')
		{
			level++;
			if(level >= MAXTOKEN) level = MAXTOKEN-1;
		}
		else if (c == ')')
		{
			level--;
			if(level<0) level=0;
		}
		else if (c == SYMBOL) {
			if (level > typedef_savelevel) {
				PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
			} else {
				/* put latest token if any */
				if (sdata->savetoken[0][0]) {
					PUT_XTAG(PARSER_REF_SYM, sdata->savetoken[0], savelineno, sp, function_scope);
				}
				/* save lastest token */
				strlimcpy(sdata->savetoken[0], token, MAXTOKEN);
				savelineno = lineno;
			}
		} else if (c == ',' || c == ';') {
			if (sdata->savetoken[0][0]) {
				//KALS
				sprintf(sdata->line, "t|s|%s", sdata->parent_token[level+1]);
				/*PUT(PARSER_DEF, savetok, lineno, sp);*/
				PUT_XTAG(PARSER_DEF, sdata->savetoken[0], lineno, sp, sdata->line);
				sdata->savetoken[0][0] = 0;
			}
		}
		if (level == typedef_savelevel && c == ';')
			break;
	}
	if (param->flags & PARSER_WARNING) {
		if (c == EOF)
			warning("unexpected eof. [+%d %s]", lineno, curfile);
		else if (level != typedef_savelevel)
			warning("unmatched () block. (last at level %d.)[+%d %s]", level, lineno, curfile);
	}


}

	static void
C_family(const struct parser_param *param, int type)
{
	int c, cc;
	int savelevel;
	//KALS
	int parent_sym = 0;
	int struct_close = 0;
	int sym_type[MAXTOKEN];
	char savetok[MAXTOKEN];
	int usr_define_datatype = 0;
	char old[300];
	static SYMDATA sdata = {0};

	int startmacro, startsharp;
	const char *interested = "{}=;";
	STRBUF *sb = strbuf_open(0);
	/*
	 * yacc file format is like the following.
	 *
	 * declarations
	 * %%
	 * rules
	 * %%
	 * programs
	 *
	 */
	int yaccstatus = (type == TYPE_YACC) ? DECLARATIONS : PROGRAMS;
	int inC = (type == TYPE_YACC) ? 0 : 1;	/* 1 while C source */


	//KALS
	memset(&sdata, 0x00, sizeof(SYMDATA));


	level = piflevel = externclevel = 0;
	savelevel = -1;
	startmacro = startsharp = 0;

	if (!opentoken(param->file))
		die("'%s' cannot open.", param->file);
	cmode = 1;			/* allow token like '#xxx' */
	crflag = 1;			/* require '\n' as a token */
	if (type == TYPE_YACC)
		ymode = 1;		/* allow token like '%xxx' */
	//KALS
	while ((cc = nexttoken(interested, c_reserved_word)) != EOF) {

		/*printf("--------------->[%c:%x], token[%s], level=%d\n", cc,cc, token, level );*/
		switch (cc) {
		case SYMBOL:		/* symbol	*/
			if (inC && peekc(0) == '('/* ) */) {
				if (param->isnotfunction(token)) {
						PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
				} else if (level > 0 || startmacro) {
						PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
				} else if (level == 0 && !startmacro && !startsharp) {
						char arg1[MAXTOKEN],  *saveline;
					int savelineno = lineno;

					strlimcpy(savetok, token, sizeof(savetok));
					strbuf_reset(sb);
					strbuf_puts(sb, sp);
					saveline = strbuf_value(sb);
					arg1[0] = '\0';
					/*
					 * Guile function entry using guile-snarf is like follows:
					 *
					 * SCM_DEFINE (scm_list, "list", 0, 0, 1, 
					 *           (SCM objs),
					 *            "Return a list containing OBJS, the arguments to `list'.")
					 * #define FUNC_NAME s_scm_list
					 * {
					 *   return objs;
					 * }
					 * #undef FUNC_NAME
					 *
					 * We should assume the first argument as a function name instead of 'SCM_DEFINE'.
					 */
					if (function_definition(param, arg1)) {
						if (!strcmp(savetok, "SCM_DEFINE") && *arg1)
							strlimcpy(savetok, arg1, sizeof(savetok));

							PUT_XTAG(PARSER_DEF, savetok, savelineno, saveline, function_scope);
							if(level == 0) strcpy(function_scope, savetok);
					} else {
							PUT_XTAG(PARSER_REF_SYM, savetok, savelineno, saveline, function_scope);
					}
				}
			} else {
					/*printf("--------------\n");*/
					//KALS
					if( inC && level == 0 && !startmacro && !startsharp){
						if (peekc(0) == '('/* ) */ ||  peekc(0) == '[' ||  peekc(0) == ',' ||  peekc(0) == ';' ||  peekc(0) == '='  ) {
							PUT_XTAG(PARSER_DEF, token, lineno, sp, function_scope);
						}
					}else{
						/*printf("-----------kalit %s---\n", token);*/
						PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
			}
			}
			break;
				//KALS
			case ';':
			break;
			case '{':
				DBG_PRINT(level, "{");
			if (yaccstatus == RULES && level == 0)
				inC = 1;
			++level;
			if ((param->flags & PARSER_BEGIN_BLOCK) && atfirst) {
				if ((param->flags & PARSER_WARNING) && level != 1)
					warning("forced level 1 block start by '{' at column 0 [+%d %s].", lineno, curfile); /* } */
				level = 1;
			}
				if(level>1000)printf("Warning ....................:%d\n", level);
			break;
			/* { */
		case '}':
			if (--level < 0) {
				if (externclevel > 0)
					externclevel--;
				else if (param->flags & PARSER_WARNING)
					warning("missing left '{' [+%d %s].", lineno, curfile); /* } */
				level = 0;
			}
			if ((param->flags & PARSER_END_BLOCK) && atfirst) {
				if ((param->flags & PARSER_WARNING) && level != 0) /* { */
					warning("forced level 0 block end by '}' at column 0 [+%d %s].", lineno, curfile);
				level = 0;
			}
				//KALS
			if (yaccstatus == RULES && level == 0)
				{
				inC = 0;
					parent_sym = 0;
				}
			DBG_PRINT(level, "}");
			break;
		case '\n':
			if (startmacro && level != savelevel) {
				if (param->flags & PARSER_WARNING)
					warning("different level before and after #define macro. reseted. [+%d %s].", lineno, curfile);
				level = savelevel;
			}
			startmacro = startsharp = 0;
			break;
		case YACC_SEP:		/* %% */
			if (level != 0) {
				if (param->flags & PARSER_WARNING)
					warning("forced level 0 block end by '%%' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (yaccstatus == DECLARATIONS) {
				PUT(PARSER_DEF, "yyparse", lineno, sp);
				yaccstatus = RULES;
			} else if (yaccstatus == RULES)
				yaccstatus = PROGRAMS;
			inC = (yaccstatus == PROGRAMS) ? 1 : 0;
			break;
		case YACC_BEGIN:	/* %{ */
			if (level != 0) {
				if (param->flags & PARSER_WARNING)
					warning("forced level 0 block end by '%%{' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (inC == 1 && (param->flags & PARSER_WARNING))
				warning("'%%{' appeared in C mode. [+%d %s].", lineno, curfile);
			inC = 1;
			break;
		case YACC_END:		/* %} */
			if (level != 0) {
				if (param->flags & PARSER_WARNING)
					warning("forced level 0 block end by '%%}' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (inC == 0 && (param->flags & PARSER_WARNING))
				warning("'%%}' appeared in Yacc mode. [+%d %s].", lineno, curfile);
			inC = 0;
			break;
		case YACC_UNION:	/* %union {...} */
			if (yaccstatus == DECLARATIONS)
				PUT(PARSER_DEF, "YYSTYPE", lineno, sp);
			break;
		/*
		 * #xxx
		 */
		case SHARP_DEFINE:
		case SHARP_UNDEF:
			startmacro = 1;
			savelevel = level;
			if ((c = nexttoken(interested, c_reserved_word)) != SYMBOL) {
				pushbacktoken();
					/*break;*/
			}
				do
				{
					if (peekc(1) == '(') {
			PUT_XTAG(PARSER_DEF, token, lineno, sp, function_scope);
						while ((c = nexttoken("()", c_reserved_word)) != EOF && c != '\n' && c != ')')
					if (c == SYMBOL)
					PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
				if (c == '\n')
					pushbacktoken();
			} else {
			PUT_XTAG(PARSER_DEF, token, lineno, sp, function_scope);
						while ((c = nexttoken("()", c_reserved_word)) != EOF && c != '\n')
							if (c == SYMBOL)
					PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
						if (c == '\n')
							pushbacktoken();
			}
				}while(c != '\n');
			break;
		case SHARP_IMPORT:
		case SHARP_INCLUDE:
		case SHARP_INCLUDE_NEXT:
		case SHARP_ERROR:
		case SHARP_LINE:
		case SHARP_PRAGMA:
		case SHARP_WARNING:
		case SHARP_IDENT:
		case SHARP_SCCS:
			while ((c = nexttoken(interested, c_reserved_word)) != EOF && c != '\n')
				;
			break;
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(param, cc);
			break;
		case SHARP_SHARP:		/* ## */
			(void)nexttoken(interested, c_reserved_word);
			break;
		case C_EXTERN: /* for 'extern "C"/"C++"' */
			if (peekc(0) != '"') /* " */
				continue; /* If does not start with '"', continue. */
			while ((c = nexttoken(interested, c_reserved_word)) == '\n')
				;
			/*
			 * 'extern "C"/"C++"' block is a kind of namespace block.
			 * (It doesn't have any influence on level.)
			 */
			if (c == '{') /* } */
				externclevel++;
			else
				pushbacktoken();
			break;
				//KALS
				//REMOVED
		/* control statement check */
		case C_BREAK:
		case C_CASE:
		case C_CONTINUE:
		case C_DEFAULT:
		case C_DO:
		case C_ELSE:
		case C_FOR:
		case C_GOTO:
		case C_IF:
		case C_RETURN:
		case C_SWITCH:
		case C_WHILE:
			if ((param->flags & PARSER_WARNING) && !startmacro && level == 0)
				warning("Out of function. %8s [+%d %s]", token, lineno, curfile);
			break;
		case C_TYPEDEF:
				//KALS
			case C_STRUCT:
			case C_ENUM:
			case C_UNION:
				process_struct(param, &sdata, c, cc);
			break;
		case C___ATTRIBUTE__:
			process_attribute(param);
			break;
		default:
			break;
		}
	}
	strbuf_close(sb);
	if (param->flags & PARSER_WARNING) {
		if (level != 0)
			warning("unmatched {} block. (last at level %d.)[+%d %s]", level, lineno, curfile);
		if (piflevel != 0)
			warning("unmatched #if block. (last at level %d.)[+%d %s]", piflevel, lineno, curfile);
	}
	closetoken();
}
/**
 * process_attribute: skip attributes in '__attribute__((...))'.
 */
static void
process_attribute(const struct parser_param *param)
{
	int brace = 0;
	int c;
	/*
	 * Skip '...' in __attribute__((...))
	 * but pick up symbols in it.
	 */
	while ((c = nexttoken("()", c_reserved_word)) != EOF) {
		if (c == '(')
			brace++;
		else if (c == ')')
			brace--;
		else if (c == SYMBOL) {
			PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
		}
		if (brace == 0)
			break;
	}
}
/**
 * function_definition: return if function definition or not.
 *
 *	@param	param	
 *	@param[out]	arg1	the first argument
 *	@return	target type
 */
static int
function_definition(const struct parser_param *param, char arg1[MAXTOKEN])
{
	int c;
	int brace_level, isdefine;
	int accept_arg1 = 0;

	brace_level = isdefine = 0;
	while ((c = nexttoken("()", c_reserved_word)) != EOF) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(param, c);
			continue;
		default:
			break;
		}
		if (c == '('/* ) */)
			brace_level++;
		else if (c == /* ( */')') {
			if (--brace_level == 0)
				break;
		}
		/* pick up symbol */
		if (c == SYMBOL) {
			if (accept_arg1 == 0) {
				accept_arg1 = 1;
				strlimcpy(arg1, token, MAXTOKEN);
			}
			PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
		}
	}
	if (c == EOF)
		return 0;
	brace_level = 0;
	while ((c = nexttoken(",;[](){}=", c_reserved_word)) != EOF) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(param, c);
			continue;
		case C___ATTRIBUTE__:
			process_attribute(param);
			continue;
		case SHARP_DEFINE:
			pushbacktoken();
			return 0;
		default:
			break;
		}
		if (c == '('/* ) */ || c == '[')
			brace_level++;
		else if (c == /* ( */')' || c == ']')
			brace_level--;
		else if (brace_level == 0
		    && ((c == SYMBOL && strcmp(token, "__THROW")) || IS_RESERVED_WORD(c)))
			isdefine = 1;
		else if (c == ';' || c == ',') {
			if (!isdefine)
				break;
		} else if (c == '{' /* } */) {
			pushbacktoken();
			return 1;
		} else if (c == /* { */'}')
			break;
		else if (c == '=')
			break;

		/* pick up symbol */
		if (c == SYMBOL)
				PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
	}
	return 0;
}

/**
 * condition_macro: 
 *
 *	@param	param	
 *	@param[in]	cc	token
 */
static void
condition_macro(const struct parser_param *param, int cc)
{
	cur = &stack[piflevel];
	if (cc == SHARP_IFDEF || cc == SHARP_IFNDEF || cc == SHARP_IF) {
		DBG_PRINT(piflevel, "#if");
		if (++piflevel >= MAXPIFSTACK)
		{
			//KALS
			/*die("#if stack over flow. [%s]", curfile);*/
			piflevel--;
		}
		++cur;
		cur->start = level;
		cur->end = -1;
		cur->if0only = 0;
		if (peekc(0) == '0')
			cur->if0only = 1;
		else if ((cc = nexttoken(NULL, c_reserved_word)) == SYMBOL && !strcmp(token, "notdef"))
			cur->if0only = 1;
		else
		{
			pushbacktoken();
		}
	} else if (cc == SHARP_ELIF || cc == SHARP_ELSE) {
		DBG_PRINT(piflevel - 1, "#else");
		if (cur->end == -1)
			cur->end = level;
		else if (cur->end != level && (param->flags & PARSER_WARNING))
			warning("uneven level. [+%d %s]", lineno, curfile);
		level = cur->start;
		cur->if0only = 0;
	} else if (cc == SHARP_ENDIF) {
		int minus = 0;

		--piflevel;
		if (piflevel < 0) {
			minus = 1;
			piflevel = 0;
		}
		DBG_PRINT(piflevel, "#endif");
		if (minus) {
			if (param->flags & PARSER_WARNING)
				warning("unmatched #if block. reseted. [+%d %s]", lineno, curfile);
		} else {
			if (cur->if0only)
				level = cur->start;
			else if (cur->end != -1) {
				if (cur->end != level && (param->flags & PARSER_WARNING))
					warning("uneven level. [+%d %s]", lineno, curfile);
				level = cur->end;
			}
		}
	}
	while ((cc = nexttoken(NULL, c_reserved_word)) != EOF && cc != '\n') {
		if (cc == SYMBOL && strcmp(token, "defined") != 0)
			PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
	}
}

/**
 * enumerator_list: process "symbol (= expression), ... "}
 */
static int
enumerator_list(const struct parser_param *param)
{
	int savelevel = level;
	int in_expression = 0;
	int c = '{';

	for (; c != EOF; c = nexttoken("{}(),=", c_reserved_word)) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(param, c);
			break;
		case SYMBOL:
			if (in_expression)
					PUT_XTAG(PARSER_REF_SYM, token, lineno, sp, function_scope);
			else
				PUT(PARSER_DEF, token, lineno, sp);
			break;
		case '{':
		case '(':
			level++;
			break;
		case '}':
		case ')':
			if (--level == savelevel)
				return c;
			break;
		case ',':
			if (level == savelevel + 1)
				in_expression = 0;
			break;
		case '=':
			in_expression = 1;
			break;
		default:
			break;
		}
	}

	return c;
}
