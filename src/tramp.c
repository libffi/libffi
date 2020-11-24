/* -----------------------------------------------------------------------
   tramp.c - Copyright (c) 2020 Madhavan T. Venkataraman

   API and support functions for managing statically defined closure
   trampolines.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <linux/limits.h>
#include <linux/types.h>
#include <fficonfig.h>

#if defined(FFI_EXEC_STATIC_TRAMP)

#if !defined(__linux__)
#error "FFI_EXEC_STATIC_TRAMP is currently only supported on Linux"
#endif

#if !defined _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/*
 * Each architecture defines static code for a trampoline code table. The
 * trampoline code table is mapped into the address space of a process.
 *
 * The following architecture specific function returns:
 *
 *	- the address of the trampoline code table in the text segment
 *	- the size of each trampoline in the trampoline code table
 *	- the size of the mapping for the whole trampoline code table
 */
void __attribute__((weak)) *ffi_tramp_arch (size_t *tramp_size,
  size_t *map_size);

/* ------------------------- Trampoline Data Structures --------------------*/

static pthread_mutex_t tramp_lock = PTHREAD_MUTEX_INITIALIZER;

struct tramp;

/*
 * Trampoline table. Manages one trampoline code table and one trampoline
 * parameter table.
 *
 * prev, next	Links in the global trampoline table list.
 * code_table	Trampoline code table mapping.
 * parm_table	Trampoline parameter table mapping.
 * array	Array of trampolines malloced.
 * free		List of free trampolines.
 * nfree	Number of free trampolines.
 */
struct tramp_table
{
  struct tramp_table *prev;
  struct tramp_table *next;
  void *code_table;
  void *parm_table;
  struct tramp *array;
  struct tramp *free;
  int nfree;
};

/*
 * Parameters for each trampoline.
 *
 * data
 *	Data for the target code that the trampoline jumps to.
 * target
 *	Target code that the trampoline jumps to.
 */
struct tramp_parm
{
  void *data;
  void *target;
};

/*
 * Trampoline structure for each trampoline.
 *
 * prev, next	Links in the trampoline free list of a trampoline table.
 * table	Trampoline table to which this trampoline belongs.
 * code		Address of this trampoline in the code table mapping.
 * parm		Address of this trampoline's parameters in the parameter
 *		table mapping.
 */
struct tramp
{
  struct tramp *prev;
  struct tramp *next;
  struct tramp_table *table;
  void *code;
  struct tramp_parm *parm;
};

/*
 * Trampoline globals.
 *
 * fd
 *	File descriptor of binary file that contains the trampoline code table.
 * offset
 *	Offset of the trampoline code table in that file.
 * map_size
 *	Size of the trampoline code table mapping.
 * size
 *	Size of one trampoline in the trampoline code table.
 * ntramp
 *	Total number of trampolines in the trampoline code table.
 * tables
 *	List of trampoline tables that contain free trampolines.
 * ntables
 *	Number of trampoline tables that contain free trampolines.
 */
struct tramp_global
{
  int fd;
  off_t offset;
  size_t map_size;
  size_t size;
  int ntramp;
  struct tramp_table *tables;
  int ntables;
};

static struct tramp_global gtramp = { -1 };

/* ------------------------ Trampoline Initialization ----------------------*/

static int ffi_tramp_get_fd_offset (void *tramp_text, off_t *offset);

/*
 * Initialize the static trampoline feature.
 */
static int
ffi_tramp_init (void)
{
  if (ffi_tramp_arch == NULL)
    return 0;

  if (gtramp.fd == -1)
    {
      void *tramp_text;

      gtramp.tables = NULL;
      gtramp.ntables = 0;

      /*
       * Get trampoline code table information from the architecture.
       */
      tramp_text = ffi_tramp_arch (&gtramp.size, &gtramp.map_size);
      gtramp.ntramp = gtramp.map_size / gtramp.size;

      /*
       * Get the binary file that contains the trampoline code table and also
       * the offset of the table within the file. These are used to mmap()
       * the trampoline code table.
       */
      gtramp.fd = ffi_tramp_get_fd_offset (tramp_text, &gtramp.offset);
    }
  return gtramp.fd != -1;
}

/*
 * From the address of the trampoline code table in the text segment, find the
 * binary file and offset of the trampoline code table from /proc/<pid>/maps.
 */
