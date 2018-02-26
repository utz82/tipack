/*
 * tipack -- program to create TI data files
 *
 * Copyright (C) 2006-2007 Benjamin Moody
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <tifiles.h>
#include <ticonv.h>

#ifndef TI73_APPVAR
# define TI73_APPVAR 0x1a
#endif

#define PACKAGE_STRING "tipack 0.1"

static const char usage[] =
"Usage: %s [OPTIONS] [FILE | -]\n"
"Where OPTIONS may include:\n"
" -o FILE:     output result to FILE\n"
" -n NAME:     set on-calc variable name to NAME\n"
" -t TYPE:     set variable type to TYPE (e.g. 82p)\n"
" -c COMMENT:  set file comment (strftime format string)\n"
" -p:          protect program\n"
" -C:          number/list/matrix is complex\n"
" -a:          send files to archive\n"
" -r:          raw mode (no length bytes)\n"
" -v:          be verbose\n";

static int err_print(const char* func, int errcode)
{
  char* p;
  if (errcode) {
    tifiles_error_get(errcode, &p);
    fprintf(stderr, "error in %s:\n%s\n", func, p);
    free(p);
  }
  return errcode;
}

static int type_name_is_tokenized(int model, int typeid)
{
  if (tifiles_calc_is_ti9x(model))
    return 0;

  if (model == CALC_TI85 || model == CALC_TI86)
    return 0;

  if (model == CALC_TI73)
    return (typeid != TI73_PRGM && typeid != TI73_ASM
	    && typeid != TI73_APPVAR);

  return (typeid != TI82_PRGM && typeid != TI82_PPGM
	  && typeid != TI83p_APPVAR);
}

static int type_has_length(int model, int typeid)
{
  if (!tifiles_calc_is_ti8x(model))
    return 0;

  if (model == CALC_TI85 || model == CALC_TI86) {
    return (typeid == TI85_EQU || typeid == TI85_STRNG
	    || typeid == TI85_PICT || typeid == TI85_PRGM);
  }

  if (model == CALC_TI82 || model == CALC_TI83)
    return (typeid == TI82_YVAR || typeid == TI83_STRNG
	    || typeid == TI82_PRGM || typeid == TI82_PPGM
	    || typeid == TI82_PIC);

  if (model == CALC_TI73)
    return (typeid == TI73_EQU || typeid == TI73_STRNG
	    || typeid == TI73_PRGM || typeid == TI73_ASM
	    || typeid == TI73_PIC || typeid == TI73_APPVAR);

  return (typeid == TI83p_EQU || typeid == TI83p_STRNG
	  || typeid == TI83p_PRGM || typeid == TI83p_ASM
	  || typeid == TI83p_PIC || typeid == TI83p_APPVAR);
}

static int protect_type(int model, int typeid)
{
  return ((tifiles_calc_is_ti8x(model)
	   && model != CALC_TI85
	   && model != CALC_TI86
	   && typeid == TI82_PRGM)
	  ? typeid + 1
	  : typeid);
}

static int complexify_type(int model, int typeid)
{
  if (tifiles_calc_is_ti9x(model))
    return ((typeid == TI89_LIST || typeid == TI89_MAT)
	    ? typeid + 1 
	    : typeid);

  if (model == CALC_TI85 || model == CALC_TI86)
    return ((typeid == TI85_REAL || typeid == TI85_VECTR
	     || typeid == TI85_LIST || typeid == TI85_MATRX
	     || typeid == TI85_CONS)
	    ? typeid + 1
	    : typeid);

  if (model == CALC_TI82 || model == CALC_TI73)
    return typeid;

  return ((typeid == TI83_REAL || typeid == TI83_LIST)
	  ? typeid + TI83_CPLX
	  : typeid);
}

static void log_nop() {}

int main(int argc, char** argv)
{
  char* infilename = NULL;
  char* outfilename = NULL;
  char* varname = NULL;
  char* vartype = NULL;
  char* comment = "Created by " PACKAGE_STRING;
  int protect = 0;
  int complexify = 0;
  int archive = 0;
  int rawmode = 0;
  int verbose = 0;
  int model, typeid;
  FILE* infile;
  FileContent* fc;
  VarEntry* ve;
  char* data = 0;
  unsigned long dsize, dalloc;
  
  int i, j;
  char* p;
  const char* cp;
  time_t t;

  /* Read command-line arguments */

  if (argc == 1) {
    fprintf(stderr, usage, argv[0]);
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1]) {
      for (j = 1; argv[i][j]; j++) {
	switch (argv[i][j]) {
	case 'o':
	  if (argv[i][++j])
	    outfilename = &(argv[i][j]);
	  else
	    outfilename = argv[++i];
	  j = strlen(argv[i]) - 1;
	  break;

	case 'n':
	  if (argv[i][++j])
	    varname = &(argv[i][j]);
	  else
	    varname = argv[++i];
	  j = strlen(argv[i]) - 1;
	  break;

	case 't':
	  if (argv[i][++j])
	    vartype = &(argv[i][j]);
	  else
	    vartype = argv[++i];
	  j = strlen(argv[i]) - 1;
	  break;

	case 'c':
	  if (argv[i][++j])
	    comment = &(argv[i][j]);
	  else
	    comment = argv[++i];
	  j = strlen(argv[i]) - 1;
	  break;

	case 'p':
	  protect = 1;
	  break;

	case 'C':
	  complexify = 1;
	  break;

	case 'a':
	  archive = 1;
	  break;

	case 'r':
	  rawmode = 1;
	  break;

	case 'v':
	  verbose = 1;
	  break;

	default:
	  fprintf(stderr, "%s: unknown option -%c\n", argv[0],
		  argv[i][j]);
	  fprintf(stderr, usage, argv[0]);
	  return 1;
	}
      }
    }
    else if (argv[i][0] != '-') {
      infilename = argv[i];
    }
  }


  /* Set default outfilename or vartype */

  if (!outfilename && vartype) {
    if (infilename) {
      outfilename = malloc(strlen(infilename) + strlen(vartype) + 2);
      strcpy(outfilename, infilename);
      if ((p = strrchr(outfilename, '.')))
	*p = 0;
      strcat(outfilename, ".");
      strcat(outfilename, vartype);
    }
    else {
      outfilename = malloc(strlen(vartype) + 3);
      sprintf(outfilename, "a.%s", vartype);
    }
  }
  else if (outfilename && !vartype) {
    if ((p = strrchr(outfilename, '.')))
      vartype = &(p[1]);
  }

  if (!vartype) {
    fprintf(stderr, "%s: no variable type specified.\n",
	    argv[0]);
    return 1;
  }


  if (!verbose)
    g_log_set_handler("tifiles", G_LOG_LEVEL_MASK, &log_nop, 0);
  tifiles_library_init();

  p = malloc(strlen(vartype) + 2);
  sprintf(p, "a.%s", vartype);
  model = tifiles_file_get_model(p);
  free(p);

  if (!model) {
    fprintf(stderr, "%s: invalid variable type %s\n", argv[0], vartype);
    tifiles_library_exit();
    return 1;
  }

  typeid = tifiles_fext2vartype(model, vartype);
  cp = tifiles_vartype2fext(model, typeid);
  if (!cp || !cp[0]) {
    fprintf(stderr, "%s: invalid variable type %s\n", argv[0], vartype);
    tifiles_library_exit();
    return 1;
  }

  if (protect)
    typeid = protect_type(model, typeid);
  if (complexify)
    typeid = complexify_type(model, typeid);

  if (!varname) {
    p = strrchr(outfilename, '.');
    if (p)
      *p = 0;
    if (type_name_is_tokenized(model, typeid)) {
      varname = ticonv_varname_tokenize(model, outfilename);
    }
    else {
      varname = malloc(strlen(outfilename) + 1);
      for (i = 0; outfilename[i]; i++) {
	if (outfilename[i] >= 'A' && outfilename[i] <= 'Z')
	  varname[i] = outfilename[i];
	else if (outfilename[i] >= 'a' && outfilename[i] <= 'z')
	  varname[i] = outfilename[i] + 'A' - 'a';
	else if (outfilename[i] >= '0' && outfilename[i] <= '9')
	  varname[i] = outfilename[i];
	else
	  varname[i] = '[';
      }
      varname[i] = 0;
    }
    if (p)
      *p = '.';
  }

  if (infilename) {
    infile = fopen(infilename, "rb");
    if (!infile) {
      perror(infilename);
      return 2;
    }
  }
  else {
    infilename = "(standard input)";
    infile = stdin;
  }

  data = malloc(dalloc = 1024);
  if (!rawmode && type_has_length(model, typeid))
    dsize = 2;
  else
    dsize = 0;

  i = fgetc(infile);
  while (!feof(infile) && !ferror(infile)) {
    if (dsize >= dalloc) {
      dalloc += 1024;
      data = realloc(data, dalloc);
    }
    data[dsize++] = i;
    i = fgetc(infile);
  }

  if (infile != stdin)
    fclose(infile);

  if (!rawmode && type_has_length(model, typeid)) {
    data[0] = (dsize - 2) & 0xff;
    data[1] = ((dsize - 2) >> 8) & 0xff;
  }

  fc = tifiles_content_create_regular(model);

  if (comment) {
    if (strchr(comment, '%')) {
      time(&t);
      strftime(fc->comment, 40, comment, localtime(&t));
    }
    else
      strncpy(fc->comment, comment, 40);
  }

  ve = tifiles_ve_create_with_data(dsize);
  memset(ve->folder, 0, FLDNAME_MAX);
  memset(ve->name, 0, VARNAME_MAX);
  strncpy(ve->name, varname, VARNAME_MAX);
  ve->type = typeid;
  ve->attr = archive ? ATTRB_ARCHIVED : 0;
  ve->size = dsize;
  memcpy(ve->data, data, dsize);
  free(data);

  tifiles_content_add_entry(fc, ve);

  i = err_print("tifiles_file_write_regular",
		tifiles_file_write_regular(outfilename, fc, 0));

  if (!i && verbose)
    tifiles_file_display_regular(fc);

  tifiles_content_delete_regular(fc);
  tifiles_library_exit();

  return i ? 3 : 0;
}
