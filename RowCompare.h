#pragma once

#include <vector>

class Row;

using namespace std;

class RowCompare
{
public:
    int operator()(Row* const &x, Row* const &y);
    bool cmp(Row* const &x, Row* const &y);
    RowCompare(const vector<unsigned>& sort_columns);

private:
    vector<unsigned> _sort_columns;
};
