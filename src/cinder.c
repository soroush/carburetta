#ifndef STDIO_H_INCLUDED
#define STDIO_H_INCLUDED
#include <stdio.h>
#endif

#ifndef STDLIB_H_INCLUDED
#define STDLIB_H_INCLUDED
#include <stdlib.h>
#endif

#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED
#include <string.h>
#endif

#ifndef CTYPE_H_INCLUDED
#define CTYPE_H_INCLUDED
#include <ctype.h>
#endif

#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED
#include <assert.h>
#endif

#ifndef HELPERS_H_INCLUDED
#define HELPERS_H_INCLUDED
#include "helpers.h"
#endif

#ifndef KLT_LOGGER_H_INCLUDED
#define KLT_LOGGER_H_INCLUDED
#define KLT_LOG_MODULE "cinder"
#include "klt_logger.h"
#endif

#ifndef LOG_FUNCTION_H_INCLUDED
#define LOG_FUNCTION_H_INCLUDED
#include "log_function.h"
#endif

#ifndef REPORT_ERROR_H_INCLUDED
#define REPORT_ERROR_H_INCLUDED
#include "report_error.h"
#endif

#ifndef SCANNER_H_INCLUDED
#define SCANNER_H_INCLUDED
#include "scanner.h"
#endif

#ifndef TOKENIZER_H_INCLUDED
#define TOKENIZER_H_INCLUDED
#include "tokenizer.h"
#endif

#ifndef LINE_ASSEMBLY_H_INCLUDED
#define LINE_ASSEMBLY_H_INCLUDED
#include "line_assembly.h"
#endif

#ifndef LINE_DEFS_H_INCLUDED
#define LINE_DEFS_H_INCLUDED
#include "line_defs.h"
#endif

#ifndef DECOMMENT_H_INCLUDED
#define DECOMMENT_H_INCLUDED
#include "decomment.h"
#endif

#ifndef TOKENS_H_INCLUDED
#define TOKENS_H_INCLUDED
#include "tokens.h"
#endif

#ifndef PRD_GRAM_H_INCLUDED
#define PRD_GRAM_H_INCLUDED
#include "prd_gram.h"
#endif

#ifndef GRAMMAR_TABLE_H_INCLUDED
#define GRAMMAR_TABLE_H_INCLUDED
#include "grammar_table.h"
#endif

#ifndef LALR_H_INCLUDED
#define LALR_H_INCLUDED
#include "lalr.h"
#endif

#ifndef SYMBOL_H_INCLUDED
#define SYMBOL_H_INCLUDED
#include "symbol.h"
#endif

#ifndef TYPESTR_H_INCLUDED
#define TYPESTR_H_INCLUDED
#include "typestr.h"
#endif

struct part {
  struct part *next_;
  size_t num_chars_;
  char *chars_;
};

static struct part *append_part(struct part **tailptr, size_t num_chars, char *chars) {
  struct part *p = (struct part *)malloc(sizeof(struct part));
  if (!p) return NULL;
  p->chars_ = (char *)malloc(num_chars + 1);
  if (!p->chars_) {
    free(p);
    return NULL;
  }
  memcpy(p->chars_, chars, num_chars);
  p->chars_[num_chars] = '\0';
  p->num_chars_ = num_chars;
  if (*tailptr) {
    p->next_ = (*tailptr)->next_;
    (*tailptr)->next_ = p;
    *tailptr = p;
  }
  else {
    *tailptr = p;
    p->next_ = p;
  }

  return p;
}

struct cinder_context {
  struct snippet token_type_;
  struct symbol_table symtab_;
  struct typestr_table tstab_;
  struct typestr *most_recent_typestr_;
  struct typestr *token_assigned_type_;
  struct snippet token_action_snippet_;
};

void cinder_context_init(struct cinder_context *cc) {
  snippet_init(&cc->token_type_);
  symbol_table_init(&cc->symtab_);
  typestr_table_init(&cc->tstab_);
  cc->most_recent_typestr_ = NULL;
  cc->token_assigned_type_ = NULL;
  snippet_init(&cc->token_action_snippet_);
}

void cinder_context_cleanup(struct cinder_context *cc) {
  snippet_cleanup(&cc->token_type_);
  symbol_table_cleanup(&cc->symtab_);
  typestr_table_cleanup(&cc->tstab_);
  snippet_cleanup(&cc->token_action_snippet_);
}


