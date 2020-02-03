#pragma once

class Row;

class Iterator
{
public:
    virtual unsigned n_columns() = 0;
    virtual void open() = 0;
    virtual Row* next() = 0;
    virtual void close() = 0;
    virtual ~Iterator() {}
};