static int
ffi_tramp_get_fd_offset (void *tramp_text, off_t *offset)
{
  FILE *fp;
  char file[PATH_MAX], line[PATH_MAX+100], perm[10], dev[10];
  unsigned long start, end, inode;
  uintptr_t addr = (uintptr_t) tramp_text;
  int nfields, found;

  snprintf (file, PATH_MAX, "/proc/%d/maps", getpid());
  fp = fopen (file, "r");
  if (fp == NULL)
    return -1;

  found = 0;
  while (feof (fp) == 0) {
    if (fgets (line, sizeof (line), fp) == 0)
      break;

    nfields = sscanf (line, "%lx-%lx %s %lx %s %ld %s",
      &start, &end, perm, offset, dev, &inode, file);
    if (nfields != 7)
      continue;

    if (addr >= start && addr < end) {
      *offset += (addr - start);
      found = 1;
      break;
    }
  }
  fclose (fp);

  if (!found)
    return -1;

  return open (file, O_RDONLY);
}

/* ---------------------- Trampoline Table functions ---------------------- */

static int tramp_table_map (char **code_table, char **parm_table);
static void tramp_add (struct tramp *tramp);

/*
 * Allocate and initialize a trampoline table.
 */
static int
tramp_table_alloc (void)
{
  char *code_table, *parm_table;
  struct tramp_table *table;
  struct tramp *tramp_array, *tramp;
  size_t size;
  char *code, *parm;
  int i;

  /*
   * If we already have tables with free trampolines, there is no need to
   * allocate a new table.
   */
  if (gtramp.ntables > 0)
    return 1;

  /*
   * Allocate a new trampoline table structure.
   */
  table = malloc (sizeof (*table));
  if (table == NULL)
    return 0;

  /*
   * Allocate new trampoline structures.
   */
  tramp_array = malloc (sizeof (*tramp) * gtramp.ntramp);
  if (tramp_array == NULL)
    goto free_table;

  /*
   * Map a code table and a parameter table into the caller's address space.
   */
  if (!tramp_table_map (&code_table, &parm_table))
    goto free_tramp_array;

  /*
   * Initialize the trampoline table.
   */
  table->code_table = code_table;
  table->parm_table = parm_table;
  table->array = tramp_array;
  table->free = NULL;
  table->nfree = 0;

  /*
   * Populate the trampoline table free list. This will also add the trampoline
   * table to the global list of trampoline tables.
   */
  size = gtramp.size;
  code = code_table;
  parm = parm_table;
  for (i = 0; i < gtramp.ntramp; i++)
    {
      tramp = &tramp_array[i];
      tramp->table = table;
      tramp->code = code;
      tramp->parm = (struct tramp_parm *) parm;
      tramp_add (tramp);

      code += size;
      parm += size;
    }
  return 1;

free_tramp_array:
  free (tramp_array);
free_table:
  free (table);
  return 0;
}

/*
 * Create a trampoline code table mapping and a trampoline parameter table
 * mapping. The two mappings must be adjacent to each other for PC-relative
 * access.
 *
 * For each trampoline in the code table, there is a corresponding parameter
 * block in the parameter table. The size of the parameter block is the same
 * as the size of the trampoline. This means that the parameter block is at
 * a fixed offset from its trampoline making it easy for a trampoline to find
 * its parameters using PC-relative access.
 *
 * The parameter block will contain a struct tramp_parm. This means that
 * sizeof (struct tramp_parm) cannot exceed the size of a parameter block.
 */
