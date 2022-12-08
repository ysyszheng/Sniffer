#ifndef FILTER_H
#define FILTER_H

#include "/utils/utils.h"
#include "sniffer.h"
#include <QTableView>
#include <regex>
#include <map>
#define P 0       
#define S 1
#define D 2
#define SPORT 3
#define DPORT 4
#define C 5

class Filter{
public:
    Filter();
    ~Filter();
    bool checkCommand(QString command);
    bool loadCommand(QString command);
    void launchFilter()
};

