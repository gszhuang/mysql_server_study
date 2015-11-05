/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/types/entity_object_impl.h"

#include "dd/impl/raw/object_keys.h" // dd::Primary_id_key
#include "dd/impl/raw/raw_record.h"  // dd::Raw_new_record

namespace dd {

///////////////////////////////////////////////////////////////////////////

void Entity_object_impl::set_primary_key_value(const Raw_new_record &r)
{
  /*
    Delay updating of the m_has_new_primary_key flag until end of store()
    method. This is necessary for children's store() methods to know that
    that parent entity has new ID which was not used before (and hence
    children primary keys based on this ID will be new too).
  */
  m_id= r.get_insert_id();
}

///////////////////////////////////////////////////////////////////////////

Object_key *Entity_object_impl::create_primary_key() const
{
  return new (std::nothrow) Primary_id_key(id());
}

///////////////////////////////////////////////////////////////////////////

void Entity_object_impl::restore_id(const Raw_record &r, int field_idx)
{
  m_id= r.read_int(field_idx);
  fix_has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////

void Entity_object_impl::restore_name(const Raw_record &r, int field_idx)
{
  m_name= r.read_str(field_idx);
}
///////////////////////////////////////////////////////////////////////////

bool Entity_object_impl::store_id(Raw_record *r, int field_idx)
{
  return r->store_pk_id(field_idx, m_id);
}

///////////////////////////////////////////////////////////////////////////

bool Entity_object_impl::store_name(Raw_record *r, int field_idx)
{
  return r->store(field_idx, m_name);
}

///////////////////////////////////////////////////////////////////////////

}