static int
tramp_table_map (char **code_table, char **parm_table)
{
  char *addr;

  /*
   * Create an anonymous mapping twice the map size. The top half will be used
   * for the code table. The bottom half will be used for the parameter table.
   */
  addr = mmap (NULL, gtramp.map_size * 2, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    return 0;

  /*
   * Replace the top half of the anonymous mapping with the code table mapping.
   */
  *code_table = mmap (addr, gtramp.map_size, PROT_READ | PROT_EXEC,
    MAP_PRIVATE | MAP_FIXED, gtramp.fd, gtramp.offset);
  if (*code_table == MAP_FAILED)
    {
      (void) munmap (addr, gtramp.map_size * 2);
      return 0;
    }
  *parm_table = *code_table + gtramp.map_size;
  return 1;
}

/*
 * Free a trampoline table.
 */
static void
tramp_table_free (struct tramp_table *table)
{
  (void) munmap (table->code_table, gtramp.map_size);
  (void) munmap (table->parm_table, gtramp.map_size);
  free (table->array);
  free (table);
}

/*
 * Add a new trampoline table to the global table list.
 */
static void
tramp_table_add (struct tramp_table *table)
{
  table->next = gtramp.tables;
  table->prev = NULL;
  if (gtramp.tables != NULL)
    gtramp.tables->prev = table;
  gtramp.tables = table;
  gtramp.ntables++;
}

/*
 * Delete a trampoline table from the global table list.
 */
static void
tramp_table_del (struct tramp_table *table)
{
  gtramp.ntables--;
  if (table->prev != NULL)
    table->prev->next = table->next;
  if (table->next != NULL)
    table->next->prev = table->prev;
  if (gtramp.tables == table)
    gtramp.tables = table->next;
}

/* ------------------------- Trampoline functions ------------------------- */

/*
 * Add a trampoline to its trampoline table.
 */
static void
tramp_add (struct tramp *tramp)
{
  struct tramp_table *table = tramp->table;

  tramp->next = table->free;
  tramp->prev = NULL;
  if (table->free != NULL)
    table->free->prev = tramp;
  table->free = tramp;
  table->nfree++;

  if (table->nfree == 1)
    tramp_table_add (table);

  /*
   * We don't want to keep too many free trampoline tables lying around.
   */
  if (table->nfree == gtramp.ntramp && gtramp.ntables > 1)
    {
      tramp_table_del (table);
      tramp_table_free (table);
    }
}

/*
 * Remove a trampoline from its trampoline table.
 */
static void
tramp_del (struct tramp *tramp)
{
  struct tramp_table *table = tramp->table;

  table->nfree--;
  if (tramp->prev != NULL)
    tramp->prev->next = tramp->next;
  if (tramp->next != NULL)
    tramp->next->prev = tramp->prev;
  if (table->free == tramp)
    table->free = tramp->next;

  if (table->nfree == 0)
    tramp_table_del (table);
}

/* ------------------------ Trampoline API functions ------------------------ */

int
ffi_tramp_is_supported(void)
{
  int ret;

  pthread_mutex_lock (&tramp_lock);
  ret = ffi_tramp_init ();
  pthread_mutex_unlock (&tramp_lock);
  return ret;
}

/*
 * Allocate a trampoline and return its opaque address.
 */
void *
ffi_tramp_alloc (int flags)
{
  struct tramp *tramp;

  pthread_mutex_lock (&tramp_lock);

  if (!ffi_tramp_init () || flags != 0)
    {
      pthread_mutex_unlock (&tramp_lock);
      return NULL;
    }

  if (!tramp_table_alloc ())
    {
      pthread_mutex_unlock (&tramp_lock);
      return NULL;
    }

  tramp = gtramp.tables->free;
  tramp_del (tramp);

  pthread_mutex_unlock (&tramp_lock);

  return tramp;
}

/*
 * Set the parameters for a trampoline.
 */
void
ffi_tramp_set_parms (void *arg, void *target, void *data)
{
  struct tramp *tramp = arg;

  pthread_mutex_lock (&tramp_lock);
  tramp->parm->target = target;
  tramp->parm->data = data;
  pthread_mutex_unlock (&tramp_lock);
}

/*
 * Get the invocation address of a trampoline.
 */
void *
ffi_tramp_get_addr (void *arg)
{
  struct tramp *tramp = arg;
  void *addr;

  pthread_mutex_lock (&tramp_lock);
  addr = tramp->code;
  pthread_mutex_unlock (&tramp_lock);

  return addr;
}

/*
 * Free a trampoline.
 */
void
ffi_tramp_free (void *arg)
{
  struct tramp *tramp = arg;

  pthread_mutex_lock (&tramp_lock);
  tramp_add (tramp);
  pthread_mutex_unlock (&tramp_lock);
}

/* ------------------------------------------------------------------------- */

#else /* !FFI_EXEC_STATIC_TRAMP */

int
ffi_tramp_is_supported(void)
{
  return 0;
}

void *
ffi_tramp_alloc (int flags)
{
  return NULL;
}

void
ffi_tramp_set_parms (void *arg, void *target, void *data)
{
}

void *
ffi_tramp_get_addr (void *arg)
{
  return NULL;
}

void
ffi_tramp_free (void *arg)
{
}

#endif /* FFI_EXEC_STATIC_TRAMP */