static int process_cinder_directive(struct tkr_tokenizer *tkr_tokens, struct xlts *directive_line_match, struct cinder_context *cc) {
  int r;
  int ate_percent = 0;
  int ate_directive = 0;
  int ate_colon_seperator = 0;
  int found_a_placeholder = 0;
  struct symbol_table *st = &cc->symtab_;
  struct symbol **typed_symbols = NULL;
  size_t num_typed_symbols = 0;
  size_t num_typed_symbols_allocated = 0;
  struct snippet dir_snippet;
  snippet_init(&dir_snippet);
  enum {
    PCD_NT_DIRECTIVE,
    PCD_TOKEN_DIRECTIVE,
    PCD_TOKEN_TYPE_DIRECTIVE,
    PCD_NT_TYPE_DIRECTIVE,
    PCD_CONSTRUCTOR_DIRECTIVE,
    PCD_DESTRUCTOR_DIRECTIVE,
    PCD_TOKEN_ACTION_DIRECTIVE
  } directive;
  tok_switch_to_nonterminal_idents(tkr_tokens);

  r = tkr_tokenizer_inputx(tkr_tokens, directive_line_match, 1);
  while ((r != TKR_END_OF_INPUT) && (r != TKR_FEED_ME)) {
    if (r == TKR_SYNTAX_ERROR) {
      if (isprint(tkr_str(tkr_tokens)[0])) {
        re_error_tkr(tkr_tokens, "Syntax error character \"%s\" not expected", tkr_str(tkr_tokens));
      }
      else {
        re_error_tkr(tkr_tokens, "Syntax error character 0x%02x not expected", tkr_str(tkr_tokens)[0]);
      }
    }
    else if (r == TKR_INTERNAL_ERROR) {
      goto cleanup_exit;
    }
    else if (r == TKR_MATCH) {
      int eat_whitespace = 0;

      if (ate_directive) {
        if ((directive == PCD_CONSTRUCTOR_DIRECTIVE) ||
            (directive == PCD_DESTRUCTOR_DIRECTIVE) ||
            (directive == PCD_TOKEN_ACTION_DIRECTIVE)) {
          eat_whitespace = 0;
          /* Keep whitespace for all code directives */
        }
        else {
          /* Consume whitespace for all type directives */
          eat_whitespace = 1;
        }
      }
      if (eat_whitespace && (tkr_tokens->best_match_variant_ == TOK_WHITESPACE)) {
        /* eat silently */
      }
      else {
        if (!ate_percent) {
          if (tkr_tokens->best_match_variant_ == TOK_PERCENT) {
            /* leading percent sign indicates cinder directive, eat it */
            ate_percent = 1;
          }
          else {
            re_error_tkr(tkr_tokens, "Syntax error, \"%%\" expected");
            r = TKR_SYNTAX_ERROR;
            goto cleanup_exit;
          }
        }
        else if (!ate_directive) {
          if (tkr_tokens->best_match_variant_ == TOK_IDENT) {
            ate_directive = 1;
            if (!strcmp("nt", tkr_str(tkr_tokens))) {
              directive = PCD_NT_DIRECTIVE;
            }
            else if (!strcmp("token", tkr_str(tkr_tokens))) {
              directive = PCD_TOKEN_DIRECTIVE;
            }
            else if (!strcmp("token_type", tkr_str(tkr_tokens))) {
              directive = PCD_TOKEN_TYPE_DIRECTIVE;
              snippet_clear(&cc->token_type_);
            }
            else if (!strcmp("nt_type", tkr_str(tkr_tokens))) {
              directive = PCD_NT_TYPE_DIRECTIVE;
            }
            else if (!strcmp("constructor", tkr_str(tkr_tokens))) {
              directive = PCD_CONSTRUCTOR_DIRECTIVE;
              if (!cc->most_recent_typestr_) {
                re_error_tkr(tkr_tokens, "%%constructor must follow %%token_type or %%nt_type directive");
              }
            }
            else if (!strcmp("destructor", tkr_str(tkr_tokens))) {
              directive = PCD_DESTRUCTOR_DIRECTIVE;
              if (!cc->most_recent_typestr_) {
                re_error_tkr(tkr_tokens, "%%destructor must follow %%token_type or %%nt_type directive");
              }
            }
            else if (!strcmp("token_action", tkr_str(tkr_tokens))) {
              directive = PCD_TOKEN_ACTION_DIRECTIVE;
            }
            else if (!strcmp("debug_end", tkr_str(tkr_tokens))) {
              re_error(directive_line_match, "%%debug_end encountered, terminating early");
              r = TKR_INTERNAL_ERROR;
              goto cleanup_exit;
            }
            else {
              re_error_tkr(tkr_tokens, "Syntax error invalid directive");
              r = TKR_SYNTAX_ERROR;
              goto cleanup_exit;
            }
          }
        }
        else {
          if (directive == PCD_NT_DIRECTIVE) {
            if (tkr_tokens->best_match_variant_ == TOK_IDENT) {
              int is_new = -1;
              struct symbol *sym = symbol_find_or_add(st, SYM_NONTERMINAL, &tkr_tokens->xmatch_, &is_new);
              if (!is_new) {
                re_error_tkr(tkr_tokens, "Non-terminal \"%s\" already declared at line %d", tkr_str(tkr_tokens), xlts_line(&sym->def_));
              }
            }
            else {
              re_error_tkr(tkr_tokens, "Syntax error identifier expected");
              r = TKR_SYNTAX_ERROR;
              goto cleanup_exit;
            }
          }
          else if (directive == PCD_TOKEN_DIRECTIVE) {
            if (tkr_tokens->best_match_variant_ == TOK_IDENT) {
              int is_new = -1;
              struct symbol *sym = symbol_find_or_add(st, SYM_TERMINAL, &tkr_tokens->xmatch_, &is_new);
              if (!is_new) {
                re_error_tkr(tkr_tokens, "Token \"%s\" already declared at line %d", tkr_str(tkr_tokens), xlts_line(&sym->def_));
              }
            }
            else {
              re_error_tkr(tkr_tokens, "Syntax error identifier expected");
              r = TKR_SYNTAX_ERROR;
              goto cleanup_exit;
            }
          }
          else if (directive == PCD_TOKEN_TYPE_DIRECTIVE) {
            /* Trim simple whitespace off the head end, otherwise simply copy. */
            /* XXX: Continue with this! */
            if (cc->token_type_.num_tokens_ || (tkr_tokens->best_match_action_ != TOK_WHITESPACE)) {
              if (tkr_tokens->best_match_variant_ == TOK_SPECIAL_IDENT) {
                if (strcmp("$", tkr_str(tkr_tokens))) {
                  re_error_tkr(tkr_tokens, "Error: \"%s\" not allowed, a type string may only have a single \"$\" special identifier as a declarator identifier placeholder", tkr_str(tkr_tokens));
                  r = TKR_SYNTAX_ERROR;
                  goto cleanup_exit;
                }
                if (found_a_placeholder) {
                  re_error_tkr(tkr_tokens, "Error: a type string may have at most 1 declarator identifier placeholder");
                  r = TKR_SYNTAX_ERROR;
                  goto cleanup_exit;
                }
                found_a_placeholder = 1;
              }
              r = snippet_append_tkr(&cc->token_type_, tkr_tokens);
              if (r) {
                r = TKR_INTERNAL_ERROR;
                goto cleanup_exit;
              }
            }
          }
          else if (directive == PCD_NT_TYPE_DIRECTIVE) {
            if (!ate_colon_seperator) {
              if (tkr_tokens->best_match_action_ != TOK_COLON) {
                if (tkr_tokens->best_match_action_ == TOK_IDENT) {
                  struct symbol *sym = symbol_find(st, tkr_str(tkr_tokens));
                  if (!sym) {
                    re_error_tkr(tkr_tokens, "Error: \"%s\" is not a non-terminal\n", tkr_str(tkr_tokens));
                    r = TKR_SYNTAX_ERROR;
                    goto cleanup_exit;
                  }
                  else if (sym->st_ != SYM_NONTERMINAL) {
                    re_error_tkr(tkr_tokens, "Error: \"%s\" is not a non-terminal but a token.\n", tkr_str(tkr_tokens));
                    r = TKR_SYNTAX_ERROR;
                    goto cleanup_exit;
                  }
                  else {
                    size_t n;
                    for (n = 0; n < num_typed_symbols; ++n) {
                      if (sym == typed_symbols[n]) {
                        re_error_tkr(tkr_tokens, "Error: \"%s\" appears more than once in type directive\n", tkr_str(tkr_tokens));
                        r = TKR_SYNTAX_ERROR;
                        goto cleanup_exit;
                      }
                    }
                    if (num_typed_symbols == num_typed_symbols_allocated) {
                      size_t new_num_alloc = num_typed_symbols_allocated * 2 + 1;
                      if (new_num_alloc < num_typed_symbols_allocated) {
                        re_error_tkr(tkr_tokens, "Error: overflow on allocation\n");
                        r = TKR_INTERNAL_ERROR;
                        goto cleanup_exit;
                      }
                      if (new_num_alloc > (SIZE_MAX / sizeof(struct symbol *))) {
                        re_error_tkr(tkr_tokens, "Error: overflow on allocation\n");
                        r = TKR_INTERNAL_ERROR;
                        goto cleanup_exit;
                      }
                      size_t alloc_size = new_num_alloc * sizeof(struct symbol *);
                      void *p = realloc(typed_symbols, alloc_size);
                      if (!p) {
                        re_error_tkr(tkr_tokens, "Error: no memory\n");
                        r = TKR_INTERNAL_ERROR;
                        goto cleanup_exit;
                      }
                      typed_symbols = (struct symbol **)p;
                      num_typed_symbols_allocated = new_num_alloc;
                    }
                    typed_symbols[num_typed_symbols++] = sym;
                  }
                }
                else {
                  re_error_tkr(tkr_tokens, "Error: \"%s\" is not a non-terminal\n", tkr_str(tkr_tokens));
                  r = TKR_SYNTAX_ERROR;
                  goto cleanup_exit;
                }
              }
              else {
                ate_colon_seperator = 1;
              }
            }
            else {
              /* ate_colon_seperator -- everything else is part of the type, use the same logic
               * as for token_type */
              if (dir_snippet.num_tokens_ || (tkr_tokens->best_match_action_ != TOK_WHITESPACE)) {
                if (tkr_tokens->best_match_variant_ == TOK_SPECIAL_IDENT) {
                  if (strcmp("$", tkr_str(tkr_tokens))) {
                    re_error_tkr(tkr_tokens, "Error: \"%s\" not allowed, a type string may only have a single \"$\" special identifier as a declarator identifier placeholder", tkr_str(tkr_tokens));
                    r = TKR_SYNTAX_ERROR;
                    goto cleanup_exit;
                  }
                  if (found_a_placeholder) {
                    re_error_tkr(tkr_tokens, "Error: a type string may have at most 1 declarator identifier placeholder");
                    r = TKR_SYNTAX_ERROR;
                    goto cleanup_exit;
                  }
                  found_a_placeholder = 1;
                }
                r = snippet_append_tkr(&dir_snippet, tkr_tokens);
                if (r) {
                  r = TKR_INTERNAL_ERROR;
                  goto cleanup_exit;
                }
              }
            }
          }
          else if (directive == PCD_CONSTRUCTOR_DIRECTIVE) {
            if (dir_snippet.num_tokens_ || (tkr_tokens->best_match_action_ != TOK_WHITESPACE)) {
              if ((tkr_tokens->best_match_variant_ == TOK_SPECIAL_IDENT) &&
                  (tkr_tokens->best_match_action_ != TOK_SPECIAL_IDENT_DST)) {
                re_error_tkr(tkr_tokens, "Error: \"%s\" not allowed, a constructor may only have a single \"$$\" special identifier as the object to initialize", tkr_str(tkr_tokens));
                r = TKR_SYNTAX_ERROR;
                goto cleanup_exit;
              }
              r = snippet_append_tkr(&dir_snippet, tkr_tokens);
              if (r) {
                r = TKR_INTERNAL_ERROR;
                goto cleanup_exit;
              }
            }
          }
          else if (directive == PCD_DESTRUCTOR_DIRECTIVE) {
            if (dir_snippet.num_tokens_ || (tkr_tokens->best_match_action_ != TOK_WHITESPACE)) {
              if ((tkr_tokens->best_match_variant_ == TOK_SPECIAL_IDENT) &&
                  (tkr_tokens->best_match_action_ != TOK_SPECIAL_IDENT_DST)) {
                re_error_tkr(tkr_tokens, "Error: \"%s\" not allowed, a destructor may only have a single \"$$\" special identifier as the object to deconstruct", tkr_str(tkr_tokens));
                r = TKR_SYNTAX_ERROR;
                goto cleanup_exit;
              }
              r = snippet_append_tkr(&dir_snippet, tkr_tokens);
              if (r) {
                r = TKR_INTERNAL_ERROR;
                goto cleanup_exit;
              }
            }
          }
          else if (directive == PCD_TOKEN_ACTION_DIRECTIVE) {
            if (dir_snippet.num_tokens_ || (tkr_tokens->best_match_action_ != TOK_WHITESPACE)) {
              if ((tkr_tokens->best_match_variant_ == TOK_SPECIAL_IDENT) &&
                  (tkr_tokens->best_match_action_ != TOK_SPECIAL_IDENT_DST)) {
                re_error_tkr(tkr_tokens, "Error: \"%s\" not allowed, a token action may only have a single \"$$\" special identifier as the object to construct", tkr_str(tkr_tokens));
                r = TKR_SYNTAX_ERROR;
                goto cleanup_exit;
              }
              r = snippet_append_tkr(&dir_snippet, tkr_tokens);
              if (r) {
                r = TKR_INTERNAL_ERROR;
                goto cleanup_exit;
              }
            }
          }
        }
      }
    }

    r = tkr_tokenizer_inputx(tkr_tokens, directive_line_match, 1);
  }

  assert(r != TKR_FEED_ME); /* should not ask to get fed on final input */

  if ((directive == PCD_NT_TYPE_DIRECTIVE) ||
      (directive == PCD_CONSTRUCTOR_DIRECTIVE) ||
      (directive == PCD_DESTRUCTOR_DIRECTIVE) ||
      (directive == PCD_TOKEN_ACTION_DIRECTIVE)) {
    /* Trim simple whitespace off the tail end */
    while (dir_snippet.num_tokens_ && 
           ((dir_snippet.tokens_[dir_snippet.num_tokens_ - 1].match_ == TOK_WHITESPACE) ||
            (dir_snippet.tokens_[dir_snippet.num_tokens_ - 1].variant_ == TOK_SPECIAL_IDENT))) {
      snippet_pop_last_token(&dir_snippet);
    }
  }

  if (directive == PCD_NT_TYPE_DIRECTIVE) {
    size_t n;
    int is_new;
    struct typestr *nt_ts = typestr_find_or_add(&cc->tstab_, &dir_snippet, &is_new);
    if (!nt_ts) {
      r = EXIT_FAILURE;
      goto cleanup_exit;
    }
    cc->most_recent_typestr_ = nt_ts;
    for (n = 0; n < num_typed_symbols; ++n) {
      struct symbol *sym = typed_symbols[n];
      snippet_clear(&sym->type_snippet_);
      r = snippet_append_snippet(&sym->type_snippet_, &dir_snippet);
      if (r) goto cleanup_exit;
      sym->assigned_type_ = nt_ts;
    }
  }

  if (directive == PCD_TOKEN_TYPE_DIRECTIVE) {
    /* Trim simple whitespace off the tail end */
    while (cc->token_type_.num_tokens_ && 
          ((cc->token_type_.tokens_[cc->token_type_.num_tokens_ - 1].match_ == TOK_WHITESPACE) ||
           (cc->token_type_.tokens_[cc->token_type_.num_tokens_ - 1].variant_ == TOK_SPECIAL_IDENT))) {
      snippet_pop_last_token(&cc->token_type_);
    }
    int is_new;
    struct typestr *token_ts = typestr_find_or_add(&cc->tstab_, &cc->token_type_, &is_new);
    if (!token_ts) {
      r = EXIT_FAILURE;
      goto cleanup_exit;
    }
    cc->token_assigned_type_ = token_ts;
    cc->most_recent_typestr_ = token_ts;
  }

  if (directive == PCD_CONSTRUCTOR_DIRECTIVE) {
    snippet_clear(&cc->most_recent_typestr_->constructor_snippet_);
    r = snippet_append_snippet(&cc->most_recent_typestr_->constructor_snippet_, &dir_snippet);
    if (r) goto cleanup_exit;
  }

  if (directive == PCD_DESTRUCTOR_DIRECTIVE) {
    snippet_clear(&cc->most_recent_typestr_->destructor_snippet_);
    r = snippet_append_snippet(&cc->most_recent_typestr_->destructor_snippet_, &dir_snippet);
    if (r) goto cleanup_exit;
  }

  if (directive == PCD_TOKEN_ACTION_DIRECTIVE) {
    snippet_clear(&cc->token_action_snippet_);
    r = snippet_append_snippet(&cc->token_action_snippet_, &dir_snippet);
    if (r) goto cleanup_exit;
  }

  r = 0;

cleanup_exit:
  snippet_cleanup(&dir_snippet);
  if (typed_symbols) free(typed_symbols);

  return r;
}

