#include <string.h>
#include "Row.h"
#include "RowCompare.h"

RowCompare::RowCompare(const vector<unsigned>& sort_columns)
    : _sort_columns(sort_columns)
{}

int RowCompare::operator()(Row* const &x, Row* const &y)
{
    unsigned n = (unsigned) x->size();
    for (unsigned i = 0; i < n; i++) {
        unsigned j = _sort_columns.at(i);
        int comparison = strcmp(x->at(j).c_str(), y->at(j).c_str());
        if (comparison != 0) {
            return comparison < 0;
        }
    }
    return 0;
}

bool RowCompare::cmp(Row* const &x, Row* const &y)
{
    unsigned n = (unsigned) x->size();
    for (unsigned i = 0; i < n; i++) {
        unsigned j = _sort_columns.at(i);
        int comparison = strcmp(x->at(j).c_str(), y->at(j).c_str());
        if (comparison < 0) {
            return true;
        }
    }
    return false;
}
