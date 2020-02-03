#include <cassert>
#include <cstring>
#include "Database.h"

const Table *Row::table() const
{
    return _table;
}

const string &Row::value(const string &column) const
{
    assert(_table != NULL);
    const ColumnNames &columns = _table->columns();
    unsigned long n = columns.size();
    for (unsigned i = 0; i < n; i++) {
        if (columns.at(i) == column) {
            return at(i);
        }
    }
    throw RowException("Unknown column");
}

void Row::append(const string &value)
{
    emplace_back(value);
}

bool Row::is_intermediate_row() const
{
    return _table == NULL;
}

Row::Row(const Table *table)
        : _table(table)
{}

Row::Row()
        : _table(NULL)
{
}

Row::Row(const initializer_list<string>& values)
    : vector<string>(values),
      _table(NULL)
{}

Row::~Row()
{
    clear();
}

void Row::reclaim(Row* row)
{
    if (row && row->is_intermediate_row()) {
        delete row;
    }
}