static int process_tokens(struct tkr_tokenizer *tkr_tokens, struct xlts *input_line, int is_final, struct prd_stack *prds, struct symbol_table *st) {
  int r;
  struct xlts empty;
  xlts_init(&empty);
  if (is_final) input_line = &empty;
  r = tkr_tokenizer_inputx(tkr_tokens, input_line, is_final);
  while ((r != TKR_END_OF_INPUT) && (r != TKR_FEED_ME)) {
    if (r == TKR_SYNTAX_ERROR) {
      if (isprint(tkr_str(tkr_tokens)[0])) {
        re_error_tkr(tkr_tokens, "Syntax error character \"%s\" not expected", tkr_str(tkr_tokens));
      }
      else {
        re_error_tkr(tkr_tokens, "Syntax error character 0x%02x not expected", tkr_str(tkr_tokens)[0]);
      }
    }
    else if (r == TKR_INTERNAL_ERROR) {
      return r;
    }
    else if (r == TKR_MATCH) {
      r = prd_parse(prds, tkr_tokens, 0, st);
      switch (r) {
      case PRD_SUCCESS:
        /* This should not be possible without is_final==1 */
        re_error_tkr(tkr_tokens, "Internal error, premature end of grammar");
        return TKR_INTERNAL_ERROR;
        break;
      case PRD_SYNTAX_ERROR:
        /* XXX: Need to recover, but how! (Transition on all parent Error syms and attempt to shift input) */
        break;
      case PRD_NEXT:
        break;
      case PRD_INTERNAL_ERROR:
        return TKR_INTERNAL_ERROR;
      }
    }

    r = tkr_tokenizer_inputx(tkr_tokens, input_line, is_final);
  }

  if (r == TKR_END_OF_INPUT) {
    r = prd_parse(prds, tkr_tokens, 1, st);
    switch (r) {
    case PRD_SUCCESS:
      return TKR_END_OF_INPUT;
      break;
    case PRD_SYNTAX_ERROR:
      /* Not attempting to recover because there is no more input to synchronize with, end here. */
      return TKR_END_OF_INPUT;
    case PRD_NEXT:
      /* Grammar should not exit regularly awaiting next token when the next token is INPUT_END.
       * (INPUT_END, i.e. end_of_input==1, never shifts) */
      re_error_tkr(tkr_tokens, "Internal error, grammar expected to end");
      return TKR_INTERNAL_ERROR;
    case PRD_INTERNAL_ERROR:
      return TKR_INTERNAL_ERROR;
    }
  }

  return r;
}

