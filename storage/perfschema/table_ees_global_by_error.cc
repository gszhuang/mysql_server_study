/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_ees_global_by_error.cc
  Table EVENTS_ERRORS_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_ees_global_by_error.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_timer.h"
#include "pfs_visitor.h"
#include "field.h"

THR_LOCK table_ees_global_by_error::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("ERROR_NUMBER") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ERROR_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SQL_STATE") },
    { C_STRING_WITH_LEN("varchar(5)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ERROR_RAISED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ERROR_HANDLED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("FIRST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LAST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_ees_global_by_error::m_field_def=
{ 7, field_types };

PFS_engine_table_share
table_ees_global_by_error::m_share=
{
  { C_STRING_WITH_LEN("events_errors_summary_global_by_error") },
  &pfs_truncatable_acl,
  table_ees_global_by_error::create,
  NULL, /* write_row */
  table_ees_global_by_error::delete_all_rows,
  table_ees_global_by_error::get_row_count,
  sizeof(pos_ees_global_by_error),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table*
table_ees_global_by_error::create(void)
{
  return new table_ees_global_by_error();
}

int
table_ees_global_by_error::delete_all_rows(void)
{
  reset_events_errors_by_thread();
  reset_events_errors_by_account();
  reset_events_errors_by_user();
  reset_events_errors_by_host();
  reset_events_errors_global();
  return 0;
}

ha_rows
table_ees_global_by_error::get_row_count(void)
{
  return error_class_max * max_server_errors;
}

table_ees_global_by_error::table_ees_global_by_error()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_ees_global_by_error::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_ees_global_by_error::rnd_init(bool scan)
{
  return 0;
}

int table_ees_global_by_error::rnd_next(void)
{
  PFS_error_class *error_class;

  m_pos.set_at(&m_next_pos);

  error_class= find_error_class(m_pos.m_index_1);
  if (error_class)
  {
    for ( ;
         m_pos.has_more_error();
         m_pos.next_error())
    {
        make_row(error_class, m_pos.m_index_2);
        m_next_pos.set_after(&m_pos);
        return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ees_global_by_error::rnd_pos(const void *pos)
{
  PFS_error_class *error_class;

  set_position(pos);

  error_class=find_error_class(m_pos.m_index_1);
  if (error_class)
  {
    for ( ;
         m_pos.has_more_error();
         m_pos.next_error())
    {
      make_row(error_class, m_pos.m_index_2);
      return 0;
    }
  }

  return HA_ERR_RECORD_DELETED;
}


void table_ees_global_by_error
::make_row(PFS_error_class *klass, int error_index)
{
  m_row_exists= false;

  PFS_connection_error_visitor visitor(klass, error_index);
  PFS_connection_iterator::visit_global(true,  /* hosts */
                                        false, /* users */
                                        true,  /* accounts */
                                        true,  /* threads */
                                        false, /* THDs */
                                        & visitor);

  m_row_exists= true;

  m_row.m_stat.set(& visitor.m_stat, error_index);
}

int table_ees_global_by_error
::read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                  bool read_all)
{
  Field *f;
  server_error *temp_error= NULL;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  if (m_row.m_stat.m_error_index > 0 && m_row.m_stat.m_error_index < PFS_MAX_SERVER_ERRORS)
    temp_error= & error_names_array[pfs_to_server_error_map[m_row.m_stat.m_error_index]];

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* ERROR NUMBER */
      case 1: /* ERROR NAME */
      case 2: /* SQLSTATE */
      case 3: /* SUM_ERROR_RAISED */
      case 4: /* SUM_ERROR_HANDLED */
      case 5: /* FIRST_SEEN */
      case 6: /* LAST_SEEN */
        /** ERROR STATS */
        m_row.m_stat.set_field(f->field_index, f, temp_error);
        break;
      default:
        /** We should never reach here */
        DBUG_ASSERT(0);
        break;
      }
    }
  }

  return 0;
}
