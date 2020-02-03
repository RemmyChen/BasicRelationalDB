#include <cassert>
#include <algorithm>
#include "QueryProcessor.h"
#include "Table.h"
#include "Index.h"
#include "Iterator.h"
#include "Row.h"
#include "RowCompare.h"
#include "ColumnSelector.h"
#include "Operators.h"
#include "util.h"

//----------------------------------------------------------------------

// TableIterator 

unsigned TableIterator::n_columns() 
{
    return _table->columns().size();
}

void TableIterator::open() 
{
    _input = _table->rows().begin();
    _end = _table->rows().end();
    
}

Row* TableIterator::next() 
{
    Row* next = NULL;
    if (_input != _end) {
        next = *(_input++);
    }
    return next;
}   

void TableIterator::close() 
{
    _input = _end;
}

TableIterator::TableIterator(Table* table)
    : _table(table)
{
}

//----------------------------------------------------------------------

// IndexScan

unsigned IndexScan::n_columns()
{
    return _index->n_columns();
}

void IndexScan::open()
{
    _input = _index->lower_bound(*_lo);
    _end = _index->upper_bound(*_hi);
}


Row* IndexScan::next()
{
    Row* next = NULL;
    if (_input != _end) {
        next = (_input++)->second;
    }
    return next;
}

void IndexScan::close()
{
    _input = _end;
}

IndexScan::IndexScan(Index* index, Row* lo, Row* hi)
    : _index(index),
      _lo(lo),
      _hi(hi == NULL ? lo : hi)
{}

//----------------------------------------------------------------------

// Select

unsigned Select::n_columns()
{
    return _input->n_columns();
}

void Select::open()
{
    _input->open();
}

Row* Select::next()
{
    Row* next = _input->next();
    while (next != NULL && !_predicate(next)) {
        Row::reclaim(next);
        next = _input->next();
    }
    return next;
}

void Select::close()
{
    _input->close();
}

Select::Select(Iterator* input, RowPredicate predicate)
    : _input(input),
      _predicate(predicate)
{
}

Select::~Select()
{
    delete _input;
}

//----------------------------------------------------------------------

// Project

unsigned Project::n_columns()
{
    return _column_selector.n_selected();
}

void Project::open()
{
    _input->open();
}

Row* Project::next()
{
    Row* projected = NULL;
    Row* row = _input->next();
    if (row) {
        projected = new Row();
        for (unsigned i = 0; i < _column_selector.n_selected(); i++) {
            projected->append(row->at(_column_selector.selected(i)));
        }
        Row::reclaim(row);
    }
    return projected;
}

void Project::close()
{
    _input->close();
}

Project::Project(Iterator* input, const initializer_list<unsigned>& columns)
    : _input(input),
      _column_selector(input->n_columns(), columns)
{}

Project::~Project()
{
    delete _input;
}

//----------------------------------------------------------------------

// NestedLoopsJoin

unsigned NestedLoopsJoin::n_columns()
{
    return _left_join_columns.n_columns() + _right_join_columns.n_unselected();
}

void NestedLoopsJoin::open()
{
    _left->open();
    _right->open();
    _left_row = _left->next();
}

Row* NestedLoopsJoin::next()
{
    Row* right_row = _right->next();
    while (_left_row != NULL && right_row != NULL && match(_left_row, right_row) == false) {
        Row::reclaim(_left_row);
        _left_row = _left->next();
        if (_left_row == NULL) {
            Row::reclaim(right_row);
            right_row = _right->next();
            if (right_row != NULL) {
                _left->close();
                _left->open();
                _left_row = _left->next();
            }
        }
    }

    Row* next = NULL;
    if (_left_row != NULL && right_row != NULL) {
        next = join_rows(_left_row, right_row);
    }
    if (right_row != NULL) {
        Row::reclaim(right_row);
    }
    return next;
}


Row* NestedLoopsJoin::join_rows(const Row* left, const Row* right)
{
    Row* joined = new Row();
    unsigned lcols = _left_join_columns.n_columns();
    unsigned rcols = _right_join_columns.n_unselected();
    for (unsigned i = 0; i < lcols; i++) {
        joined->append(left->at(i));
    }
    for (unsigned i = 0; i < rcols; i++) {
        joined->append(right->at(_right_join_columns.unselected(i)));
    }
    return joined;
}

bool NestedLoopsJoin::match(const Row* left, const Row* right)
{
    unsigned cols = _left_join_columns.n_selected();
    for (unsigned i = 0, j = 0; i < cols; i++, j++) {
        if (left->at(_left_join_columns.selected(i)) != right->at(_right_join_columns.selected(i))) {
            return false;
        }
    }
    return true;
}


void NestedLoopsJoin::close()
{
    _left->close();
    _right->close();
    Row::reclaim(_left_row);
}

NestedLoopsJoin::NestedLoopsJoin(Iterator* left,
                                 const initializer_list<unsigned>& left_join_columns,
                                 Iterator* right,
                                 const initializer_list<unsigned>& right_join_columns)
    : _left(left),
      _right(right),
      _left_join_columns(left->n_columns(), left_join_columns),
      _right_join_columns(right->n_columns(), right_join_columns),
      _left_row(NULL)
{
    assert(_left_join_columns.n_selected() == _right_join_columns.n_selected());
}

NestedLoopsJoin::~NestedLoopsJoin()
{
    delete _left;
    delete _right;
}

//----------------------------------------------------------------------

// Sort

unsigned Sort::n_columns() 
{
    return _input->n_columns();
}

void Sort::open() 
{
    _input->open();
    Row *row;
    while ((row = _input->next()) != NULL) {
        _sorted.emplace_back(row);
    }
    std::sort(_sorted.begin(), _sorted.end(), RowCompare(_sort_columns));
    _sorted_iterator = _sorted.begin();
}

Row* Sort::next() 
{
    Row* next = NULL;
    if (_sorted_iterator != _sorted.end()) {
        next = *(_sorted_iterator++);
    }
    return next;
}

void Sort::close() 
{
    _input->close();
    _sorted.clear();
}

Sort::Sort(Iterator* input, const initializer_list<unsigned>& sort_columns)
    : _input(input),
      _sort_columns(sort_columns)
{}

Sort::~Sort()
{
    delete _input;
}

//----------------------------------------------------------------------

// Unique

unsigned Unique::n_columns()
{
    return _input->n_columns();
}

void Unique::open() 
{
    _input->open();
    _next_unique = new Row();
}

Row* Unique::next()
{
    Row* next = NULL;
    while ((next = _input->next()) != NULL) {
        if ((*next) != (*_next_unique)) {
            *_next_unique = *next;
            break;
        } else {
            Row::reclaim(next);
        }
    }
    return next;
}

void Unique::close() 
{
    _input->close();
    delete _next_unique;
}

Unique::Unique(Iterator* input)
    : _input(input),
      _next_unique(NULL)
{}

Unique::~Unique()
{
    delete _input;
}