int main(int argc, char **argv) {
  int r;
  klt_logger_init();
  klt_log_set_function("/", log_function, NULL);

  LOG("We've started..\n");

  r = prd_init();
  if (r) {
    LOGERROR("Failed to initialize prd\n");
    return EXIT_FAILURE;
  }
  r = ldl_init();
  if (r) {
    LOGERROR("Failed to initialize ldl\n");
    return EXIT_FAILURE;
  }
  r = tok_init();
  if (r) {
    LOGERROR("Failed to initialize tok\n");
    return EXIT_FAILURE;
  }
  r = las_init();
  if (r) {
    LOGERROR("Failed to initialize las\n");
    return EXIT_FAILURE;
  }
  r = dct_init();
  if (r) {
    LOGERROR("Failed to initialize dct\n");
    return EXIT_FAILURE;
  }

  struct las_line_assembly line_assembly;
  las_init_line_assembly(&line_assembly);

  struct tkr_tokenizer tkr_lines;
  ldl_init_tokenizer(&tkr_lines);

  struct tkr_tokenizer tkr_tokens;
  tok_init_tkr_tokenizer(&tkr_tokens);

  struct cinder_context cc;
  cinder_context_init(&cc);

  struct prd_stack prds;
  prd_stack_init(&prds);

  struct grammar_table gt;
  gt_grammar_table_init(&gt);

  lr_generator_t lalr;
  lr_init(&lalr);

  struct xlts token_buf;
  xlts_init(&token_buf);

  struct xlts comment_free_line;
  xlts_init(&comment_free_line);

  int *state_syms = NULL;

  r = prd_reset(&prds);
  if (r) {
    LOGERROR("Internal error, failed to reset parsing stack\n");
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  //const char *input_filename = "test_input.cnd";
  const char *input_filename = "prd_grammar.cnd";
  FILE *fp = fopen(input_filename, "rb");
  if (!fp) {
    LOGERROR("Failed to open file \"%s\"\n", input_filename);
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }
  las_set_filename(&line_assembly, input_filename);

  struct part *prologue = NULL;
  struct part *epilogue = NULL;

  enum {
    PROLOGUE,
    GRAMMAR,
    EPILOGUE
  } where_are_we = PROLOGUE;

  size_t num_bytes_read;
  static char buf[2400];

  int have_error = 0;

  do {
    num_bytes_read = fread(buf, sizeof(*buf), sizeof(buf) / sizeof(*buf), fp);

    r = las_input(&line_assembly, buf, num_bytes_read, !num_bytes_read);
    while ((r != LAS_END_OF_INPUT) && (r != LAS_FEED_ME)) {
      /* Cannot modify (shift) the line_assembly's mlc_buf line buffer, so copy it over to a work area (token_buf) */
      xlts_reset(&token_buf);
      r = xlts_append(&token_buf, &line_assembly.mlc_buf_);
      if (r) {
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }

      xlts_reset(&comment_free_line);
      xlts_append(&comment_free_line, &token_buf);
      dct_decomment(&comment_free_line);

      r = tkr_tokenizer_inputx(&tkr_lines, &comment_free_line, 1);

      /* Empty line cannot be matched as a match requires at least a single byte to be processed to force progress;
       * manually override that case to match LD_REGULAR */
      if ((r == TKR_END_OF_INPUT) && (comment_free_line.num_translated_ == 0)) {
        r = TKR_MATCH;
        tkr_lines.best_match_action_ = LD_REGULAR;
        tkr_lines.best_match_variant_ = LD_REGULAR;
      }

      if ((r == TKR_END_OF_INPUT) || (r == TKR_FEED_ME)) {
        LOGERROR("%s(%d): Internal error: all lines are expected to match.\n", line_assembly.lc_tkr_.filename_, line_assembly.lc_tkr_.input_line_);
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }
      if (r == TKR_SYNTAX_ERROR) {
        if (isprint(tkr_str(&tkr_lines)[0])) {
          re_error_tkr(&tkr_lines, "Syntax error character \"%s\" not expected", tkr_str(&tkr_lines));
          have_error = 1;
        }
        else {
          re_error_tkr(&tkr_lines, "Syntax error character 0x%02x not expected", tkr_str(&tkr_lines));
          have_error = 1;
        }
      }
      else if (r == TKR_INTERNAL_ERROR) {
        num_bytes_read = 0;
        break;
      }

      if (r == TKR_MATCH) {
        if (where_are_we == PROLOGUE) {
          switch (tkr_lines.best_match_variant_) {
          case LD_C_PREPROCESSOR:
            /* Preserve line continuations */
            append_part(&prologue, token_buf.num_original_, token_buf.original_);
            break;
          case LD_CINDER_SECTION_DELIMITER:
            where_are_we = GRAMMAR;
            break;
          case LD_REGULAR:
            /* XXX: Process this by individual tokens later ? */
            /* Preserve line continuations */
            append_part(&prologue, token_buf.num_original_, token_buf.original_);
            break;
          case LD_CINDER_DIRECTIVE:
            r = process_cinder_directive(&tkr_tokens, &token_buf, &cc);
            if (r == TKR_INTERNAL_ERROR) {
              r = EXIT_FAILURE;
              goto cleanup_exit;
            }
            else if (r) {
              have_error = 1;
            }
            break;
          }
        }
        else if (where_are_we == GRAMMAR) {
          switch (tkr_lines.best_match_variant_) {
          case LD_C_PREPROCESSOR:
            re_error_tkr(&tkr_lines, "Error, preprocessor directives do not belong in grammar area");
            have_error = 1;
            break;
          case LD_CINDER_SECTION_DELIMITER:
            /* Finish up */
            r = process_tokens(&tkr_tokens, &token_buf, 1, &prds, &cc.symtab_);

            where_are_we = EPILOGUE;
            break;
          case LD_REGULAR:
          {
            r = process_tokens(&tkr_tokens, &token_buf, 0, &prds, &cc.symtab_);
            break;
          }
          case LD_CINDER_DIRECTIVE:
            r = process_cinder_directive(&tkr_tokens, &token_buf, &cc);
            if (r == TKR_INTERNAL_ERROR) {
              r = EXIT_FAILURE;
              goto cleanup_exit;
            }
            else if (r) {
              have_error = 1;
            }
            break;
          }
        }
        else /* (where_are_we == EPILOGUE) */ {
          switch (tkr_lines.best_match_variant_) {
          case LD_C_PREPROCESSOR:
            append_part(&epilogue, token_buf.num_original_, token_buf.original_);
            break;
          case LD_CINDER_SECTION_DELIMITER:
            /* Going back to the grammar, append the epilogue we gathered to the prologue as it is
             * actually inline code. */
          {
            struct part *prologue_head, *prologue_tail, *epilogue_head, *epilogue_tail;
            prologue_tail = prologue;
            prologue_head = prologue ? prologue->next_ : NULL;
            epilogue_tail = epilogue;
            epilogue_head = epilogue ? epilogue->next_ : NULL;
            if (epilogue_head) {
              if (!prologue_head) {
                prologue = epilogue_head;
                epilogue = NULL;
              }
              else {
                epilogue_tail->next_ = prologue_head;
                prologue_tail->next_ = epilogue_head;
                prologue = epilogue_tail;
                epilogue = NULL;
              }
            }
          }
          where_are_we = GRAMMAR;
          break;
          case LD_REGULAR:
            /* Preserve line continuations */
            append_part(&epilogue, token_buf.num_original_, token_buf.original_);
            break;
          case LD_CINDER_DIRECTIVE:
            r = process_cinder_directive(&tkr_tokens, &token_buf, &cc);
            if (r == TKR_INTERNAL_ERROR) {
              r = EXIT_FAILURE;
              goto cleanup_exit;
            }
            else if (r) {
              have_error = 1;
            }
            break;
          }
        }
      }

      r = tkr_tokenizer_inputx(&tkr_lines, &comment_free_line, 1);
      //r = tkr_tokenizer_input(&tkr_lines, token_buf.translated_, token_buf.num_translated_, 1);
      if ((r != TKR_END_OF_INPUT) && (r != TKR_INTERNAL_ERROR)) {
        re_error(&comment_free_line, "Internal error: lines from a single line are expected to match a single time");
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }

      r = las_input(&line_assembly, buf, num_bytes_read, !num_bytes_read);
    }

  } while (num_bytes_read);

  /* Finished parsing */
  if (prds.have_errors_) {
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  /* Assign types to all symbols */
  struct symbol *sym;
  struct typestr *token_ts = NULL;
  if (cc.token_type_.num_tokens_) {
    int is_new;
    token_ts = typestr_find_or_add(&cc.tstab_, &cc.token_type_, &is_new);
    if (!token_ts) {
      r = EXIT_FAILURE;
      goto cleanup_exit;
    }
  }

  sym = cc.symtab_.terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      if (!sym->type_snippet_.num_tokens_) {
        sym->assigned_type_ = token_ts;
      }
      else {
        int is_new;
        struct typestr *ts = typestr_find_or_add(&cc.tstab_, &sym->type_snippet_, &is_new);
        if (!ts) {
          r = EXIT_FAILURE;
          goto cleanup_exit;
        }
        sym->assigned_type_ = ts;
      }
    } while (sym != cc.symtab_.terminals_);
  }
  sym = cc.symtab_.non_terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      if (!sym->type_snippet_.num_tokens_) {
        sym->assigned_type_ = NULL;
      }
      else {
        int is_new;
        struct typestr *ts = typestr_find_or_add(&cc.tstab_, &sym->type_snippet_, &is_new);
        if (!ts) {
          r = EXIT_FAILURE;
          goto cleanup_exit;
        }
        sym->assigned_type_ = ts;
      }

    } while (sym != cc.symtab_.non_terminals_);
  }


  LOG("Parsing completed successfully\n");
  
  /* Number all symbols */
  int NT_END = 0, RULE_END = 1, GRAMMAR_END = 2;
  int next_ordinal = 3;
  sym = cc.symtab_.terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      sym->ordinal_ = next_ordinal++;
    } while (sym != cc.symtab_.terminals_);
  }
  int INPUT_END = next_ordinal++;
  sym = cc.symtab_.non_terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      sym->ordinal_ = next_ordinal++;
    } while (sym != cc.symtab_.non_terminals_);
  }
  int SYNTHETIC_S = next_ordinal++;

  r = gt_transcribe_grammar(&gt, prds.num_productions_, prds.productions_, RULE_END, GRAMMAR_END);
  if (r) {
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  r = gt_generate_lalr(&gt, &lalr, RULE_END, GRAMMAR_END, INPUT_END, SYNTHETIC_S);
  if (r) {
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  const char *output_filename = "prd_gram_gen.c";
  FILE *outfp;
  outfp = fopen(output_filename, "wb");
  if (!outfp) {
    int err = errno;
    LOGERROR("Failed to open file \"%s\" for writing: %s\n", output_filename, strerror(err));
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  struct part *pt;
  pt = prologue;
  if (pt) {
    do {
      pt = pt->next_;

      size_t written = fwrite(pt->chars_, 1, pt->num_chars_, outfp);
      if (written != pt->num_chars_) {
        int err = errno;
        LOGERROR("Failed to write to \"%s\": %s\n", output_filename, strerror(err));
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }

    } while (pt != prologue);
  }

  fprintf(outfp, "/* --------- HERE GOES THE GENERATED FLUFF ------------ */\n");

  fprintf(outfp, "union prd_sym_data_union {\n");
  size_t ts_idx;
  for (ts_idx = 0; ts_idx < cc.tstab_.num_typestrs_; ++ts_idx) {
    struct typestr *ts;
    ts = cc.tstab_.typestrs_[ts_idx];
    int found_placeholder = 0;

    fprintf(outfp, " ");
    size_t tok_idx;
    for (tok_idx = 0; tok_idx < ts->typestr_snippet_.num_tokens_; ++tok_idx) {
      struct snippet_token *st = ts->typestr_snippet_.tokens_ + tok_idx;
      if (st->variant_ != TOK_SPECIAL_IDENT) {
        fprintf(outfp, "%s%s", tok_idx ? " " : "", st->text_.translated_);
      }
      else {
        found_placeholder = 1;
        fprintf(outfp, " uv%d_", ts->ordinal_);
      }
    }
    if (!found_placeholder) {
      /* Placeholder is implied at the end */
      fprintf(outfp, " uv%d_", ts->ordinal_);
    }
    fprintf(outfp, ";\n");
  }
  fprintf(outfp, "};\n");


  size_t num_columns = (size_t)(1 + lalr.max_sym - lalr.min_sym);
  fprintf(outfp, "static int minimum_sym = %d;\n", lalr.min_sym);
  fprintf(outfp, "static size_t num_columns = %zu;\n", num_columns);
  fprintf(outfp, "static size_t num_rows = %zu;\n", (size_t)lalr.nr_states);
  fprintf(outfp, "static size_t num_productions = %zu;\n", lalr.nr_productions);
  fprintf(outfp, "static int parse_table[] = {\n");
  size_t row, col;
  char *column_widths = (char *)malloc(num_columns);
  if (!column_widths) {
    LOGERROR("Error, no memory\n");
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }

  for (col = 0; col < num_columns; ++col) {
    column_widths[col] = 1;
    for (row = 0; row < lalr.nr_states; ++row) {
      int action = lalr.parse_table[row * num_columns + col];
      int width_needed = 1;
      if (action <= -1000) {
        width_needed = 5;
      }
      else if (action <= -100) {
        width_needed = 4;
      }
      else if (action <= -10) {
        width_needed = 3;
      }
      else if (action < 100) {
        width_needed = 2;
      }
      else if (action < 1000) {
        width_needed = 3;
      }
      else if (action < 10000) {
        width_needed = 4;
      }
      else {
        width_needed = 5;
      }
      if (width_needed > column_widths[col]) {
        column_widths[col] = width_needed;
      }
    }
  }
  for (row = 0; row < lalr.nr_states; ++row) {
    for (col = 0; col < num_columns; ++col) {
      int action = lalr.parse_table[row * num_columns + col];

      fprintf(outfp, "%*d,%s", column_widths[col], action, col == (num_columns - 1) ? "\n" : "");
    }
  }
  free(column_widths);
  fprintf(outfp, "};\n");
  fprintf(outfp, "static size_t production_lengths[] = {\n");
  for (row = 0; row < lalr.nr_productions; ++row) {
    fprintf(outfp, " %d%s\n", lalr.production_lengths[row], (row == lalr.nr_productions - 1) ? "" : ",");
  }
  fprintf(outfp, "};\n");
  fprintf(outfp, "static int production_syms[] = {\n");
  for (row = 0; row < lalr.nr_productions; ++row) {
    fprintf(outfp, " %d%s\n", lalr.productions[row][0], (row == lalr.nr_productions - 1) ? "" : ",");
  }
  fprintf(outfp, "};\n");

  /* For each state, what is the top-most symbol on the stack? */
  fprintf(outfp, "static int state_syms[] = {\n");

  state_syms = (int *)malloc(sizeof(int) * (size_t)lalr.nr_states);
  if (!state_syms) {
    LOGERROR("Error, no memory\n");
    r = EXIT_FAILURE;
    goto cleanup_exit;
  }
  for (row = 0; row < lalr.nr_states; ++row) {
    state_syms[row] = -1;
  }
  for (row = 0; row < lalr.nr_states; ++row) {
    for (col = 0; col < num_columns; ++col) {
      int action = lalr.parse_table[row * num_columns + col];
      if (action > 0) {
        /* We're shifting to a destination state. */
        int sym_shifting = ((int)col) + lalr.min_sym;
        int state_shifting_to = action;
        if (state_syms[state_shifting_to] != sym_shifting) {
          if (state_syms[state_shifting_to] == -1) {
            state_syms[state_shifting_to] = sym_shifting;
          }
          else {
            LOGERROR("Inconsistent state entry: each state should be entered by 1 unique symbol\n");
            free(state_syms);
            r = EXIT_FAILURE;
            goto cleanup_exit;
          }
        }
      }
    }
  }
  for (row = 0; row < lalr.nr_states; ++row) {
    fprintf(outfp, " %d%s\n", state_syms[row], (row == lalr.nr_states - 1) ? "" : ",");
  }
  fprintf(outfp, "};\n");

  const char *symbol_prefix = "PRD_";
  fprintf(outfp, "#define NT_END %d\n", NT_END);
  fprintf(outfp, "#define RULE_END %d\n", RULE_END);
  fprintf(outfp, "#define GRAMMAR_END %d\n", GRAMMAR_END);
  sym = cc.symtab_.terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      char *ident = malloc(1 + sym->def_.num_translated_);
      char *s = ident;
      const char *p;
      if (!ident) {
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }
      /* Transform into C identifier */
      for (p = sym->def_.translated_; p < (sym->def_.translated_ + sym->def_.num_translated_); ++p) {
        int c = *p;
        if ((c >= 'a') && (c <= 'z')) c = c - 'a' + 'A';
        if (c == '-') c = '_';
        *s++ = c;
      }
      *s++ = '\0';

      fprintf(outfp, "#define %s%s %d\n", symbol_prefix, ident, sym->ordinal_);
      free(ident);
    } while (sym != cc.symtab_.terminals_);
  }
  fprintf(outfp, "\n");
  fprintf(outfp, "#define INPUT_END %d\n", INPUT_END);
  sym = cc.symtab_.non_terminals_;
  if (sym) {
    do {
      sym = sym->next_;

      char *ident = malloc(1 + sym->def_.num_translated_);
      char *s = ident;
      const char *p;
      if (!ident) {
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }
      /* Transform into C identifier */
      for (p = sym->def_.translated_; p < (sym->def_.translated_ + sym->def_.num_translated_); ++p) {
        int c = *p;
        if ((c >= 'a') && (c <= 'z')) c = c - 'a' + 'A';
        if (c == '-') c = '_';
        *s++ = c;
      }
      *s++ = '\0';

      fprintf(outfp, "#define %s%s %d\n", symbol_prefix, ident, sym->ordinal_);
      free(ident);
    } while (sym != cc.symtab_.non_terminals_);
  }
  fprintf(outfp, "#define SYNTHETIC_S %d\n", SYNTHETIC_S);


  fprintf(outfp, "static int reduce(struct prd_stack *stack, struct tkr_tokenizer *tkr, int production, struct prd_sym_data *dst_sym, union prd_sym_data_union *dst_sym_data, struct prd_sym_data *syms, union prd_sym_data_union *sym_data, struct symbol_table *st) {\n");
  fprintf(outfp, "  int r;\n"
                 "  struct prd_production *pd;\n"
                 "  struct symbol *sym;\n"
                 "  switch (production) {\n");
  for (row = 0; row < prds.num_productions_; ++row) {
    struct prd_production *pd = prds.productions_ + row;
    fprintf(outfp, "  case %d: {\n    ", (int)row + 1);
    /* Emit dst_sym_data constructor first */
    if (pd->nt_.sym_->assigned_type_ && pd->nt_.sym_->assigned_type_->constructor_snippet_.num_tokens_) {
      struct snippet *constructor = &pd->nt_.sym_->assigned_type_->constructor_snippet_;
      for (col = 0; col < constructor->num_tokens_; ++col) {
        if (constructor->tokens_[col].match_ == TOK_SPECIAL_IDENT_DST) {
          /* Insert destination sym at appropriate ordinal value type. */
          fprintf(outfp, "(dst_sym_data->uv%d_)", pd->nt_.sym_->assigned_type_->ordinal_);
        }
        else {
          /* Regular token, just emit as-is */
          fprintf(outfp, constructor->tokens_[col].text_.original_);
        }
      }
      /* Close this compound block and open the one for the action */
      fprintf(outfp, "\n  }\n  {\n  ");
    }

    for (col = 0; col < pd->action_sequence_.num_tokens_; ++col) {
      /* Print the original code, to preserve formatting and line continuations */
      if (pd->action_sequence_.tokens_[col].match_ == TOK_SPECIAL_IDENT_DST) {
        /* Expansion of special destination sym identifier */
        /* Destination sym -- do we have a datatype for it ? */
        struct symbol *dst_sym = pd->nt_.sym_;
        assert(dst_sym);
        if (!dst_sym->assigned_type_) {
          re_error(&pd->action_sequence_.tokens_[col].text_, "$$ cannot resolve to a data type for non-terminal %s\n", pd->nt_.id_.translated_);
          r = EXIT_FAILURE;
          goto cleanup_exit;
        }
        fprintf(outfp, "(dst_sym_data->uv%d_)", dst_sym->assigned_type_->ordinal_);
      }
      else if (pd->action_sequence_.tokens_[col].match_ == TOK_SPECIAL_IDENT_STR) {
        /* Expansion of another sym identifier */
        size_t n;
        int failed = 0;
        size_t special_index = 0;
        for (n = 1; n < pd->action_sequence_.tokens_[col].text_.num_translated_; ++n) {
          char c = pd->action_sequence_.tokens_[col].text_.translated_[n];
          if (!isdigit(c)) {
            re_error(&pd->action_sequence_.tokens_[col].text_, "Unrecognized special identifier\n");
            failed = 1;
            break;
          }
          if (mul_size_t(10, special_index, NULL, &special_index)) {
            re_error(&pd->action_sequence_.tokens_[col].text_, "Overflow on symbol index\n");
            failed = 1;
            break;
          }
        }
        if (special_index >= pd->num_syms_) {
          re_error(&pd->action_sequence_.tokens_[col].text_, "Symbol index exceeds number of symbols in production\n");
          failed = 1;
        }
        if (failed) {
          r = EXIT_FAILURE;
          goto cleanup_exit;
        }
        struct symbol *sym = pd->syms_[special_index].sym_;
        if (!sym->assigned_type_) {
          re_error(&pd->action_sequence_.tokens_[col].text_, "%s cannot resolve to a data type for a symbol\n", pd->action_sequence_.tokens_[col].text_.translated_);
          r = EXIT_FAILURE;
          goto cleanup_exit;
        }
        fprintf(outfp, "(sym_data[%zu].uv%d_)", special_index, sym->assigned_type_->ordinal_);
      }
      else {
        fprintf(outfp, pd->action_sequence_.tokens_[col].text_.original_);
      }
    }
    fprintf(outfp, "\n"
                    "  }\n"
                    "  break;\n");
  }
  fprintf(outfp, "  } /* switch */\n"
                 "  return PRD_SUCCESS;\n"
                 "}\n");

  /* Emit the parse function */
  fprintf(outfp,
    "static int parse(struct prd_stack *stack, int sym, struct tkr_tokenizer *tkr, int end_of_input, struct symbol_table *st) {\n"
    "  int current_state = stack->states_[stack->pos_ - 1];\n"
    "  int action = parse_table[num_columns * current_state + (sym - minimum_sym)];\n"
    "  if (!action) {\n"
    "    /* Syntax error */\n"
    "    if (sym != INPUT_END) {\n"
    "      re_error_tkr(tkr, \"Syntax error \\\"%%s\\\" not expected\", tkr->xmatch_.translated_);\n"
    "    }\n"
    "    else {\n"
    "      re_error_tkr(tkr, \"Syntax error end of input not expected\");\n"
    "    }\n"
    "    /* XXX: Pop until we transition */\n"
    "    return PRD_SYNTAX_ERROR;\n"
    "  }\n"
    "  while (action < 0) {\n"
    "    /* While we're reducing.. */\n"
    "    int production = -action - 1;\n"
    "    size_t production_length = production_lengths[production];\n"
    "    int nonterminal = production_syms[production];\n"
    "\n"
    "    if (0 == production) {\n"
    "      /* Synth S we're done. */\n"
    "      return PRD_SUCCESS;\n"
    "    }\n"
    "\n"
    "    struct prd_sym_data nonterminal_data_reduced_to;\n"
    "    union prd_sym_data_union nonterminal_sym_data_reduced_to;\n"
    "    prd_sym_data_init(&nonterminal_data_reduced_to);\n"
    "    int r;\n"
    "    r = reduce(stack, tkr, production, &nonterminal_data_reduced_to, &nonterminal_sym_data_reduced_to, stack->syms_ + stack->pos_ - production_length, stack->sym_data_ + stack->pos_ - production_length, st);\n"
    "    if (r) {\n"
    "      /* Semantic error */\n"
    "      return r;\n"
    "    }\n"
    "\n"
    "    /* Free symdata for every symbol in the production, including the first slot where we will soon\n"
    "     * push nonterminal_data_reduced_to */\n"
    "    size_t prd_sym_idx;\n"
    "    for (prd_sym_idx = stack->pos_ - production_length; prd_sym_idx < stack->pos_; ++prd_sym_idx) {\n"
    "      prd_sym_data_cleanup(stack->syms_ + prd_sym_idx);\n");

  fprintf(outfp,
    "      switch (stack->states_[prd_sym_idx]) {\n"
  );
  size_t typestr_idx;
  for (typestr_idx = 0; typestr_idx < cc.tstab_.num_typestrs_; ++typestr_idx) {
    struct typestr *ts = cc.tstab_.typestrs_[typestr_idx];
    if (ts->destructor_snippet_.num_tokens_) {
      int have_cases = 0; /* always true if all types are always used */
      /* Type has a destructor associated.. Find all state for whose corresponding symbol has the associated type */
      size_t state_idx;
      for (state_idx = 0; state_idx < lalr.nr_states; ++state_idx) {
        struct symbol *sym = symbol_find_by_ordinal(&cc.symtab_, state_syms[state_idx]);
        if (!sym) continue;
        if (sym->assigned_type_ == ts) {
          fprintf(outfp, "      case %d: /* %s */\n", (int)state_idx, sym->def_.translated_);
          have_cases = 1;
        }
      }
      if (have_cases) {
        fprintf(outfp, "      {\n      ");
        struct snippet *action = &ts->destructor_snippet_;
        for (col = 0; col < action->num_tokens_; ++col) {
          if (action->tokens_[col].match_ == TOK_SPECIAL_IDENT_DST) {
            /* Insert destination sym at appropriate ordinal value type. */
            fprintf(outfp, "((stack->sym_data_ + prd_sym_idx)->uv%d_)", ts->ordinal_);
          }
          else {
            /* Regular token, just emit as-is */
            fprintf(outfp, action->tokens_[col].text_.translated_);
          }
        }
        /* Close this compound block  */
        fprintf(outfp, "\n      }\n      break;\n");
      }
    }
  }
  /* Enumerate all states, for each state, determine the type corresponding to the state from the symbol corresponding to the state. */
  fprintf(outfp,
    "      }\n");

  fprintf(outfp,
    "    }\n"
    "\n"
    "    stack->pos_ -= production_length;\n"
    "\n"
    "    current_state = stack->states_[stack->pos_ - 1];\n"
    "    action = parse_table[num_columns * current_state + (nonterminal - minimum_sym)];\n"
    "\n"
    "    if (!action) {\n"
    "      re_error_tkr(tkr, \"Internal error \\\"%%s\\\" cannot shift an already reduced nonterminal\", tkr->xmatch_.translated_);\n"
    "      return PRD_INTERNAL_ERROR;\n"
    "    }\n"
    "    if (action < 0) {\n"
    "      re_error_tkr(tkr, \"Internal error \\\"%%s\\\" reduced non-terminal not shifting\", tkr->xmatch_.translated_);\n"
    "      return PRD_INTERNAL_ERROR;\n"
    "    }\n"
    "    push_state(stack, action /* action for a shift is the ordinal */);\n"
    "    struct prd_sym_data *sd = stack->syms_ + stack->pos_ - 1;\n"
    "    *sd = nonterminal_data_reduced_to;\n"
    "    union prd_sym_data_union *sdu = stack->sym_data_ + stack->pos_ - 1;\n"
    "    *sdu = nonterminal_sym_data_reduced_to;\n"
  "\n"
    "    current_state = stack->states_[stack->pos_ - 1];\n"
    "    action = parse_table[num_columns * current_state + (sym - minimum_sym)];\n"
    "    if (!action) {\n"
    "      /* Syntax error */\n"
    "      if (sym != INPUT_END) {\n"
    "        re_error_tkr(tkr, \"Syntax error \\\"%%s\\\" not expected\", tkr->xmatch_.translated_);\n"
    "      }\n"
    "      else {\n"
    "        re_error_tkr(tkr, \"Syntax error end of input not expected\");\n"
    "      }\n"
    "      return PRD_SYNTAX_ERROR;\n"
    "    }\n"
    "  }\n"
    "\n"
    "  /* Shift token onto stack */\n"
    "  if (action > 0 /* shift? */) {\n"
    "    push_state(stack, action /* action for a shift is the ordinal */);\n"
    "    struct prd_sym_data *sym = stack->syms_ + stack->pos_ - 1;\n"
    "\n"
    "    /* Fill in the sym from the tokenizer */\n");
  int need_sym_data = 0;
  if (cc.token_assigned_type_ && cc.token_assigned_type_->constructor_snippet_.num_tokens_) {
    need_sym_data = 1;
  }
  if (cc.token_action_snippet_.num_tokens_) {
    need_sym_data = 1;
  }
  if (need_sym_data) {
    fprintf(outfp, "    union prd_sym_data_union *sym_data = stack->sym_data_ + stack->pos_ - 1;\n");
  }
  if (cc.token_assigned_type_ && cc.token_assigned_type_->constructor_snippet_.num_tokens_) {
    fprintf(outfp, "    {\n      ");
    struct snippet *constructor = &cc.token_assigned_type_->constructor_snippet_;
    for (col = 0; col < constructor->num_tokens_; ++col) {
      if (constructor->tokens_[col].match_ == TOK_SPECIAL_IDENT_DST) {
        /* Insert destination sym at appropriate ordinal value type. */
        fprintf(outfp, "(sym_data->uv%d_)", cc.token_assigned_type_->ordinal_);
      }
      else {
        /* Regular token, just emit as-is */
        fprintf(outfp, constructor->tokens_[col].text_.translated_);
      }
    }
    /* Close this compound block and open the one for the action */
    fprintf(outfp, "\n    }\n");
  }
  if (cc.token_action_snippet_.num_tokens_) {
    fprintf(outfp, "    {\n      ");
    struct snippet *action = &cc.token_action_snippet_;
    for (col = 0; col < action->num_tokens_; ++col) {
      if (action->tokens_[col].match_ == TOK_SPECIAL_IDENT_DST) {
        /* Insert destination sym at appropriate ordinal value type. */
        fprintf(outfp, "(sym_data->uv%d_)", cc.token_assigned_type_->ordinal_);
      }
      else {
        /* Regular token, just emit as-is */
        fprintf(outfp, action->tokens_[col].text_.translated_);
      }
    }
    /* Close this compound block and open the one for the action */
    fprintf(outfp, "\n    }\n");
  }

  fprintf(outfp,
    "    prd_sym_data_init(sym);\n"
    "    xlts_append(&sym->text_, &tkr->xmatch_);\n"
    "    sym->match_ = tkr->best_match_action_;\n"
    "    sym->variant_ = tkr->best_match_variant_;\n"
    "  }\n"
    "  else {\n"
    "    re_error_tkr(tkr, \"Syntax error \\\"%%s\\\" not expected\", tkr->xmatch_.translated_);\n"
    "    return PRD_SYNTAX_ERROR;\n"
    "  }\n"
    "\n"
    "  return PRD_NEXT;\n"
    "}\n"
  );


  fprintf(outfp, "/* --------- HERE ENDS THE GENERATED FLUFF ------------ */\n");


  pt = epilogue;
  if (pt) {
    do {
      pt = pt->next_;

      size_t written = fwrite(pt->chars_, 1, pt->num_chars_, outfp);
      if (written != pt->num_chars_) {
        int err = errno;
        LOGERROR("Failed to write to \"%s\": %s\n", output_filename, strerror(err));
        r = EXIT_FAILURE;
        goto cleanup_exit;
      }

    } while (pt != epilogue);
  }
  
  fclose(outfp);

  r = EXIT_SUCCESS;
cleanup_exit:
  if (state_syms) free(state_syms);

  xlts_cleanup(&comment_free_line);

  xlts_cleanup(&token_buf);

  lr_cleanup(&lalr);

  gt_grammar_table_cleanup(&gt);

  cinder_context_cleanup(&cc);

  prd_stack_cleanup(&prds);

  tkr_tokenizer_cleanup(&tkr_tokens);

  tkr_tokenizer_cleanup(&tkr_lines);

  las_cleanup_line_assembly(&line_assembly);

  dct_cleanup();
  las_cleanup();
  tok_cleanup();
  ldl_cleanup();
  prd_cleanup();

  LOG("We've finished%s\n", (r == EXIT_SUCCESS) ? "" : " in failure...");

  return r;
}
